/*
 * twemproxy - A fast and lightweight proxy for memcached protocol.
 * Copyright (C) 2011 Twitter, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <nc_core.h>
#include <nc_conf.h>
#include <nc_server.h>
#include <proto/nc_proto.h>
#include <arpa/inet.h>

#define DEFINE_ACTION(_hash, _name) string(#_name),
static struct string hash_strings[] = {
    HASH_CODEC( DEFINE_ACTION )
    null_string
};
#undef DEFINE_ACTION

#define DEFINE_ACTION(_hash, _name) hash_##_name,
static hash_t hash_algos[] = {
    HASH_CODEC( DEFINE_ACTION )
    NULL
};
#undef DEFINE_ACTION

#define DEFINE_ACTION(_dist, _name) string(#_name),
static struct string dist_strings[] = {
    DIST_CODEC( DEFINE_ACTION )
    null_string
};
#undef DEFINE_ACTION

static struct string read_prefer_strings[] = {
    string("none"),
    string("master"),
    string("slave"),
    null_string
};

static struct command conf_commands[] = {
	{string("auth_mode"), conf_set_auth_mode, 0},
	{string("grant_psm"), conf_add_grant_psm, 0},
	{string("authority_public_key"), conf_add_authority_public_key, 0},
	{string("proxy_password"), conf_set_proxy_password, 0},
	{string("listen"), conf_set_listen, offsetof(struct conf_pool, listen)},

	{string("hash"), conf_set_hash, offsetof(struct conf_pool, hash)},

	{string("hash_tag"), conf_set_hashtag,
	 offsetof(struct conf_pool, hash_tag)},

	{string("distribution"), conf_set_distribution,
	 offsetof(struct conf_pool, distribution)},

	{string("read_prefer"), conf_set_read_prefer,
	 offsetof(struct conf_pool, read_prefer)},

	{string("read_local_first"), conf_set_bool,
	 offsetof(struct conf_pool, read_local_first)},

	{string("timeout"), conf_set_num, offsetof(struct conf_pool, timeout)},

	{string("backlog"), conf_set_num, offsetof(struct conf_pool, backlog)},

	{string("client_connections"), conf_set_num,
	 offsetof(struct conf_pool, client_connections)},

	{string("redis"), conf_set_bool, offsetof(struct conf_pool, redis)},

	{string("preconnect"), conf_set_bool,
	 offsetof(struct conf_pool, preconnect)},

	{string("auto_eject_hosts"), conf_set_bool,
	 offsetof(struct conf_pool, auto_eject_hosts)},

	{string("server_connections"), conf_set_num,
	 offsetof(struct conf_pool, server_connections)},

	{string("server_connections_wr"), conf_set_num,
	 offsetof(struct conf_pool, server_connections_wr)},

	{string("server_retry_timeout"), conf_set_num,
	 offsetof(struct conf_pool, server_retry_timeout)},

	{string("server_failure_limit"), conf_set_num,
	 offsetof(struct conf_pool, server_failure_limit)},

	{string("servers"), conf_add_server, offsetof(struct conf_pool, server)},

	{string("sync_write_queue_num"), conf_set_num,
	 offsetof(struct conf_pool, sync_write_queue_num)},

	null_command};

struct idcinfo conf_idcinfos[] = {
    { string("10.3.0.0"),
      string("langfang")},
    { string("10.6.0.0"),
      string("langfang")},
    { string("10.8.0.0"),
      string("langfang")},
    { string("10.9.0.0"),
      string("langfang")},
    { string("10.10.0.0"),
      string("langfang")},
    { string("10.11.0.0"),
      string("langfang")},
    { string("10.12.0.0"),
      string("langfang")},
    { string("10.13.0.0"),
      string("langfang")},
    { string("10.14.0.0"),
      string("langfang")},
    { string("10.15.0.0"),
      string("langfang")},
    { string("10.20.0.0"),
      string("huailai")},
    { string("10.21.0.0"),
      string("huailai")},
    { string("10.22.0.0"),
      string("huailai")},
    { string("10.23.0.0"),
      string("huailai")},
    { string("10.24.0.0"),
      string("huailai")},
    { string("10.25.0.0"),
      string("huailai")},
    { string("10.26.0.0"),
      string("huailai")},
    { string("10.27.0.0"),
      string("huailai")},
    { string("10.28.0.0"),
      string("huailai")},
    { string("10.29.0.0"),
      string("huailai")},
    { string("10.30.0.0"),
      string("huailai")},
    { string("10.31.0.0"),
      string("huailai")},
    { string("10.32.0.0"),
      string("huailai")},
    { string("10.100.0.0"),
      string("va")},
    { string("10.101.0.0"),
      string("sg")},
    { string("10.110.0.0"),
      string("aliva")},
    { string("10.115.0.0"),
      string("alisg")},
    { string("end"),
      string("end")}
};

void
conf_server_init(struct conf_server *cs)
{
    string_init(&cs->pname);
    string_init(&cs->name);
    cs->port = 0;
    cs->weight = 0;

    memset(&cs->info, 0, sizeof(cs->info));

    cs->valid = 0;
    cs->slave = 0;
    cs->local = 0;

    log_debug(LOG_VVERB, "init conf server %p", cs);
}

static void
conf_server_deinit(struct conf_server *cs)
{
    string_deinit(&cs->pname);
    string_deinit(&cs->name);
    cs->valid = 0;
    log_debug(LOG_VVERB, "deinit conf server %p", cs);
}

rstatus_t
conf_server_transform(struct conf_server *cs, struct server *s, uint32_t nslave)
{
    rstatus_t status;

    ASSERT(cs->valid);

    /* idx, sidx, owner should be initialized in server_init */

    s->pname = cs->pname;
    s->name = cs->name;
    string_init(&s->runid);
    s->port = (uint16_t)cs->port;
    s->weight = (uint32_t)cs->weight;
    s->slave = cs->slave;
    s->local = cs->local;

    s->family = cs->info.family;
    s->addrlen = cs->info.addrlen;
    s->addr = (struct sockaddr *)&cs->info.addr;

    s->ns_conn_q_rd = 0;
    TAILQ_INIT(&s->s_conn_q_rd);
    s->ns_conn_q_wr = 0;
    TAILQ_INIT(&s->s_conn_q_wr);

    s->next_retry = 0LL;
    s->failure_count = 0;

    /* We precalloc the server array to make sure not realloc, which may waste
     * some memory */
    if (nslave > 0) {
        status = array_init(&s->slave_pool, nslave, sizeof(struct server));
        if (status != NC_OK) {
            return status;
        }
    } else {
        array_null(&s->slave_pool);
    }
    status = array_init(&s->slaves, 1, sizeof(struct server*));
    if (status != NC_OK) {
        return status;
    }
    s->local_server = NULL;

    log_debug(LOG_VERB, "transform to server '%.*s'",
              s->pname.len, s->pname.data);

    return NC_OK;
}

/**
  * update ip:port
  */
rstatus_t
conf_server_update(struct conf_server *cs, struct server *s)
{
    ASSERT(cs->valid);

    /* idx, sidx, owner should be initialized in server_init */

    s->pname = cs->pname;
    s->name = cs->name;
    string_init(&s->runid);
    s->port = (uint16_t)cs->port;
    s->weight = (uint32_t)cs->weight;
    s->slave = cs->slave;
    s->local = cs->local;

    s->family = cs->info.family;
    s->addrlen = cs->info.addrlen;
    s->addr = (struct sockaddr *)&cs->info.addr;

    s->next_retry = 0LL;
    s->failure_count = 0;

    s->local_server = NULL;

    log_debug(LOG_VERB, "update to server '%.*s'",
              s->pname.len, s->pname.data);

    return NC_OK;
}

// well-formed configuration
// gdpr:
//   auth_mode: grant_all
//   grant_psm:
//      - a.b.c:123333
//      - a.b.e:1111
//   authority_public_key:
//      - tce:0:kkkkkkk
// NOTE: Ensure only one config section of gdpr. Otherwise memleak happen.
static rstatus_t conf_gdpr_init(struct string *name) {
	const char *gdpr_section_name = "gdpr";
	if (strncmp((char *)name->data, gdpr_section_name, name->len) != 0) {
		return NC_ERROR;
	}
	return NC_OK;
}

static void conf_gdpr_deinit(struct conf_gdpr *cg) {
	string_deinit(&cg->password);
	cg->mode = GDPR_DISABLE;
	gdpr_deinit_acl_context(&cg->acl_context);
	gdpr_deinit_authority_context(&cg->authority_context);
}

static rstatus_t
conf_pool_init(struct conf_pool *cp, struct string *name)
{
    rstatus_t status;

    string_init(&cp->name);

    string_init(&cp->listen.pname);
    string_init(&cp->listen.name);
    cp->listen.port = 0;
    memset(&cp->listen.info, 0, sizeof(cp->listen.info));
    cp->listen.valid = 0;

    cp->hash = CONF_UNSET_HASH;
    string_init(&cp->hash_tag);
    cp->distribution = CONF_UNSET_DIST;
    cp->read_prefer = CONF_UNSET_READ_PREFER;
    cp->read_local_first = CONF_UNSET_NUM;
    cp->sync_write_queue_num = CONF_UNSET_NUM;

    cp->timeout = CONF_UNSET_NUM;
    cp->backlog = CONF_UNSET_NUM;

    cp->client_connections = CONF_UNSET_NUM;

    cp->redis = CONF_UNSET_NUM;
    cp->preconnect = CONF_UNSET_NUM;
    cp->auto_eject_hosts = CONF_UNSET_NUM;
    cp->server_connections = CONF_UNSET_NUM;
    cp->server_connections_wr = CONF_UNSET_NUM;
    cp->server_retry_timeout = CONF_UNSET_NUM;
    cp->server_failure_limit = CONF_UNSET_NUM;

    array_null(&cp->server);

    cp->valid = 0;

    status = string_duplicate(&cp->name, name);
    if (status != NC_OK) {
        return status;
    }

    status = array_init(&cp->server, CONF_DEFAULT_SERVERS,
                        sizeof(struct conf_server));
    if (status != NC_OK) {
        string_deinit(&cp->name);
        return status;
    }

    log_debug(LOG_VVERB, "init conf pool %p, '%.*s'", cp, name->len, name->data);

    return NC_OK;
}

static void
conf_pool_deinit(struct conf_pool *cp)
{
    string_deinit(&cp->name);

    string_deinit(&cp->listen.pname);
    string_deinit(&cp->listen.name);
    string_deinit(&cp->hash_tag);

    while (array_n(&cp->server) != 0) {
        conf_server_deinit(array_pop(&cp->server));
    }
    array_deinit(&cp->server);

    log_debug(LOG_VVERB, "deinit conf pool %p", cp);
}

/*
 * conf_pool 与 server_pool数组
 */
rstatus_t
conf_pool_each_diff(void *elem, void *data)
{
    rstatus_t status;
    struct conf_pool *cp = elem;
    struct array *server_pool = data;
    struct server_pool *sp;

    ASSERT(cp->valid);

    bool exist = false;
    uint32_t i, nelem;
    for (i = 0, nelem = array_n(server_pool); i< nelem; i++) {
        struct server_pool *server_p = array_get(server_pool, i);
        if (string_compare(&server_p->name, &cp->name) == 0) {
            exist = true;
            sp = server_p;
            break;
        }
    }

	if (exist) {
        /* 用cp的server更新sp的server，比较每个server */
        status = server_array_update(&sp->server, &cp->server, sp);
    } else {
        /* add new server_pool */
        status = conf_pool_each_transform(cp, server_pool);
    }

    return status;
}

rstatus_t
conf_pool_each_transform(void *elem, void *data)
{
    rstatus_t status;
    // 解析配置池
    struct conf_pool *cp = elem;
    // 服务池 数组，只是申请了空间的server_pool
    // 现在elem(conf)转换到server_pool内
    struct array *server_pool = data;
    // 单个 服务池，临时变量当作从server_pool取出的某个临时变量而已
    struct server_pool *sp;

    ASSERT(cp->valid);

    // 第几次执行，就拿到空间中第几个位置
    // 第一次执行的话，就拿到这个数组的内存中第一个位置
    sp = array_push(server_pool);
    ASSERT(sp != NULL);

    sp->idx = array_idx(server_pool, sp);
    sp->ctx = NULL;

    sp->p_conn = NULL;
    sp->nc_conn_q = 0;
    TAILQ_INIT(&sp->c_conn_q);

    array_null(&sp->server);
    sp->slots = NULL;
    sp->ncontinuum = 0;
    sp->nserver_continuum = 0;
    sp->continuum = NULL;
    sp->nlive_server = 0;
    sp->next_rebuild = 0LL;

    sp->name = cp->name;
    sp->addrstr = cp->listen.pname;
    sp->port = (uint16_t)cp->listen.port;

    sp->family = cp->listen.info.family;
    sp->addrlen = cp->listen.info.addrlen;
    sp->addr = (struct sockaddr *)&cp->listen.info.addr;

    sp->key_hash_type = cp->hash;
    sp->key_hash = hash_algos[cp->hash];
    sp->dist_type = cp->distribution;
    sp->hash_tag = cp->hash_tag;
    sp->read_prefer = cp->read_prefer;
    sp->read_local_first = cp->read_local_first;
    sp->sync_write_queue_num =  cp->sync_write_queue_num;

    sp->redis = cp->redis ? 1 : 0;
    sp->timeout = cp->timeout * 1000;
    sp->backlog = cp->backlog;

    sp->client_connections = (uint32_t)cp->client_connections;

    sp->server_connections = (uint32_t)cp->server_connections;
    sp->server_connections_wr = (uint32_t)cp->server_connections_wr;
    sp->server_retry_timeout = (int64_t)cp->server_retry_timeout * 1000LL;
    sp->server_failure_limit = (uint32_t)cp->server_failure_limit;
    sp->auto_eject_hosts = cp->auto_eject_hosts ? 1 : 0;
    sp->preconnect = cp->preconnect ? 1 : 0;

    status = server_init(&sp->server, &cp->server, sp);
    if (status != NC_OK) {
        return status;
    }

    /* init failover server */
    memset(sp->fail_server, 0, sizeof(sp->fail_server));

    log_debug(LOG_VERB, "transform to pool %"PRIu32" '%.*s'", sp->idx,
              sp->name.len, sp->name.data);

    return NC_OK;
}

static void
conf_dump(struct conf *cf)
{
    uint32_t i, j, npool, nserver;
    struct conf_pool *cp;
    struct conf_server *s;

    npool = array_n(&cf->pool);
    if (npool == 0) {
        return;
    }

    log_debug(LOG_VVERB, "%"PRIu32" pools in configuration file '%s'", npool,
              cf->fname);

    for (i = 0; i < npool; i++) {
        cp = array_get(&cf->pool, i);

        log_debug(LOG_VVERB, "%.*s", cp->name.len, cp->name.data);
        log_debug(LOG_VVERB, "  listen: %.*s",
                  cp->listen.pname.len, cp->listen.pname.data);
        log_debug(LOG_VVERB, "  timeout: %d", cp->timeout);
        log_debug(LOG_VVERB, "  backlog: %d", cp->backlog);
        log_debug(LOG_VVERB, "  hash: %d", cp->hash);
        log_debug(LOG_VVERB, "  hash_tag: \"%.*s\"", cp->hash_tag.len,
                  cp->hash_tag.data);
        log_debug(LOG_VVERB, "  distribution: %d", cp->distribution);
        log_debug(LOG_VVERB, "  read_prefer: %d", cp->read_prefer);
        log_debug(LOG_VVERB, "  read_local_first: %d", cp->read_local_first);
        log_debug(LOG_VVERB, "  sync_write_queue: %d", cp->sync_write_queue_num);
        log_debug(LOG_VVERB, "  client_connections: %d",
                  cp->client_connections);
        log_debug(LOG_VVERB, "  redis: %d", cp->redis);
        log_debug(LOG_VVERB, "  preconnect: %d", cp->preconnect);
        log_debug(LOG_VVERB, "  auto_eject_hosts: %d", cp->auto_eject_hosts);
        log_debug(LOG_VVERB, "  server_connections: %d",
                  cp->server_connections);
        log_debug(LOG_VVERB, "  server_connections_wr: %d",
                  cp->server_connections_wr);
        log_debug(LOG_VVERB, "  server_retry_timeout: %d",
                  cp->server_retry_timeout);
        log_debug(LOG_VVERB, "  server_failure_limit: %d",
                  cp->server_failure_limit);

        nserver = array_n(&cp->server);
        log_debug(LOG_VVERB, "  servers: %"PRIu32"", nserver);

        for (j = 0; j < nserver; j++) {
            s = array_get(&cp->server, j);
            log_debug(LOG_VVERB, "    %.*s name:%.*s slave:%d local:%d",
                      s->pname.len, s->pname.data, s->name.len, s->name.data,
                      s->slave, s->local);
        }
    }
    // 为什么没有打印出来 ？？？？
	log_debug(LOG_VVERB, "GDPR Configuration");
	log_debug(LOG_VVERB, "  mode:%d", cf->gdpr.mode);
	log_debug(LOG_VVERB, "  key:");
	for (i = 0; i < array_n(&cf->gdpr.authority_context.keyset); ++i) {
		gdpr_public_key_t *key =
			array_get(&cf->gdpr.authority_context.keyset, i);
		log_debug(LOG_VVERB, "    - %.*s %d", key->name.len, key->name.data,
				  key->version);
		(void)key;
	}
	log_debug(LOG_VVERB, "  psm:");
	for (i = 0; i < array_n(&cf->gdpr.acl_context.grant_services); ++i) {
		gdpr_grant_service_t *psm =
			array_get(&cf->gdpr.acl_context.grant_services, i);
		log_debug(LOG_VVERB, "    - %.*s %d", psm->grant_psm.len,
				  psm->grant_psm.data, psm->expire_time);
		(void)psm;
	}
}

static rstatus_t
conf_yaml_init(struct conf *cf)
{
    int rv;

    ASSERT(!cf->valid_parser);

    // 这里的意思是文件读写位置移动到开头
    rv = fseek(cf->fh, 0L, SEEK_SET);
    if (rv < 0) {
        log_error("conf: failed to seek to the beginning of file '%s': %s",
                  cf->fname, strerror(errno));
        return NC_ERROR;
    }

    // 解析器初始化？
    rv = yaml_parser_initialize(&cf->parser);
    if (!rv) {
        log_error("conf: failed (err %d) to initialize yaml parser",
                  cf->parser.error);
        return NC_ERROR;
    }

    // 将打开的 文件描述符 fh 放进解析器
    yaml_parser_set_input_file(&cf->parser, cf->fh);
    cf->valid_parser = 1;

    return NC_OK;
}

static void
conf_yaml_deinit(struct conf *cf)
{
    if (cf->valid_parser) {
        yaml_parser_delete(&cf->parser);
        cf->valid_parser = 0;
    }
}

static rstatus_t
conf_token_next(struct conf *cf)
{
    int rv;

    ASSERT(cf->valid_parser && !cf->valid_token);

    rv = yaml_parser_scan(&cf->parser, &cf->token);
    if (!rv) {
        log_error("conf: failed (err %d) to scan next token", cf->parser.error);
        return NC_ERROR;
    }
    cf->valid_token = 1;

    return NC_OK;
}

static void
conf_token_done(struct conf *cf)
{
    ASSERT(cf->valid_parser);

    if (cf->valid_token) {
        yaml_token_delete(&cf->token);
        cf->valid_token = 0;
    }
}

static rstatus_t
conf_event_next(struct conf *cf)
{
    int rv;

    ASSERT(cf->valid_parser && !cf->valid_event);

    rv = yaml_parser_parse(&cf->parser, &cf->event);
    if (!rv) {
        log_error("conf: failed (err %d) to get next event", cf->parser.error);
        return NC_ERROR;
    }
    cf->valid_event = 1;

    return NC_OK;
}

static void
conf_event_done(struct conf *cf)
{
    if (cf->valid_event) {
        yaml_event_delete(&cf->event);
        cf->valid_event = 0;
    }
}

static rstatus_t
conf_push_scalar(struct conf *cf)
{
    rstatus_t status;
    struct string *value;
    uint8_t *scalar;
    uint32_t scalar_len;

    scalar = cf->event.data.scalar.value;
    scalar_len = (uint32_t)cf->event.data.scalar.length;

    log_debug(LOG_VVERB, "push '%.*s'", scalar_len, scalar);

    value = array_push(&cf->arg);
    if (value == NULL) {
        return NC_ENOMEM;
    }
    string_init(value);

    status = string_copy(value, scalar, scalar_len);
    if (status != NC_OK) {
        array_pop(&cf->arg);
        return status;
    }

    return NC_OK;
}

static void
conf_pop_scalar(struct conf *cf)
{
    struct string *value;

    value = array_pop(&cf->arg);
    log_debug(LOG_VVERB, "pop '%.*s'", value->len, value->data);
    string_deinit(value);
}

static rstatus_t
conf_handler(struct conf *cf, void *data)
{
    struct command *cmd;
    struct string *key, *value;
    uint32_t narg;

    if (array_n(&cf->arg) == 1) {
        value = array_top(&cf->arg);
        log_debug(LOG_VVERB, "conf handler on '%.*s'", value->len, value->data);
		if (conf_gdpr_init(value) == NC_ERROR) {
			return conf_pool_init(data, value);
		}
		array_pop(&cf->pool);
		return NC_OK;
	}

    narg = array_n(&cf->arg);
    value = array_get(&cf->arg, narg - 1);
    key = array_get(&cf->arg, narg - 2);

    log_debug(LOG_VVERB, "conf handler on %.*s: %.*s", key->len, key->data,
              value->len, value->data);

    for (cmd = conf_commands; cmd->name.len != 0; cmd++) {
        char *rv;

        if (string_compare(key, &cmd->name) != 0) {
            continue;
        }

        rv = cmd->set(cf, cmd, data);
        if (rv != CONF_OK) {
            log_error("conf: directive \"%.*s\" value \"%.*s\" %s",
                      key->len, key->data, value->len, value->data, rv);
            return NC_ERROR;
        }

        return NC_OK;
    }

    log_error("conf: directive \"%.*s\" is unknown", key->len, key->data);

    return NC_ERROR;
}

static rstatus_t
conf_begin_parse(struct conf *cf)
{
    rstatus_t status;
    bool done;

    ASSERT(cf->sound && !cf->parsed);
    ASSERT(cf->depth == 0);

    status = conf_yaml_init(cf);
    if (status != NC_OK) {
        return status;
    }

    done = false;
    do {
        status = conf_event_next(cf);
        if (status != NC_OK) {
            return status;
        }

        log_debug(LOG_VVERB, "next begin event %d", cf->event.type);

        switch (cf->event.type) {
        case YAML_STREAM_START_EVENT:
        case YAML_DOCUMENT_START_EVENT:
            break;

        case YAML_MAPPING_START_EVENT:
            ASSERT(cf->depth < CONF_MAX_DEPTH);
            cf->depth++;
            done = true;
            break;

        default:
            NOT_REACHED();
        }

        conf_event_done(cf);

    } while (!done);

    return NC_OK;
}

static rstatus_t
conf_end_parse(struct conf *cf)
{
    rstatus_t status;
    bool done;

    ASSERT(cf->sound && !cf->parsed);
    ASSERT(cf->depth == 0);

    done = false;
    do {
        status = conf_event_next(cf);
        if (status != NC_OK) {
            return status;
        }

        log_debug(LOG_VVERB, "next end event %d", cf->event.type);

        switch (cf->event.type) {
        case YAML_STREAM_END_EVENT:
            done = true;
            break;

        case YAML_DOCUMENT_END_EVENT:
            break;

        default:
            NOT_REACHED();
        }

        conf_event_done(cf);
    } while (!done);

    conf_yaml_deinit(cf);

    return NC_OK;
}

static rstatus_t
conf_parse_core(struct conf *cf, void *data)
{
    rstatus_t status;
    bool done, leaf, new_pool;

    ASSERT(cf->sound);

    status = conf_event_next(cf);
    if (status != NC_OK) {
        return status;
    }

    log_debug(LOG_VVERB, "next event %d depth %"PRIu32" seq %d", cf->event.type,
              cf->depth, cf->seq);

    done = false;
    leaf = false;
    new_pool = false;

    switch (cf->event.type) {
    case YAML_MAPPING_END_EVENT:
        cf->depth--;
        if (cf->depth == 1) {
            conf_pop_scalar(cf);
        } else if (cf->depth == 0) {
            done = true;
        }
        break;

    case YAML_MAPPING_START_EVENT:
        cf->depth++;
        break;

    case YAML_SEQUENCE_START_EVENT:
        cf->seq = 1;
        break;

    case YAML_SEQUENCE_END_EVENT:
        conf_pop_scalar(cf);
        cf->seq = 0;
        break;

    case YAML_SCALAR_EVENT:
        status = conf_push_scalar(cf);
        if (status != NC_OK) {
            break;
        }

        /* take appropriate action */
        if (cf->seq) {
            /* for a sequence, leaf is at CONF_MAX_DEPTH */
            ASSERT(cf->depth == CONF_MAX_DEPTH);
            leaf = true;
        } else if (cf->depth == CONF_ROOT_DEPTH) {
            /* create new conf_pool */
            data = array_push(&cf->pool);
            if (data == NULL) {
                status = NC_ENOMEM;
                break;
           }
           new_pool = true;
        } else if (array_n(&cf->arg) == cf->depth + 1) {
            /* for {key: value}, leaf is at CONF_MAX_DEPTH */
            ASSERT(cf->depth == CONF_MAX_DEPTH);
            leaf = true;
        }
        break;

    default:
        NOT_REACHED();
        break;
    }

    conf_event_done(cf);

    if (status != NC_OK) {
        return status;
    }

    if (done) {
        /* terminating condition */
        return NC_OK;
    }

    if (leaf || new_pool) {
        status = conf_handler(cf, data);

        if (leaf) {
            conf_pop_scalar(cf);
            if (!cf->seq) {
                conf_pop_scalar(cf);
            }
        }

        if (status != NC_OK) {
            return status;
        }
    }

    // 递归循环解析
    return conf_parse_core(cf, data);
}

static rstatus_t
conf_parse(struct conf *cf)
{
    rstatus_t status;

    ASSERT(cf->sound && !cf->parsed);
    ASSERT(array_n(&cf->arg) == 0);

    status = conf_begin_parse(cf);
    if (status != NC_OK) {
        return status;
    }

    status = conf_parse_core(cf, NULL);
    if (status != NC_OK) {
        return status;
    }

    status = conf_end_parse(cf);
    if (status != NC_OK) {
        return status;
    }

    cf->parsed = 1;

    return NC_OK;
}

static struct conf *
conf_open(char *filename)
{
    rstatus_t status;
    struct conf *cf;
    FILE *fh;

    fh = fopen(filename, "r");
    if (fh == NULL) {
        log_error("conf: failed to open configuration '%s': %s", filename,
                  strerror(errno));
        return NULL;
    }

    cf = nc_alloc(sizeof(*cf));
    if (cf == NULL) {
        fclose(fh);
        return NULL;
    }

    status = array_init(&cf->arg, CONF_DEFAULT_ARGS, sizeof(struct string));
    if (status != NC_OK) {
        nc_free(cf);
        fclose(fh);
        return NULL;
    }

    status = array_init(&cf->pool, CONF_DEFAULT_POOL, sizeof(struct conf_pool));
    if (status != NC_OK) {
        array_deinit(&cf->arg);
        nc_free(cf);
        fclose(fh);
        return NULL;
    }

    cf->fname = filename;
    cf->fh = fh;
    cf->depth = 0;
    /* parser, event, and token are initialized later */
    cf->seq = 0;
    cf->valid_parser = 0;
    cf->valid_event = 0;
    cf->valid_token = 0;
    cf->sound = 0;
    cf->parsed = 0;
    cf->valid = 0;
	cf->gdpr.mode = GDPR_DISABLE;
	string_init(&cf->gdpr.password);
	if (gdpr_init_acl_context(&cf->gdpr.acl_context) != NC_OK) {
		array_deinit(&cf->pool);
		array_deinit(&cf->arg);
		nc_free(cf);
		fclose(fh);
		return NULL;
	}
	if (gdpr_init_authority_context(&cf->gdpr.authority_context) != NC_OK) {
		gdpr_deinit_acl_context(&cf->gdpr.acl_context);
		array_deinit(&cf->pool);
		array_deinit(&cf->arg);
		nc_free(cf);
		fclose(fh);
		return NULL;
	}

	log_debug(LOG_VVERB, "opened conf '%s'", filename);

    return cf;
}

// 循环验证打开的文件描述符中的document是否正确
static rstatus_t
conf_validate_document(struct conf *cf)
{
    rstatus_t status;
    uint32_t count;
    bool done;

    status = conf_yaml_init(cf);
    if (status != NC_OK) {
        return status;
    }

    count = 0;
    done = false;
    do {
        yaml_document_t document;
        yaml_node_t *node;
        int rv;

        // 加载文件流中的下一个document，
        rv = yaml_parser_load(&cf->parser, &document);
        if (!rv) {
            log_error("conf: failed (err %d) to get the next yaml document",
                      cf->parser.error);
            conf_yaml_deinit(cf);
            return NC_ERROR;
        }

        // 检查是否有root node
        node = yaml_document_get_root_node(&document);
        if (node == NULL) {
            done = true;
        } else {
            count++;
        }

        yaml_document_delete(&document);
    } while (!done);

    conf_yaml_deinit(cf);

    if (count != 1) {
        log_error("conf: '%s' must contain only 1 document; found %"PRIu32" "
                  "documents", cf->fname, count);
        return NC_ERROR;
    }

    return NC_OK;
}

static rstatus_t
conf_validate_tokens(struct conf *cf)
{
    rstatus_t status;
    bool done, error;
    int type;

    status = conf_yaml_init(cf);
    if (status != NC_OK) {
        return status;
    }

    done = false;
    error = false;
    do {
        status = conf_token_next(cf);
        if (status != NC_OK) {
            return status;
        }
        type = cf->token.type;

        switch (type) {
        case YAML_NO_TOKEN:
            error = true;
            log_error("conf: no token (%d) is disallowed", type);
            break;

        case YAML_VERSION_DIRECTIVE_TOKEN:
            error = true;
            log_error("conf: version directive token (%d) is disallowed", type);
            break;

        case YAML_TAG_DIRECTIVE_TOKEN:
            error = true;
            log_error("conf: tag directive token (%d) is disallowed", type);
            break;

        case YAML_DOCUMENT_START_TOKEN:
            error = true;
            log_error("conf: document start token (%d) is disallowed", type);
            break;

        case YAML_DOCUMENT_END_TOKEN:
            error = true;
            log_error("conf: document end token (%d) is disallowed", type);
            break;

        case YAML_FLOW_SEQUENCE_START_TOKEN:
            error = true;
            log_error("conf: flow sequence start token (%d) is disallowed", type);
            break;

        case YAML_FLOW_SEQUENCE_END_TOKEN:
            error = true;
            log_error("conf: flow sequence end token (%d) is disallowed", type);
            break;

        case YAML_FLOW_MAPPING_START_TOKEN:
            error = true;
            log_error("conf: flow mapping start token (%d) is disallowed", type);
            break;

        case YAML_FLOW_MAPPING_END_TOKEN:
            error = true;
            log_error("conf: flow mapping end token (%d) is disallowed", type);
            break;

        case YAML_FLOW_ENTRY_TOKEN:
            error = true;
            log_error("conf: flow entry token (%d) is disallowed", type);
            break;

        case YAML_ALIAS_TOKEN:
            error = true;
            log_error("conf: alias token (%d) is disallowed", type);
            break;

        case YAML_ANCHOR_TOKEN:
            error = true;
            log_error("conf: anchor token (%d) is disallowed", type);
            break;

        case YAML_TAG_TOKEN:
            error = true;
            log_error("conf: tag token (%d) is disallowed", type);
            break;

        case YAML_BLOCK_SEQUENCE_START_TOKEN:
        case YAML_BLOCK_MAPPING_START_TOKEN:
        case YAML_BLOCK_END_TOKEN:
        case YAML_BLOCK_ENTRY_TOKEN:
            break;

        case YAML_KEY_TOKEN:
        case YAML_VALUE_TOKEN:
        case YAML_SCALAR_TOKEN:
            break;

        case YAML_STREAM_START_TOKEN:
            break;

        case YAML_STREAM_END_TOKEN:
            done = true;
            log_debug(LOG_VVERB, "conf '%s' has valid tokens", cf->fname);
            break;

        default:
            error = true;
            log_error("conf: unknown token (%d) is disallowed", type);
            break;
        }

        conf_token_done(cf);
    } while (!done && !error);

    conf_yaml_deinit(cf);

    return !error ? NC_OK : NC_ERROR;
}

static rstatus_t
conf_validate_structure(struct conf *cf)
{
    rstatus_t status;
    int type, depth;
    uint32_t i, count[CONF_MAX_DEPTH + 1];
	bool done, error;
	int seq;

	status = conf_yaml_init(cf);
    if (status != NC_OK) {
        return status;
    }

    done = false;
    error = false;
	seq = 0;
	depth = 0;
    for (i = 0; i < CONF_MAX_DEPTH + 1; i++) {
        count[i] = 0;
    }

    /*
     * Validate that the configuration conforms roughly to the following
     * yaml tree structure:
     *
     * keyx:
     *   key1: value1
     *   key2: value2
     *   seq:
     *     - elem1
     *     - elem2
     *     - elem3
     *   key3: value3
     *
     * keyy:
     *   key1: value1
     *   key2: value2
     *   seq:
     *     - elem1
     *     - elem2
     *     - elem3
     *   key3: value3
     */
    do {
        status = conf_event_next(cf);
        if (status != NC_OK) {
            return status;
        }

        type = cf->event.type;

        log_debug(LOG_VVERB, "next event %d depth %d seq %d", type, depth, seq);

        switch (type) {
        case YAML_STREAM_START_EVENT:
        case YAML_DOCUMENT_START_EVENT:
            break;

        case YAML_DOCUMENT_END_EVENT:
            break;

        case YAML_STREAM_END_EVENT:
            done = true;
            break;

        case YAML_MAPPING_START_EVENT:
            if (depth == CONF_ROOT_DEPTH && count[depth] != 1) {
                error = true;
                log_error("conf: '%s' has more than one \"key:value\" at depth"
                          " %d", cf->fname, depth);
            } else if (depth >= CONF_MAX_DEPTH) {
                error = true;
                log_error("conf: '%s' has a depth greater than %d", cf->fname,
                          CONF_MAX_DEPTH);
            }
            depth++;
            break;

        case YAML_MAPPING_END_EVENT:
            if (depth == CONF_MAX_DEPTH) {
				seq = 0;
			}
            depth--;
            count[depth] = 0;
            break;

        case YAML_SEQUENCE_START_EVENT:
            if (seq) {
            } else if (depth != CONF_MAX_DEPTH) {
                error = true;
                log_error("conf: '%s' has sequence at depth %d instead of %d",
                          cf->fname, depth, CONF_MAX_DEPTH);
            } else if (count[depth] != 1) {
                error = true;
                log_error("conf: '%s' has invalid \"key:value\" at depth %d",
                          cf->fname, depth);
            }
			seq++;
			break;

        case YAML_SEQUENCE_END_EVENT:
            ASSERT(depth == CONF_MAX_DEPTH);
            count[depth] = 0;
			break;

        case YAML_SCALAR_EVENT:
            if (depth == 0) {
                error = true;
                log_error("conf: '%s' has invalid empty \"key:\" at depth %d",
                          cf->fname, depth);
            } else if (depth == CONF_ROOT_DEPTH && count[depth] != 0) {
                error = true;
                log_error("conf: '%s' has invalid mapping \"key:\" at depth %d",
                          cf->fname, depth);
            } else if (depth == CONF_MAX_DEPTH && count[depth] == 2) {
                /* found a "key: value", resetting! */
                count[depth] = 0;
            }
            count[depth]++;
            break;

        default:
            NOT_REACHED();
        }

        conf_event_done(cf);
    } while (!done && !error);

    conf_yaml_deinit(cf);

    return !error ? NC_OK : NC_ERROR;
}

static rstatus_t
conf_pre_validate(struct conf *cf)
{
    rstatus_t status;

    status = conf_validate_document(cf);
    if (status != NC_OK) {
        return status;
    }

    status = conf_validate_tokens(cf);
    if (status != NC_OK) {
        return status;
    }

    status = conf_validate_structure(cf);
    if (status != NC_OK) {
        return status;
    }

    cf->sound = 1;

    return NC_OK;
}

static int
conf_server_name_cmp(const void *t1, const void *t2)
{
    const struct conf_server *s1 = t1, *s2 = t2;
    int result;

    /* order: name, slave, pname */
    result = string_compare(&s1->name, &s2->name);
    if (result != 0) {
        return result;
    }
    result = s1->slave - s2->slave; /* master will before slave */
    //if (result != 0) {
    return result;
    //}
    // return string_compare(&s1->pname, &s2->pname);
}

static int
conf_pool_name_cmp(const void *t1, const void *t2)
{
    const struct conf_pool *p1 = t1, *p2 = t2;

    return string_compare(&p1->name, &p2->name);
}

static int
conf_pool_listen_cmp(const void *t1, const void *t2)
{
    const struct conf_pool *p1 = t1, *p2 = t2;

    return string_compare(&p1->listen.pname, &p2->listen.pname);
}

static rstatus_t
conf_validate_server(struct conf *cf, struct conf_pool *cp)
{
    uint32_t i, nserver;
    bool valid;

    nserver = array_n(&cp->server);
    if (nserver == 0) {
        log_error("conf: pool '%.*s' has no servers", cp->name.len,
                  cp->name.data);
        return NC_ERROR;
    }

    /*
     * Disallow duplicate masters - masters with identical "host:port:weight"
     * or "name" combination are considered as duplicates. When server name
     * is configured, we only check for duplicate "name" and not for duplicate
     * "host:port:weight"
     *
     * It's allowed that a shard has no master. In this case, write requests
     * will fail
     */
     if (cp->distribution != DIST_REDIS_CLUSTER) {
        array_sort(&cp->server, conf_server_name_cmp);
     }
    for (valid = true, i = 0; i < nserver - 1; i++) {
        struct conf_server *cs1, *cs2;

        cs1 = array_get(&cp->server, i);
        cs2 = array_get(&cp->server, i + 1);

        if (string_compare(&cs1->name, &cs2->name) == 0
            && !cs1->slave && !cs2->slave) {
            log_error("conf: pool '%.*s' '%.*s' is dulicated or has duplicate "
                      "masters", cp->name.len, cp->name.data, cs1->name.len, 
                      cs1->name.data);
            valid = false;
            break;
        }
    }
    if (!valid) {
        return NC_ERROR;
    }

    return NC_OK;
}

static rstatus_t
conf_validate_pool(struct conf *cf, struct conf_pool *cp)
{
    rstatus_t status;

    ASSERT(!cp->valid);
    ASSERT(!string_empty(&cp->name));

    if (!cp->listen.valid) {
        log_error("conf: directive \"listen:\" is missing");
        return NC_ERROR;
    }

    /* set default values for unset directives */

    if (cp->distribution == CONF_UNSET_DIST) {
        cp->distribution = CONF_DEFAULT_DIST;
    }

    if (cp->hash == CONF_UNSET_HASH) {
        cp->hash = CONF_DEFAULT_HASH;
    }

    if (cp->read_prefer == CONF_UNSET_READ_PREFER) {
        cp->read_prefer = CONF_DEFAULT_READ_PREFER;
    }

    if (cp->read_local_first == CONF_UNSET_NUM) {
        cp->read_local_first = CONF_DEFAULT_READ_LOCAL;
    }

    if (cp->sync_write_queue_num == CONF_UNSET_NUM) {
        cp->sync_write_queue_num = CONF_DEFAULT_SYNC_WRITE_QUEUE_NUM;
    }

    if (cp->timeout == CONF_UNSET_NUM) {
        cp->timeout = CONF_DEFAULT_TIMEOUT;
    }

    if (cp->backlog == CONF_UNSET_NUM) {
        cp->backlog = CONF_DEFAULT_LISTEN_BACKLOG;
    }

    cp->client_connections = CONF_DEFAULT_CLIENT_CONNECTIONS;

    if (cp->redis == CONF_UNSET_NUM) {
        cp->redis = CONF_DEFAULT_REDIS;
    }

    if (cp->preconnect == CONF_UNSET_NUM) {
        cp->preconnect = CONF_DEFAULT_PRECONNECT;
    }

    if (cp->auto_eject_hosts == CONF_UNSET_NUM) {
        cp->auto_eject_hosts = CONF_DEFAULT_AUTO_EJECT_HOSTS;
    }

    if (cp->server_connections == CONF_UNSET_NUM) {
        cp->server_connections = CONF_DEFAULT_SERVER_CONNECTIONS;
    } else if (cp->server_connections == 0) {
        log_error("conf: directive \"server_connections:\" cannot be 0");
        return NC_ERROR;
    }

    if (cp->server_connections_wr == CONF_UNSET_NUM) {
        cp->server_connections_wr = CONF_DEFAULT_SERVER_CONNECTIONS_WR;
    } else if (cp->server_connections_wr == 0) {
        log_error("conf: directive \"server_connections_wr:\" cannot be 0");
        return NC_ERROR;
    }

    if (cp->server_retry_timeout == CONF_UNSET_NUM) {
        cp->server_retry_timeout = CONF_DEFAULT_SERVER_RETRY_TIMEOUT;
    }

    if (cp->server_failure_limit == CONF_UNSET_NUM) {
        cp->server_failure_limit = CONF_DEFAULT_SERVER_FAILURE_LIMIT;
    }

    if (cp->distribution && !cp->redis) {
        log_error("conf: redis_cluster distribution only for redis");
        return NC_ERROR;
    }

    status = conf_validate_server(cf, cp);
    if (status != NC_OK) {
        return status;
    }

    cp->valid = 1;

    return NC_OK;
}

static rstatus_t
conf_post_validate(struct conf *cf)
{
    rstatus_t status;
    uint32_t i, npool;
    bool valid;

    ASSERT(cf->sound && cf->parsed);
    ASSERT(!cf->valid);

    // 所有的配置现在都已经放在了解析池里了, parsed pool
    npool = array_n(&cf->pool);
    if (npool == 0) {
        log_error("conf: '%.*s' has no pools", cf->fname);
        return NC_ERROR;
    }

    /* validate pool */
    for (i = 0; i < npool; i++) {
        struct conf_pool *cp = array_get(&cf->pool, i);

        status = conf_validate_pool(cf, cp);
        if (status != NC_OK) {
            return status;
        }
    }

    /* disallow pools with duplicate listen: key values */
    array_sort(&cf->pool, conf_pool_listen_cmp);
    for (valid = true, i = 0; i < npool - 1; i++) {
        struct conf_pool *p1, *p2;

        p1 = array_get(&cf->pool, i);
        p2 = array_get(&cf->pool, i + 1);

        if (string_compare(&p1->listen.pname, &p2->listen.pname) == 0) {
            log_error("conf: pools '%.*s' and '%.*s' have the same listen "
                      "address '%.*s'", p1->name.len, p1->name.data,
                      p2->name.len, p2->name.data, p1->listen.pname.len,
                      p1->listen.pname.data);
            valid = false;
            break;
        }
    }
    if (!valid) {
        return NC_ERROR;
    }

    /* disallow pools with duplicate names */
    array_sort(&cf->pool, conf_pool_name_cmp);
    for (valid = true, i = 0; i < npool - 1; i++) {
        struct conf_pool *p1, *p2;

        p1 = array_get(&cf->pool, i);
        p2 = array_get(&cf->pool, i + 1);

        if (string_compare(&p1->name, &p2->name) == 0) {
            log_error("conf: '%s' has pools with same name %.*s'", cf->fname,
                      p1->name.len, p1->name.data);
            valid = false;
            break;
        }
    }
    if (!valid) {
        return NC_ERROR;
    }

    return NC_OK;
}

struct conf *
conf_create(char *filename)
{
    rstatus_t status;
    struct conf *cf;

    cf = conf_open(filename);
    if (cf == NULL) {
        return NULL;
    }

    /* validate configuration file before parsing */
    status = conf_pre_validate(cf);
    if (status != NC_OK) {
        goto error;
    }

    /* parse the configuration file */
    status = conf_parse(cf);
    if (status != NC_OK) {
        goto error;
    }

    /* validate parsed configuration */
    status = conf_post_validate(cf);
    if (status != NC_OK) {
        goto error;
    }

    conf_dump(cf);

    fclose(cf->fh);
    cf->fh = NULL;

    return cf;

error:
    fclose(cf->fh);
    cf->fh = NULL;
    conf_destroy(cf);
    return NULL;
}

void
conf_destroy(struct conf *cf)
{
    while (array_n(&cf->arg) != 0) {
        conf_pop_scalar(cf);
    }
    array_deinit(&cf->arg);

    while (array_n(&cf->pool) != 0) {
        conf_pool_deinit(array_pop(&cf->pool));
    }
    array_deinit(&cf->pool);
	conf_gdpr_deinit(&cf->gdpr);

	nc_free(cf);
}

char *
conf_set_string(struct conf *cf, struct command *cmd, void *conf)
{
    rstatus_t status;
    uint8_t *p;
    struct string *field, *value;

    p = conf;
    field = (struct string *)(p + cmd->offset);

    if (field->data != CONF_UNSET_PTR) {
        return "is a duplicate";
    }

    value = array_top(&cf->arg);

    status = string_duplicate(field, value);
    if (status != NC_OK) {
        return CONF_ERROR;
    }

    return CONF_OK;
}

char *
conf_set_listen(struct conf *cf, struct command *cmd, void *conf)
{
    rstatus_t status;
    struct string *value;
    struct conf_listen *field;
    uint8_t *p, *name;
    uint32_t namelen;

    p = conf;
    field = (struct conf_listen *)(p + cmd->offset);

    if (field->valid == 1) {
        return "is a duplicate";
    }

    value = array_top(&cf->arg);

    status = string_duplicate(&field->pname, value);
    if (status != NC_OK) {
        return CONF_ERROR;
    }

    if (value->len == 0) {
        field->info.family = AF_UNSPEC;
        field->valid = 1;
        return CONF_OK;
    } else if (value->data[0] == '/') {
        name = value->data;
        namelen = value->len;
    } else {
        uint8_t *q, *start, *port;
        uint32_t portlen;

        /* parse "hostname:port" from the end */
        p = value->data + value->len - 1;
        start = value->data;
        q = nc_strrchr(p, start, ':');
        if (q == NULL) {
            return "has an invalid \"hostname:port\" format string";
        }

        port = q + 1;
        portlen = (uint32_t)(p - port + 1);

        p = q - 1;

        name = start;
        namelen = (uint32_t)(p - start + 1);

        field->port = nc_atoi(port, portlen);
        if (field->port < 0 || !nc_valid_port(field->port)) {
            return "has an invalid port in \"hostname:port\" format string";
        }
    }

    status = string_copy(&field->name, name, namelen);
    if (status != NC_OK) {
        return CONF_ERROR;
    }

    status = nc_resolve(&field->name, field->port, &field->info);
    if (status != NC_OK) {
        return CONF_ERROR;
    }

    field->valid = 1;

    return CONF_OK;
}

/* check server->addr and local_ip in the same idc */
bool 
check_local_server(struct string *server, struct string *local_ip, uint32_t lpm_mask) 
{
    struct string addrend = string("end");
    struct string serveridc;
    struct string localidc;
    struct idcinfo *addridc;

	string_init(&serveridc);
	string_init(&localidc);
	for (addridc = conf_idcinfos; string_compare(&addridc->addr, &addrend) != 0; addridc++) {
        if ((ntohl(inet_addr((char *)addridc->addr.data) ^ inet_addr((char *)server->data)) & lpm_mask) == 0) {
            string_copy(&serveridc, addridc->idcname.data, addridc->idcname.len);
        }
        if ((ntohl(inet_addr((char *)addridc->addr.data) ^ inet_addr((char *)local_ip->data)) & lpm_mask) == 0) {
            string_copy(&localidc, addridc->idcname.data, addridc->idcname.len);
        }
    }

    if (string_compare(&serveridc, &localidc) == 0) {
        return true;
    } else {
        return false;
    }

}

char *
conf_add_server(struct conf *cf, struct command *cmd, void *conf)
{
    rstatus_t status;
    struct array *a;
    struct string *value;
    struct conf_server *field;
    bool is_unix_socket;
    uint8_t *p, *q, *start, *end;
    uint8_t *pname, *addr, *port, *weight, *name, *role;
    uint32_t k, len, pnamelen, addrlen, portlen, weightlen, namelen, rolelen;
    struct string address;
    struct conf_pool* cpool;

    p = conf;
    a = (struct array *)(p + cmd->offset);
    cpool = (struct conf_pool *)conf;

    field = array_push(a);
    if (field == NULL) {
        return CONF_ERROR;
    }

    conf_server_init(field);

    value = array_top(&cf->arg);
    is_unix_socket = (value->data[0] == '/' ? true : false);

    /* parse "hostname:port:weight [name [master|slave]]"
     * or "/path/unix_socket:weight [name [master|slave]]" from the begging */

    p = value->data;
    end = value->data + value->len;
    pname = NULL;
    pnamelen = 0;
    name = NULL;
    namelen = 0;
    role = NULL;
    rolelen = 0;

    for (k = 0; ; k++) {
        q = nc_strchr(p, end, ' ');
        len = (uint32_t)((q != NULL ? q : end) - p);
        switch (k) {
        case 0:
            pname = p;
            pnamelen = len;
            break;
        case 1:
            name = p;
            namelen = len;
            break;
        case 2:
            role = p;
            rolelen = len;
            break;
        default:
            NOT_REACHED();
            break;
        }
        if (q == NULL) {
            break;
        } else {
            p = q + 1;
        }
    }

    if (k > 2 || pname == NULL || pnamelen == 0) {
        return "has an invalid \"hostname:port:weight [name [role]]\" or "
               "\"/path/unix_socket:weight [name [role]]\" format string";
    }

    /* parse hostname:port:weight or /path/unix_socket:weight from the end */

    p = pname + pnamelen - 1;
    start = pname;
    addr = NULL;
    addrlen = 0;
    weight = NULL;
    weightlen = 0;
    port = NULL;
    portlen = 0;

    for (k = 0; ; k++) {
        q = nc_strrchr(p, start, ':');
        if (q == NULL) {
            break;
        }

        switch (k) {
        case 0:
            weight = q + 1;
            weightlen = (uint32_t)(p - weight + 1);
            break;
        case 1:
            port = q + 1;
            portlen = (uint32_t)(p - port + 1);
            break;
        default:
            NOT_REACHED();
            break;
        }

        p = q - 1;
    }

    /* the remaining part is the addr */
    addr = start;
    addrlen = (uint32_t)(p - start + 1);

    if (k != (is_unix_socket ? 1 : 2) /* k should match delim times */
        || (addr == NULL || addrlen == 0)
        || (!is_unix_socket && (port == NULL || portlen == 0))
        || (weight == NULL || weightlen == 0)) {
        return "has an invalid \"hostname:port:weight\" or "
               "\"/path/unix_socket:weight\" format string";
    }

    /* now, check optional value and copy it */

    status = string_copy(&field->pname, pname, pnamelen);
    if (status != NC_OK) {
        return CONF_ERROR;
    }

    field->weight = nc_atoi(weight, weightlen);
    if (field->weight < 0) {
        return "has an invalid weight in \"hostname:port:weight "
               "[name [role]]\" format string";
    }

    if (!is_unix_socket) {
        field->port = nc_atoi(port, portlen);
        if (field->port < 0 || !nc_valid_port(field->port)) {
            return "has an invalid port in \"hostname:port:weight "
                   "[name [role]]\" format string";
        }
    }

    if (name == NULL) {
        /*
         * To maintain backward compatibility with libmemcached, we don't
         * include the port as the part of the input string to the consistent
         * hashing algorithm, when it is equal to 11211.
         */
        if (field->port == CONF_DEFAULT_KETAMA_PORT) {
            name = addr;
            namelen = addrlen;
        } else {
            name = addr;
            namelen = addrlen + 1 + portlen;
        }
    }

    status = string_copy(&field->name, name, namelen);
    if (status != NC_OK) {
        return CONF_ERROR;
    }

    if (role != NULL) {
        if (nc_strncmp("master", role, rolelen) == 0) {
            field->slave = 0;
        } else if (nc_strncmp("slave", role, rolelen) == 0) {
            field->slave = 1;
        } else {
            return "has an invalid role in \"hostname:port:weight "
                   "[name [role]]\" format string";
        }
    }

    /* resolve addr */

    /*if (is_unix_socket || nc_is_local_addr((char*)addr, addrlen)) {
        field->local = 1;
    }*/

    uint32_t mask = ~((1ull << (32 - 16)) - 1);

    string_init(&address);
    status = string_copy(&address, addr, addrlen);
    if (status != NC_OK) {
        string_deinit(&address);
        return CONF_ERROR;
    }

    status = nc_resolve(&address, field->port, &field->info);
    if (status != NC_OK) {
        string_deinit(&address);
        return CONF_ERROR;
    }

    if (check_local_server(&address, &(cpool->listen.name), mask)) {
        field->local = 1;
    }

    string_deinit(&address);
    field->valid = 1;

    return CONF_OK;
}

char *conf_add_grant_psm(struct conf *cf, struct command *cmd, void *conf) {
	struct conf_gdpr *gdpr = &cf->gdpr;
	gdpr_grant_service_t *grant_psm = NULL;
	struct string *value = NULL;
	uint8_t *q;

	(void)cmd;
	(void)conf;
	grant_psm = array_push(&gdpr->acl_context.grant_services);
	gdpr_init_grant_service(grant_psm);

	if (grant_psm == NULL) {
		return CONF_ERROR;
	}
	value = array_top(&cf->arg);
	q = nc_strchr(value->data, value->data + value->len, ':');
	if (q == NULL || q >= value->data + value->len) {
		return CONF_ERROR;
	}
	grant_psm->expire_time = nc_atoi(q + 1, (value->data + value->len - q - 1));
	log_debug(LOG_NOTICE, "GDPR PSM expire %d", grant_psm->expire_time);
	if (grant_psm->expire_time < 0) {
		return CONF_ERROR;
	}

	string_init(&grant_psm->grant_psm);
	if (string_copy(&grant_psm->grant_psm, value->data,
					(uint32_t)(q - value->data)) != NC_OK) {
		return CONF_ERROR;
	}
	log_debug(LOG_NOTICE, "GDPR PSM %.*s", q - value->data, value->data);

	return CONF_OK;
}

char *conf_set_proxy_password(struct conf *cf, struct command *cmd,
							  void *conf) {
	struct string *value;

	(void)conf;
	(void)cmd;
	value = array_top(&cf->arg);
	if (string_copy(&cf->gdpr.password, value->data, value->len) != NC_OK) {
		return CONF_ERROR;
	}
	return CONF_OK;
}

char *conf_add_authority_public_key(struct conf *cf, struct command *cmd,
									void *conf) {
	struct string *value;
	gdpr_public_key_t *key;
	uint8_t *start, *end, *next;
	int i = 0;

	(void)cmd;
	(void)conf;
	key = array_push(&cf->gdpr.authority_context.keyset);
	gdpr_init_public_key(key);

	value = array_top(&cf->arg);
	for (value = array_top(&cf->arg), start = value->data,
		end = start + value->len, next = start;
		 start < end && next != NULL; start = next + 1, i++) {
		next = nc_strchr(start, end, ':');
		switch (i) {
		case 0:
			log_debug(LOG_VERB, "GDPR PubKey:%.*s", next - start, start);
			// name
			if (string_copy(&key->name, start, (uint32_t)(next - start)) !=
				NC_OK) {
				return CONF_ERROR;
			}
			break;
		case 1:
			log_debug(LOG_VERB, "GDPR PubKey:%.*s", next - start, start);
			// version
			key->version = nc_atoi(start, (next - start));
			if (key->version < 0) {
				return CONF_ERROR;
			}
			break;
		case 2:
			log_debug(LOG_VERB, "GDPR PubKey:%.*s", end - start, start);
			// key pem string
			if (next != NULL) return CONF_ERROR;
			if (gdpr_init_key_context(&key->key_context, start,
									  (uint32_t)(end - start)) != NC_OK) {
				gdpr_deinit_key_context(&key->key_context);
				return CONF_ERROR;
			}
			break;
		default:
			return CONF_ERROR;
		}
	}

	return i != 3 ? CONF_ERROR : CONF_OK;
}

char *conf_set_auth_mode(struct conf *cf, struct command *cmd, void *conf) {
	struct string *value;

	(void)cmd;
	(void)conf;
	value = array_top(&cf->arg);
	cf->gdpr.mode = GDPR_UNKNOWN;
	log_debug(LOG_VVERB, "conf set auth mode %.*s", value->len, value->data);
	if (strncmp((char *)value->data, "disable", value->len) == 0) {
		cf->gdpr.mode = GDPR_DISABLE;
	} else if (strncmp((char *)value->data, "grant_strict", value->len) == 0) {
		cf->gdpr.mode = GDPR_GRANT_STRICT;
	} else if (strncmp((char *)value->data, "grant_all", value->len) == 0) {
		cf->gdpr.mode = GDPR_GRANT_ALL;
	}
	return cf->gdpr.mode == GDPR_UNKNOWN ? "unknown gdpr mode" : CONF_OK;
}

char *
conf_set_num(struct conf *cf, struct command *cmd, void *conf)
{
    uint8_t *p;
    int num, *np;
    struct string *value;

    p = conf;
    np = (int *)(p + cmd->offset);

    if (*np != CONF_UNSET_NUM) {
        return "is a duplicate";
    }

    value = array_top(&cf->arg);

    num = nc_atoi(value->data, value->len);
    if (num < 0) {
        return "is not a number";
    }

    *np = num;

    return CONF_OK;
}

char *
conf_set_bool(struct conf *cf, struct command *cmd, void *conf)
{
    uint8_t *p;
    int *bp;
    struct string *value, true_str, false_str;

    p = conf;
    bp = (int *)(p + cmd->offset);

    if (*bp != CONF_UNSET_NUM) {
        return "is a duplicate";
    }

    value = array_top(&cf->arg);
    string_set_text(&true_str, "true");
    string_set_text(&false_str, "false");

    if (string_compare(value, &true_str) == 0) {
        *bp = 1;
    } else if (string_compare(value, &false_str) == 0) {
        *bp = 0;
    } else {
        return "is not \"true\" or \"false\"";
    }

    return CONF_OK;
}

char *
conf_set_hash(struct conf *cf, struct command *cmd, void *conf)
{
    uint8_t *p;
    hash_type_t *hp;
    struct string *value, *hash;

    p = conf;
    hp = (hash_type_t *)(p + cmd->offset);

    if (*hp != CONF_UNSET_HASH) {
        return "is a duplicate";
    }

    value = array_top(&cf->arg);

    for (hash = hash_strings; hash->len != 0; hash++) {
        if (string_compare(value, hash) != 0) {
            continue;
        }

        *hp = hash - hash_strings;

        return CONF_OK;
    }

    return "is not a valid hash";
}

char *
conf_set_distribution(struct conf *cf, struct command *cmd, void *conf)
{
    uint8_t *p;
    dist_type_t *dp;
    struct string *value, *dist;

    p = conf;
    dp = (dist_type_t *)(p + cmd->offset);

    if (*dp != CONF_UNSET_DIST) {
        return "is a duplicate";
    }

    value = array_top(&cf->arg);

    for (dist = dist_strings; dist->len != 0; dist++) {
        if (string_compare(value, dist) != 0) {
            continue;
        }

        *dp = dist - dist_strings;

        return CONF_OK;
    }

    return "is not a valid distribution";
}

char *
conf_set_hashtag(struct conf *cf, struct command *cmd, void *conf)
{
    rstatus_t status;
    uint8_t *p;
    struct string *field, *value;

    p = conf;
    field = (struct string *)(p + cmd->offset);

    if (field->data != CONF_UNSET_PTR) {
        return "is a duplicate";
    }

    value = array_top(&cf->arg);

    if (value->len != 2) {
        return "is not a valid hash tag string with two characters";
    }

    status = string_duplicate(field, value);
    if (status != NC_OK) {
        return CONF_ERROR;
    }

    return CONF_OK;
}

char *
conf_set_read_prefer(struct conf *cf, struct command *cmd, void *conf)
{
    uint8_t *p;
    read_prefer_type_t *dp;
    struct string *value, *it;

    p = conf;
    dp = (read_prefer_type_t *)(p + cmd->offset);

    if (*dp != CONF_UNSET_READ_PREFER) {
        return "is a duplicate";
    }

    value = array_top(&cf->arg);

    for (it = read_prefer_strings; it->len != 0; it++) {
        if (string_compare(value, it) != 0) {
            continue;
        }

        *dp = it - read_prefer_strings;

        return CONF_OK;
    }

    return "is not a valid read_prefer";
}
