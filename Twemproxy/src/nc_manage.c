#include <hashkit/nc_hashkit.h>
#include <nc_hash_table.h>
#include <nc_array.h>
#include <nc_conf.h>
#include <nc_util.h>
#include <nc_manage.h>
#include <nc_core.h>
#include <nc_server.h>
#include <nc_connection.h>
#include <nc_migrate.h>


static rstatus_t
manager_accept(struct context *ctx, struct conn *conn)
{
    ssize_t n;
    int sd;
    struct sockaddr_storage addr;
    socklen_t addr_len;

    for (;;) {
        sd = accept(conn->sd, NULL, NULL);
        if (sd < 0) {
            if (errno == EINTR) {
                log_debug(LOG_VERB, "accept on p %d not ready - eintr", conn->sd);
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED) {
                log_debug(LOG_VERB, "accept on p %d not ready - eagain", conn->sd);
                conn->recv_ready = 0;
                return NC_OK;
            }
            if (errno == EMFILE || errno == ENFILE) {
                log_crit("accept on p %d failed: %s", conn->sd,
                            strerror(errno));
                conn->recv_ready = 0;
                log_panic("HIT MAX OPEN FILES, IT SHOULD NOT HAPPEN. ABORT.");
                return NC_OK;
            }
            log_error("accept on p %d failed: %s", conn->sd, strerror(errno));
            return NC_ERROR;
        }
        addr_len = sizeof(addr);
        if (getsockname(sd, (struct sockaddr *)&addr, &addr_len)) {
            log_error("getsockname on p %d failed: %s", conn->sd, strerror(errno));
            close(sd);
            continue;
        }
        break;
    }
    
    char cmd_len[MANAGE_CMD_BYTE_LEN];
    n = nc_recvn(sd, &cmd_len, MANAGE_CMD_BYTE_LEN);
    if (n < 0){
        log_error("recv manage command length failed:%s", strerror(errno));
        close(sd);
        return NC_ERROR;
    }
    
    struct string manage_cmd;
    string_init(&manage_cmd);
    manage_cmd.len = (uint32_t)nc_atoi(&cmd_len, MANAGE_CMD_BYTE_LEN);
    manage_cmd.data = nc_zalloc(manage_cmd.len+1);

    n = nc_recvn(sd,  manage_cmd.data, manage_cmd.len);
    if (n < 0 ){
        log_error("recv manage command on %d failed:%s", sd, strerror(errno));
        close(sd);
        return NC_ERROR;
    }

    struct string result;
    string_init(&result);
    command_processor(&manage_cmd, &result);
    log_debug(LOG_VERB, "send results on sd %d %d bytes", sd, result.len);

    n = nc_sendn(sd, result.data, result.len);
    if (n < 0) {
        log_error("send result on sd %d failed: %s", sd, strerror(errno));
        close(sd);
        return NC_ERROR;
    }

    close(sd);
    return NC_OK;
}

rstatus_t
manager_recv(struct context *ctx, struct conn *conn)
{
    rstatus_t status;
    ASSERT(conn->recv_active);

    conn->recv_ready = 1;
    do {
        status = manager_accept(ctx, conn);
        if (status != NC_OK) {
            return status;
        }
    } while (conn->recv_ready);

    return NC_OK;
}

static rstatus_t
manager_listen(struct context *ctx, struct conn *p, char *manage_addr)
{
    rstatus_t status;
    struct sockinfo si;
    struct string addr;
    string_set_raw(&addr, manage_addr);
    status = nc_resolve_unix(&addr, &si);
    if (status < 0) {
        return status;
    }

    p->sd = socket(si.family, SOCK_STREAM, 0);
    if (p->sd < 0) {
        log_error("socket failed: %s", strerror(errno));
        return NC_ERROR;
    }

    status = nc_set_reuseaddr(p->sd);
    if (status < 0) {
        log_error("set reuseaddr on m %d failed: %s", p->sd, strerror(errno));
        return NC_ERROR;
    }

    if (si.family == AF_UNIX && si.addr.un.sun_path[0] == '/' &&
        unlink(si.addr.un.sun_path) && errno != ENOENT) {
        log_error("unlink %s failed: %s", si.addr.un.sun_path, strerror(errno));
        return NC_ERROR;
    }
    status = bind(p->sd, (struct sockaddr *)&si.addr, si.addrlen);
    if (status < 0) {
        log_error("bind on m %d to addr '%.*s' failed: %s", p->sd,
                    addr.len, addr.data, strerror(errno));
        return NC_ERROR;
    }

    status = listen(p->sd, SOMAXCONN);
    if (status < 0) {
        log_error("listen on m %d failed: %s", p->sd, strerror(errno));
        return NC_ERROR;
    }

    status = nc_set_nonblocking(p->sd);
    if (status < 0) {
        log_error("set nonblock on p %d on addr '%.*s' failed: %s", p->sd,
                    addr.len, addr.data, strerror(errno));
        return NC_ERROR;
    }

    log_notice("m %d listening on '%.*s'", p->sd, addr.len, addr.data);

    status = event_add_conn(ctx->evb, p);
    if (status < 0) {
        log_error("event add conn p %d on addr '%.*s' failed: %s",
                  p->sd, addr.len, addr.data, strerror(errno));
        return NC_ERROR;
    }

    status = event_del_out(ctx->evb, p);
    if (status < 0) {
        log_error("event del out p %d on addr '%.*s' failed: %s",
                  p->sd, addr.len, addr.data, strerror(errno));
        return NC_ERROR;
    }

    return NC_OK;
}

rstatus_t
manager_init(struct context *ctx, char *manage_addr)
{
    if (manage_addr==NULL){
        return NC_OK;
    }
    struct server_pool *pool = array_get(&ctx->pool, 0);
    rstatus_t status;
    struct conn *p;

    p = conn_get_manager(pool);
    if (p == NULL) {
        return NC_ENOMEM;
    }

    status = manager_listen(ctx, p, manage_addr);
    if (status != NC_OK) {
        close(p->sd);
        return status;
    }

    log_debug(LOG_VVERB, "init manager with %"PRIu32" pools",
              array_n(&ctx->pool));
    return NC_OK;
}

void
manager_deinit(struct context *ctx)
{
    // log_debug(LOG_VVERB, "deinit manager with %"PRIu32" pools",
    //         array_n(&ctx->pool));
    return;
}

manage_type_info_t mtype_info[] = {
    { MANAGE_COMMAND_UNKNOWN,   "unknown",          NULL },
    /* migrate commands*/
    { REDIS_MIGRATE_PREPARE,    "migrate_prepare",  migrate_prepare},
    { REDIS_MIGRATE_BEGIN,      "migrate_begin",    migrate_begin},
    { REDIS_MIGRATE_SLOT,       "migrate_slot",     migrate_slot},
    { REDIS_MIGRATE_DISCARD,    "migrate_discard",  migrate_discard},
    { REDIS_MIGRATE_DONE,       "migrate_done",     migrate_done},
    { REDIS_MIGRATE_STATUS,     "migrate_status",   migrate_status},
    { DOUBLE_CLUSTER_MODE,      "doubleclustermode",double_cluster_mode},
    {MANAGE_COMMAND_END,        "end",              NULL},
};

const uint MANAGE_TYPE_LEN = sizeof(mtype_info) / sizeof(mtype_info[0]);


bool
manage_type_check()
{
    unsigned i;
    for (i = MANAGE_COMMAND_UNKNOWN; i < MANAGE_COMMAND_END; i++) {
        if (mtype_info[i].type != i) {
            log_crit("manage type info %d mismatch %d %s",
                     i, mtype_info[i].type, mtype_info[i].str);
            return false;
        }
    }
    return true;
}

void
command_processor(struct string* manage_cmd, struct string* result){
    log_debug(LOG_VERB, "recv command len:%d ,real len:%d, %.*s",
                 manage_cmd->len, nc_strlen(manage_cmd->data),
                 manage_cmd->len, manage_cmd->data);
    manage_cmd->len = (uint32_t)nc_strlen(manage_cmd->data);
    uint8_t *split_pos = nc_strchr(manage_cmd->data, manage_cmd->data+manage_cmd->len, ':');
    uint32_t compare_len;
    compare_len = manage_cmd->len;
    if(split_pos != NULL){
        compare_len = (uint32_t)(split_pos - manage_cmd->data);
    }
    
    size_t i;
    for(i = 0; i < MANAGE_TYPE_LEN; i++){
        if(nc_strncmp(manage_cmd->data, mtype_info[i].str, compare_len) ==0){
            mtype_info[i].manage_cmd_handler(manage_cmd, result);
            break;
        }
    }
}

const char *
manage_type_string(manage_type_t type)
{
    ASSERT(type > MANAGE_COMMAND_UNKNOWN && type < MANAGE_COMMAND_END);
    return mtype_info[type].str;
}