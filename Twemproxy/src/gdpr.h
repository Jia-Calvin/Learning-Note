#ifndef _GDPR_H_
#define _GDPR_H_

#include <cJSON.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <nc_string.h>

typedef enum gdpr_auth_mode {
	GDPR_UNKNOWN,
	GDPR_DISABLE,		// Disable gdpr. Always return '+OK\r\n'
	GDPR_GRANT_ALL,		// Grant access to all request. Take down illegal client
	GDPR_GRANT_STRICT,  // Perform the most strict check
} gdpr_auth_mode_t;

typedef enum {
	GDPR_AUTH_FAIL,
	GDPR_AUTH_DOWNGRADE,
	GDPR_AUTH_SUCC,
} gdpr_auth_result_t;

typedef struct {
	EVP_PKEY *key;
	BIO *buf_key;
	EVP_MD_CTX *ctx;
} gdpr_key_context_t;

typedef struct {
	cJSON *header_json;
	cJSON *payload_json;
	cJSON *version;
	cJSON *authority;		   // json:"authority"
	cJSON *authorityChain;	 // json:"authorityChain,omitempty"
	cJSON *podName;			   // json:"podName,omitempty"
	cJSON *primaryAuthType;	// json:"primaryAuthType,omitempty"
	cJSON *clientIp;		   // json:"clientIp,omitempty"
	cJSON *psm;				   // json:"psm,omitempty"
	cJSON *user;			   // json:"user,omitempty"
	cJSON *resource;		   // json:"resource,omitempty"
	cJSON *expireTime;		   // json:"expireTime,omitempty"
	cJSON *alg;
	cJSON *typ;
	struct string header;
	struct string payload;
	struct string digest;
	struct string text;
} dps_token_t;

typedef struct {
	struct string grant_psm;
	int expire_time;
} gdpr_grant_service_t;

typedef struct {
	struct array grant_services;  // gdpr_grant_service_t
} gdpr_acl_context_t;

typedef struct {
	gdpr_key_context_t key_context;
	struct string name;
	int version;
} gdpr_public_key_t;

typedef struct {
	struct array keyset;  // gdpr_public_key_t
} gdpr_authority_context_t;

typedef struct {
	gdpr_auth_result_t result;
	struct string reason;
} gdpr_auth_msg_t;

static inline void gdpr_init_grant_service(gdpr_grant_service_t *psm) {
	string_init(&psm->grant_psm);
	psm->expire_time = 0;
}

static inline void gdpr_init_public_key(gdpr_public_key_t *key) {
	string_init(&key->name);
	key->version = 0;
	key->key_context.buf_key = NULL;
	key->key_context.key = NULL;
}

int gdpr_init_key_context(gdpr_key_context_t *, const uint8_t *, uint32_t);
void gdpr_deinit_key_context(gdpr_key_context_t *gdpr_ctx);
int gdpr_verify_digest(const gdpr_key_context_t *gdpr_ctx,
					   const struct string *text, const struct string *sig);

int gdpr_parse_dps_token(dps_token_t *token, const struct string *encoded_str);
void gdpr_clean_dps_token(dps_token_t *token);
void gdpr_init_dps_token(dps_token_t *token);

int gdpr_init_authority_context(gdpr_authority_context_t *);
void gdpr_deinit_authority_context(gdpr_authority_context_t *);

int gdpr_init_acl_context(gdpr_acl_context_t *);
void gdpr_deinit_acl_context(gdpr_acl_context_t *);

const gdpr_auth_msg_t *gdpr_auth_check(const void *, const struct msg *,
									   int sd);

struct string *gdpr_require_auth_raw_response(void);
struct string *gdpr_info_raw_response(const void *cf);
void gdpr_add_peer_client(const char *addr, char);
#endif
