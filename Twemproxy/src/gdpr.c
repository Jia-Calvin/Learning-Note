#include <chromiumbase64.h>
#include <nc_core.h>
#include <gdpr.h>
#include <time.h>
#include <nc_util.h>
#include <nc_queue.h>
#include <nc_conf.h>
#include <openssl/buffer.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

static gdpr_auth_msg_t *gdpr_auth_downgrade;
static gdpr_auth_msg_t *gdpr_auth_argument_size;
static gdpr_auth_msg_t *gdpr_auth_token_format;
static gdpr_auth_msg_t *gdpr_auth_psm_not_found;
static gdpr_auth_msg_t *gdpr_auth_authority_not_found;
static gdpr_auth_msg_t *gdpr_auth_rsa_verify;
static gdpr_auth_msg_t *gdpr_auth_ip_not_found;
static gdpr_auth_msg_t *gdpr_auth_expire_time;
static gdpr_auth_msg_t *gdpr_auth_token_corruption;
static gdpr_auth_msg_t *gdpr_auth_argument_protocol;
static gdpr_auth_msg_t *gdpr_auth_password;
static gdpr_auth_msg_t *gdpr_auth_succ;

// jwt encoding format:
// "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9".<payload>.<rsa256 signature>
static const char dps_header[] = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9";
static const size_t dps_header_len = sizeof(dps_header) - 1;
// rsa256: 256 / 3 * 4
static const size_t rsa256_sign_base64_len = 342;

// It's safe to use a global variable for nc's single threaded model
static dps_token_t *global_token;

#define ADDR_COUNT 64
#define ADDR_SIZE 32

typedef struct {
	char addr[ADDR_SIZE];
} addr_t;

static struct {
	unsigned access_success_count;
	unsigned access_deny_count;
	unsigned access_downgrade_count;
	unsigned access_other_count;
	struct {
		addr_t addr[ADDR_COUNT];
		unsigned cur_pos;
	} addr_mgr;
} gdpr_info;

static inline void *nc_alloc_wrapper(size_t s) { return nc_alloc(s); }
static inline void nc_free_wrapper(void *p) { nc_free(p); }

#define addr_mgr_next_pos(n) ((n + 1) & (ADDR_COUNT - 1))

static inline void addr_mgr_add(const char *addr, char state) {
	char *p = NULL;

	ASSERT(ADDR_COUNT & (ADDR_COUNT - 1) == 0);
	p = gdpr_info.addr_mgr.addr[gdpr_info.addr_mgr.cur_pos].addr;
	p[0] = state;
	p[1] = ':';
	strncpy(p + 2, addr, ADDR_SIZE - 2);
	gdpr_info.addr_mgr.cur_pos = addr_mgr_next_pos(gdpr_info.addr_mgr.cur_pos);
}

static inline unsigned addr_mgr_count(void) {
	unsigned i = 0, count = 0;
	for (i = addr_mgr_next_pos(gdpr_info.addr_mgr.cur_pos);
		 i != gdpr_info.addr_mgr.cur_pos; i = addr_mgr_next_pos(i)) {
		if (gdpr_info.addr_mgr.addr[i].addr[0] != '\0') {
			count++;
		}
	}
	return count;
}

__attribute__((constructor)) static void init(void) {
	static dps_token_t local_token;
	cJSON_Hooks hooks;

	global_token = &local_token;
	gdpr_init_dps_token(global_token);
#define INIT_GDPR_AUTH_MSG(name, code, msg) \
	{                                       \
		static gdpr_auth_msg_t obj;         \
		obj.result = code;                  \
		obj.reason.data = (uint8_t *)msg;   \
		obj.reason.len = strlen(msg);       \
		name = &obj;                        \
	}

	INIT_GDPR_AUTH_MSG(gdpr_auth_downgrade, GDPR_AUTH_DOWNGRADE, "+OK\r\n");
	INIT_GDPR_AUTH_MSG(gdpr_auth_argument_size, GDPR_AUTH_FAIL,
					   "-ERR Argument Size Error\r\n");
	INIT_GDPR_AUTH_MSG(gdpr_auth_argument_protocol, GDPR_AUTH_FAIL,
					   "-ERR Dps Command Protocol\r\n");
	INIT_GDPR_AUTH_MSG(gdpr_auth_token_format, GDPR_AUTH_FAIL,
					   "-ERR Dps Token Format\r\n");
	INIT_GDPR_AUTH_MSG(gdpr_auth_psm_not_found, GDPR_AUTH_FAIL,
					   "-ERR Psm Not Found\r\n");
	INIT_GDPR_AUTH_MSG(gdpr_auth_authority_not_found, GDPR_AUTH_FAIL,
					   "-ERR Authority Not Found\r\n");
	INIT_GDPR_AUTH_MSG(gdpr_auth_rsa_verify, GDPR_AUTH_FAIL,
					   "-ERR RSA Digest Verify\r\n");
	INIT_GDPR_AUTH_MSG(gdpr_auth_ip_not_found, GDPR_AUTH_FAIL,
					   "-ERR Peer Ip not found\r\n");
	INIT_GDPR_AUTH_MSG(gdpr_auth_expire_time, GDPR_AUTH_FAIL,
					   "-ERR Expire Time\r\n");
	INIT_GDPR_AUTH_MSG(gdpr_auth_token_corruption, GDPR_AUTH_FAIL,
					   "-ERR Token Corruption\r\n");
	INIT_GDPR_AUTH_MSG(gdpr_auth_password, GDPR_AUTH_FAIL,
					   "-ERR Password Mismatch\r\n");
	INIT_GDPR_AUTH_MSG(gdpr_auth_succ, GDPR_AUTH_SUCC, "+OK\r\n");
#undef INIT_GDPR_AUTH_MSG

	hooks.free_fn = nc_free_wrapper;
	hooks.malloc_fn = nc_alloc_wrapper;
	cJSON_InitHooks(&hooks);
}

__attribute__((always_inline)) static inline uint32_t mem_distance(
	const void *right, const void *left) {
	ASSERT(right >= left);
	return (uint32_t)((char *)right - (char *)left);
}

__attribute__((always_inline)) static inline void gdpr_set_string(
	struct string *str, void *d, uint32_t len) {
	str->data = d;
	str->len = len;
}

// NOTE: Suppose key is well-formed
static inline int preprocess_public_key(struct string *str,
										const uint8_t *key_pem, uint32_t size) {
	const uint8_t *start, *end, *next;
	uint8_t *dest;
	str->data = nc_alloc(size);
	if (str->data == NULL) return NC_ENOMEM;
	for (start = key_pem, end = key_pem + size, dest = str->data; start < end;
		 start = next + 2) {
		next = nc_strchr(start, end, '\\');
		if (next != NULL) {
			nc_memcpy(dest, start, mem_distance(next, start));
			dest += mem_distance(next, start);
			*dest++ = '\n';
		} else {
			if (end > start) {
				nc_memcpy(dest, start, mem_distance(end, start));
				dest += mem_distance(end, start);
				*dest++ = '\0';
			}
			break;
		}
	}
	str->len = mem_distance(dest, str->data);
	return NC_OK;
}

int gdpr_init_key_context(gdpr_key_context_t *ctx, const uint8_t *old_style_key,
						  uint32_t old_style_size) {
	struct string convert_key;
	int result = NC_ERROR;
	ctx->buf_key = NULL;
	ctx->key = NULL;

	string_init(&convert_key);
	if (preprocess_public_key(&convert_key, old_style_key, old_style_size) !=
		NC_OK) {
		return NC_ERROR;
	}

	if ((ctx->ctx = EVP_MD_CTX_create()) == NULL) {
		log_debug(LOG_VVERB, "Fail to create rsa ctx");
		goto done;
	}
	if ((ctx->buf_key =
			 BIO_new_mem_buf(convert_key.data, (int)convert_key.len)) == NULL) {
		log_debug(LOG_VVERB, "Fail to alloc BIO");
		goto done;
	}
	if ((ctx->key = PEM_read_bio_PUBKEY(ctx->buf_key, NULL, NULL, NULL)) ==
		NULL) {
		log_debug(LOG_VVERB, "Fail to read key");
		goto done;
	}
	if (EVP_PKEY_id(ctx->key) != EVP_PKEY_RSA) {
		log_debug(LOG_VVERB, "Key type mismatch");
		goto done;
	}
	result = NC_OK;
done:
	string_deinit(&convert_key);
	return result;
}

int gdpr_verify_digest(const gdpr_key_context_t *gdpr_ctx,
					   const struct string *text, const struct string *sig) {
	/* Initialize the DigestVerify operation using alg */
	if (EVP_DigestVerifyInit(gdpr_ctx->ctx, NULL, EVP_sha256(), NULL,
							 gdpr_ctx->key) != 1) {
		log_debug(LOG_VVERB, "Fail to inject key into ctx:%p", gdpr_ctx->key);
		return NC_ERROR;
	}

	/* Call update with the message */
	log_debug(LOG_VVERB, "RSA Text:%.*s", text->len, text->data);
	if (EVP_DigestVerifyUpdate(gdpr_ctx->ctx, text->data, text->len) != 1) {
		return NC_ERROR;
	}

	/* Now check the sig for validity. */
	if (EVP_DigestVerifyFinal(gdpr_ctx->ctx, sig->data, sig->len) != 1) {
		return NC_ERROR;
	}
	return NC_OK;
}

void gdpr_deinit_key_context(gdpr_key_context_t *gdpr_ctx) {
	if (gdpr_ctx == NULL) {
		return;
	}
	if (gdpr_ctx->ctx) {
		EVP_MD_CTX_destroy(gdpr_ctx->ctx);
		gdpr_ctx->ctx = NULL;
	}
	if (gdpr_ctx->key) {
		EVP_PKEY_free(gdpr_ctx->key);
		gdpr_ctx->key = NULL;
	}
	if (gdpr_ctx->buf_key) {
		BIO_free(gdpr_ctx->buf_key);
		gdpr_ctx->buf_key = NULL;
	}
}

static inline int is_fast_split_header(const struct string *hdr) {
	return hdr->data == (uint8_t *)&dps_header[0];
}

static inline int fast_split_token_string(const struct string *encoded_str,
										  struct string *header,
										  struct string *payload,
										  struct string *digest,
										  struct string *text) {
	if (encoded_str->len > MAX(dps_header_len, rsa256_sign_base64_len) &&
		encoded_str->data[dps_header_len] == '.' &&
		encoded_str->data[encoded_str->len - (rsa256_sign_base64_len + 1)] ==
			'.' &&
		strncmp((char *)encoded_str->data, dps_header, dps_header_len) == 0 &&
		encoded_str->len > (dps_header_len + rsa256_sign_base64_len + 5)) {
		gdpr_set_string(header, (uint8_t *)&dps_header[0], dps_header_len);
		gdpr_set_string(
			payload, encoded_str->data + (dps_header_len + 1),
			encoded_str->len - (dps_header_len + rsa256_sign_base64_len + 2));
		gdpr_set_string(
			digest,
			encoded_str->data + (encoded_str->len - rsa256_sign_base64_len),
			rsa256_sign_base64_len);
		gdpr_set_string(text, encoded_str->data,
						dps_header_len + 1 + payload->len);
		return NC_OK;
	}
	return NC_ERROR;
}

static inline int split_token_string(const struct string *encoded_str,
									 struct string *header,
									 struct string *payload,
									 struct string *digest,
									 struct string *text) {
	const uint8_t *next, *end;
	if (fast_split_token_string(encoded_str, header, payload, digest, text) ==
		NC_OK) {
		log_debug(LOG_VVERB, "Fast Split Success");
		return 3;
	}

	log_debug(LOG_WARN, "Fast Split Fail");
	// split header
	end = encoded_str->data + encoded_str->len;
	if ((next = nc_strchr(encoded_str->data, end, '.')) == NULL) {
		log_debug(LOG_VVERB, "Token found zero dot");
		return 0;
	}
	gdpr_set_string(header, encoded_str->data,
					mem_distance(next, encoded_str->data));

	// split payload
	payload->data = (uint8_t *)next + 1;
	if ((next = nc_strchr(payload->data, end, '.')) == NULL) {
		log_debug(LOG_VVERB, "Token found one dot");
		return 1;
	}
	payload->len = mem_distance(next, payload->data);

	gdpr_set_string(text, encoded_str->data,
					mem_distance(next, encoded_str->data));
	// remain digest
	digest->data = (uint8_t *)next + 1;
	if (digest->data >= end) {
		log_debug(LOG_VVERB, "Token third part empty");
		return 2;
	}
	digest->len = mem_distance(end, digest->data);
	return 3;
}

static inline int reserve_decode_capacity(struct string *buffer,
										  uint32_t now_len) {
	unsigned need_len = chromium_base64_encode_len(now_len);
	if (need_len > buffer->len) {
		uint8_t *new = nc_realloc(buffer->data, need_len);
		if (new == NULL) {
			return NC_ERROR;
		}
		gdpr_set_string(buffer, new, need_len);
	}
	return NC_OK;
}

static inline int gdpr_base64_decode(struct string *dest,
									 const struct string *src) {
	size_t size;
	if (reserve_decode_capacity(dest, src->len) == NC_ERROR) {
		return NC_ERROR;
	}
	if ((size = chromium_urlbase64_decode((char *)dest->data, (char *)src->data,
										  src->len)) == MODP_B64_ERROR) {
		return NC_ERROR;
	}
	dest->len = (uint32_t)size;
	dest->data[size] = 0;
	return NC_OK;
}

int gdpr_parse_dps_token(dps_token_t *token, const struct string *encoded_str) {
	struct string header, payload, digest;

	if (split_token_string(encoded_str, &header, &payload, &digest,
						   &token->text) != 3) {
		return NC_ERROR;
	}

	if (is_fast_split_header(&header)) {
		token->header_json = NULL;
	} else {
		if (gdpr_base64_decode(&token->header, &header) != NC_OK) {
			return NC_ERROR;
		}
		token->header_json = cJSON_Parse((char *)token->header.data);
		if (token->header_json == NULL) {
			return NC_ERROR;
		}
	}
	if (gdpr_base64_decode(&token->payload, &payload) != NC_OK ||
		gdpr_base64_decode(&token->digest, &digest) != NC_OK) {
		return NC_ERROR;
	}
	token->payload_json = cJSON_Parse((char *)token->payload.data);
	return token->payload_json ? NC_OK : NC_ERROR;
}

static inline void __gdpr_clean_dps_token(dps_token_t *token) {
	token->header_json = NULL;
	token->payload_json = NULL;
	token->version = NULL;
	token->authority = NULL;
	token->authorityChain = NULL;
	token->podName = NULL;
	token->primaryAuthType = NULL;
	token->clientIp = NULL;
	token->psm = NULL;
	token->user = NULL;
	token->resource = NULL;
	token->expireTime = 0;
	token->alg = NULL;
	token->typ = NULL;
}

#define get_token_int(token, part, member, default_value)                \
	({                                                                   \
		token->member == NULL                                            \
			? token->member =                                            \
				  cJSON_GetObjectItemCaseSensitive(token->part, #member) \
			: NULL;                                                      \
		cJSON_IsNumber(token->member) ? token->member->valueint          \
									  : default_value;                   \
	})

#define get_token_string(token, part, member)                              \
	({                                                                     \
		token->member == NULL                                              \
			? token->member =                                              \
				  cJSON_GetObjectItemCaseSensitive(token->part, #member)   \
			: NULL;                                                        \
		cJSON_IsString(token->member) ? token->member->valuestring : NULL; \
	})

void gdpr_clean_dps_token(dps_token_t *token) {
	if (token->header_json) {
		cJSON_Delete(token->header_json);
	}
	if (token->payload_json) {
		cJSON_Delete(token->payload_json);
	}
	__gdpr_clean_dps_token(token);
}

void gdpr_init_dps_token(dps_token_t *token) {
	memset(token, 0, sizeof(*token));
}

int gdpr_init_authority_context(gdpr_authority_context_t *ctx) {
	return array_init(&ctx->keyset, 4, sizeof(gdpr_public_key_t));
}

void gdpr_deinit_authority_context(gdpr_authority_context_t *ctx) {
	while (array_n(&ctx->keyset) > 0) {
		gdpr_public_key_t *key = array_pop(&ctx->keyset);
		string_deinit(&key->name);
		gdpr_deinit_key_context(&key->key_context);
	}
	array_deinit(&ctx->keyset);
}

int gdpr_init_acl_context(gdpr_acl_context_t *ctx) {
	return array_init(&ctx->grant_services, 8, sizeof(gdpr_grant_service_t));
}

void gdpr_deinit_acl_context(gdpr_acl_context_t *ctx) {
	while (array_n(&ctx->grant_services) > 0) {
		gdpr_grant_service_t *psm = array_pop(&ctx->grant_services);
		string_deinit(&psm->grant_psm);
	}
	array_deinit(&ctx->grant_services);
}

static inline gdpr_public_key_t *gdpr_find_in_authority_context(
	const gdpr_authority_context_t *ctx, const char *name, int version) {
	unsigned i;
	struct array *data = (struct array *)&ctx->keyset;
	for (i = 0; i < array_n(data); ++i) {
		gdpr_public_key_t *key = array_get(data, i);
		if (version == key->version &&
			strncasecmp((char *)key->name.data, name, key->name.len) == 0) {
			return key;
		}
	}
	return NULL;
}

static inline gdpr_grant_service_t *gdpr_find_in_acl_context(
	const gdpr_acl_context_t *ctx, const char *psm) {
	unsigned i;
	struct array *data = (struct array *)&ctx->grant_services;
	for (i = 0; i < array_n(data); ++i) {
		gdpr_grant_service_t *svr = array_get(data, i);
		if (strncmp((char *)svr->grant_psm.data, psm, svr->grant_psm.len) ==
			0) {
			return svr;
		}
	}
	return NULL;
}

static inline int ip_len(const char *ip) {
	const char *s = ip;
	const char *e = ip + 15;
	while (*s && *s != ':' && s < e) s++;
	return s - ip;
}

static const gdpr_auth_msg_t *__gdpr_auth_check(const struct conf *conf,
												const struct string *token_part,
												const char *peer_addr) {
	const struct conf_gdpr *gdpr = &conf->gdpr;
	const char *psm, *authority, *ip, *alg, *type;
	const gdpr_acl_context_t *acl_context = &gdpr->acl_context;
	const gdpr_authority_context_t *authority_context =
		&gdpr->authority_context;
	int key_version, expire_time;
	gdpr_public_key_t *key = NULL;

	gdpr_clean_dps_token(global_token);

	if (gdpr_parse_dps_token(global_token, token_part) != NC_OK) {
		return gdpr_auth_token_format;
	}

	psm = get_token_string(global_token, payload_json, psm);
	authority = get_token_string(global_token, payload_json, authority);
	key_version = get_token_int(global_token, payload_json, version, -1);
	ip = get_token_string(global_token, payload_json, clientIp);
	expire_time = get_token_int(global_token, payload_json, expireTime, 0);
	if (psm == NULL || authority == NULL || key_version == -1) {
		return gdpr_auth_token_corruption;
	}
	if (global_token->header_json != NULL) {
		alg = get_token_string(global_token, header_json, alg);
		type = get_token_string(global_token, header_json, typ);
		if (alg == NULL || type == NULL || strncmp(alg, "RS256", 5) != 0 ||
			strncmp(type, "JWT", 3) != 0) {
			return gdpr_auth_token_corruption;
		}
	}
	if (gdpr_find_in_acl_context(acl_context, psm) == NULL) {
		return gdpr_auth_psm_not_found;
	}
	key = gdpr_find_in_authority_context(authority_context, authority,
										 key_version);
	if (key == NULL) {
		return gdpr_auth_authority_not_found;
	}
	if (ip != NULL && *ip != '\0' && peer_addr != NULL &&
		strncmp(ip, peer_addr, ip_len(peer_addr)) != 0) {
		return gdpr_auth_ip_not_found;
	}
	if (gdpr_verify_digest(&key->key_context, &global_token->text,
						   &global_token->digest) != NC_OK) {
		return gdpr_auth_rsa_verify;
	}
	if (expire_time < time(NULL) - 60 * 60 * 3) {
		return gdpr_auth_expire_time;
	}
	return gdpr_auth_succ;
}

static inline int split_msg(const struct msg *msg, char *gdpr_switch,
							struct string *token) {
	struct mbuf *b = NULL;
	// FIXME: Only parse the first mbuf block. i.e. return error if size of
	// auth msg block is larger than 16kb.
	b = STAILQ_LAST(&msg->mhdr, mbuf, next);
	if (b == NULL || b->magic != MBUF_MAGIC || msg->key_start < b->start ||
		msg->key_start > b->end || msg->key_end < b->start ||
		msg->key_end > b->end) {
		return NC_ERROR;
	}
	uint32_t len = mem_distance(msg->key_end, msg->key_start);
	if (len == 0) return NC_ERROR;
	*gdpr_switch = msg->key_start[0];
	if (*gdpr_switch != '0' && *gdpr_switch != '1' && *gdpr_switch != '2') {
		return NC_ERROR;
	}

	if (msg->narg == 2) {
		// get from key part
		token->data = msg->key_start + 1;
		token->len = len - 1;
	} else if (msg->narg == 3) {
		// Now msg->key_end point to '\r' after the last key char
		// It's safe to expect well-formed msg specification
		// Skip one '\r' and reach token part
		const uint8_t *next = nc_strchr(msg->key_end + 1, b->end, '\r');
		if (next == NULL) {
			// FIXME: size of msg is larger than mbuf
			return NC_ERROR;
		}
		token->data = (uint8_t *)next + 2;
		if (token->data >= b->end) return NC_ERROR;
		next = nc_strchr(token->data, b->end, '\r');
		if (next == NULL) return NC_ERROR;
		token->len = mem_distance(next, token->data);
	} else {
		return NC_ERROR;
	}
	return NC_OK;
}

static inline const char *itoa(int v) {
	static char num_buf[64] = {'\0'};
	snprintf(num_buf, sizeof(num_buf), "%d", v);
	return num_buf;
}

static inline int pack_bulk_string(char **p, int *rbytes, const char *value) {
	int nlen = 0;
	char *buf = *p;

	nlen =
		nc_snprintf(buf, *rbytes, "$%d\r\n%s\r\n", (int)strlen(value), value);
	if (nlen <= 0 || nlen >= *rbytes) return NC_ERROR;
	*rbytes -= nlen;
	*p += nlen;
	return NC_OK;
}

static inline int pack_bulk_number(char **p, int *rbytes, int value) {
	return pack_bulk_string(p, rbytes, itoa(value));
}

static inline int pack_bulk_array(char **p, int *rbytes, int value) {
	int nlen = 0;
	char *buf = *p;
	nlen = snprintf(buf, *rbytes, "*%d\r\n", value);
	if (nlen <= 0 || nlen >= *rbytes) return NC_ERROR;
	*rbytes -= nlen;
	*p += nlen;
	return NC_OK;
}

struct string *gdpr_info_raw_response(const void *d) {
	static struct string raw_response;
	const struct conf_gdpr *cf = (struct conf_gdpr *)d;
	int rbytes = 0;
	char *p = 0;
	unsigned addr_count;

	string_deinit(&raw_response);
	addr_count = addr_mgr_count();
	rbytes = addr_count * (ADDR_SIZE + 20) + 256;
	p = nc_alloc(rbytes);
	if (p == NULL) {
		return gdpr_require_auth_raw_response();
	}
	gdpr_set_string(&raw_response, p, rbytes);

	if (pack_bulk_array(&p, &rbytes, addr_count + 10 + 1) == NC_OK &&
		pack_bulk_string(&p, &rbytes, "mode") == NC_OK &&
		pack_bulk_number(&p, &rbytes, cf->mode) == NC_OK &&
		pack_bulk_string(&p, &rbytes, "deny") == NC_OK &&
		pack_bulk_number(&p, &rbytes, gdpr_info.access_deny_count) == NC_OK &&
		pack_bulk_string(&p, &rbytes, "other") == NC_OK &&
		pack_bulk_number(&p, &rbytes, gdpr_info.access_other_count) == NC_OK &&
		pack_bulk_string(&p, &rbytes, "success") == NC_OK &&
		pack_bulk_number(&p, &rbytes, gdpr_info.access_success_count) ==
			NC_OK &&
		pack_bulk_string(&p, &rbytes, "downgrade") == NC_OK &&
		pack_bulk_number(&p, &rbytes, gdpr_info.access_downgrade_count) ==
			NC_OK &&
		pack_bulk_string(&p, &rbytes, "ip") == NC_OK) {
		unsigned i = 0;
		for (i = 0; i < addr_count; ++i) {
			if (gdpr_info.addr_mgr.addr[i].addr[0] != '\0' &&
				pack_bulk_string(&p, &rbytes,
								 gdpr_info.addr_mgr.addr[i].addr) != NC_OK) {
				return gdpr_require_auth_raw_response();
			}
		}
		raw_response.len = p - (char *)raw_response.data;
		return &raw_response;
	}

	return gdpr_require_auth_raw_response();
}

struct string *gdpr_require_auth_raw_response(void) {
	static const char msg_require_auth_str[] = "-ERR Auth Require\r\n";
	static struct string msg_require_auth = {
		sizeof(msg_require_auth_str) - 1,
		(uint8_t *)msg_require_auth_str,
	};
	return &msg_require_auth;
}

void gdpr_add_peer_client(const char *addr, char state) {
	addr_mgr_add(addr, state);
}

const gdpr_auth_msg_t *gdpr_auth_check(const void *data, const struct msg *msg,
									   int sd) {
	struct conf *conf = (struct conf *)data;
	char gdpr_switch;
	struct string token;
	const char *ip;
	if (conf->gdpr.mode == GDPR_DISABLE) {
		gdpr_info.access_other_count++;
		return gdpr_auth_succ;
	}

	if (split_msg(msg, &gdpr_switch, &token) != NC_OK) {
		gdpr_info.access_deny_count++;
		return gdpr_auth_argument_protocol;
	}
	if (gdpr_switch == '2') {
		if (token.len == conf->gdpr.password.len &&
			strncmp((char *)token.data, (char *)conf->gdpr.password.data,
					token.len) == 0) {
			return gdpr_auth_downgrade;
		}
		return gdpr_auth_password;
	}
	if (gdpr_switch == '1' || conf->gdpr.mode == GDPR_GRANT_STRICT) {
		ip = nc_unresolve_peer_desc(sd);
		const gdpr_auth_msg_t *result = NULL;
		result = __gdpr_auth_check(conf, &token, ip);
		result->result == GDPR_AUTH_SUCC ? gdpr_info.access_success_count++
										 : gdpr_info.access_deny_count++;
		log_info("dpsauth:%s %d", ip, result->result);
		gdpr_add_peer_client(ip, 's');
		return result;
	} else {
		ip = nc_unresolve_peer_desc(sd);
		gdpr_add_peer_client(ip, 'd');
		log_info("dpsauth-downgrade:%s", ip);
		gdpr_info.access_downgrade_count++;
		return gdpr_auth_downgrade;
	}
}
