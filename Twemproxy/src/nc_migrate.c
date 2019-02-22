#include <nc_migrate.h>
#include <nc_array.h>
#include <nc_util.h>
#include <nc_core.h>
#include <nc_server.h>
#include <nc_connection.h>
// #include <nc_hashkit.h>
#include <stdio.h>
#include <ctype.h>
#include <extern/crc32.h>

int migrate_slot_num(uint8_t *key, uint32_t keylen)
{
    int slot_num = (int)crc32_checksum((const char*)key, keylen) & MIGRATE_SLOTS_MASK;
    return slot_num;
}

static void
wrapper_migrating_key(uint8_t **pkey, uint32_t keylen, struct msg *msg)
{
    uint32_t org_arg_len = (uint32_t)(msg->narg_end - msg->narg_start);
    if(msg->narg % 10 == 8 || msg->narg % 10 == 9 ){ // need carry
        msg->narg_end = msg->narg_end + 1;
    }
    msg->narg += 2;
    
    char pro_header[512] = {0};
    sprintf(pro_header, "*%d\r\n%s$%d\r\n", msg->narg, SLOT_WRAPPER, keylen);
    nc_memcpy(pro_header+ strlen(pro_header), *pkey, keylen);
    uint32_t pro_len = (uint32_t)strlen(pro_header);
    uint32_t expand_len = pro_len - org_arg_len;
    mbuf_expand_space_at_pos(&msg->mhdr, msg->narg_end, expand_len); 
    nc_memcpy(msg->narg_start, pro_header, pro_len);
    
    msg->key_start = msg->key_start + expand_len;
    msg->key_end = msg->key_end + expand_len;
    msg->migrate_wrapper = 1;
    *pkey = msg->key_start;
}

void
unwrapper_migrating_key(struct msg *msg)
{
    char pro_header[512] = {0};
    uint8_t *key = msg->key_start;
    uint32_t keylen = (uint32_t)(msg->key_end - msg->key_start);
    sprintf(pro_header, "*%d\r\n%s$%d\r\n", msg->narg, SLOT_WRAPPER, keylen);
    nc_memcpy(pro_header+ strlen(pro_header), key, keylen);
    uint32_t pro_len = (uint32_t)strlen(pro_header);
    if(msg->narg >= 10 && ( msg->narg % 10 == 0 || msg->narg % 10 == 1 )){ // carry
        msg->narg_end = msg->narg_end - 1;
    }
    msg->narg -= 2;
    uint32_t arg_len = (uint32_t)(msg->narg_end - msg->narg_start);
    uint32_t reduce_len = pro_len - arg_len;
    mbuf_delete_space(&msg->mhdr, msg->narg_end, reduce_len); 
    char arg_header[4] = {0};
    sprintf(arg_header, "*%d", msg->narg);
    nc_memcpy(msg->narg_start, arg_header, strlen(arg_header));
    
    msg->key_start = msg->key_start - reduce_len;
    msg->key_end = msg->key_end - reduce_len;
    msg->migrate_wrapper = 0;
}

struct server_pool * 
get_access_pool(struct context *ctx, struct conn *c_conn, uint8_t **pkey, 
                uint32_t keylen, struct msg *msg)
{
    struct server_pool *access_pool;
    if(!ctx->migrating){
        if (ctx->double_cluster_mode == MODE_MASTER || c_conn->owner == ctx->sync_cache_del) {
            access_pool = c_conn->owner;
        } else {
            switch (ctx->double_cluster_mode) {
            case MODE_SLAVE:
                if (msg_type_is_read(msg->type)) {
                    access_pool = c_conn->owner;
                } else {
                    access_pool = ctx->sync_cache_peer;
                }
                break;
            case MODE_REDIRECT:
                access_pool = ctx->sync_cache_peer;
                break;
            case MODE_READOTHER:
                if (msg_type_is_read(msg->type)) {
                    access_pool = ctx->sync_cache_peer;
                } else {
                    access_pool = c_conn->owner;
                }
                break;
            default:
                access_pool = c_conn->owner;
            }
        }
        return access_pool;
    }
    int access_slot = migrate_slot_num(*pkey, keylen);
    log_info("in migrating mode, current key:%.*s, slot_num:%d, migrating:%d",
             keylen, *pkey, access_slot, ctx->migrating_slot);
    if(access_slot > ctx->migrating_slot){
        log_info("access old pool.");
        return ctx->migrating_pool;
    }else if(access_slot == ctx->migrating_slot){
        log_info("access old pool first.");
        wrapper_migrating_key(pkey, keylen, msg);
        return ctx->migrating_pool;
    }
    else{
        log_info("access new pool.");
        return c_conn->owner;
    }
}

rstatus_t
unwrapper_migrating_response(struct msg *msg)
{
    if (!msg->migrate_wrapper){
        return NC_OK;
    }
    if (msg->migrate_status == RESULT_OK){
        return NC_OK;
    }

    enum {
        SW_START,
        SW_ERROR,
        SW_INTEGER,
        SW_MULTIBULK,
    } state;
    state = SW_START;

    struct mbuf *mbuf = STAILQ_LAST(&msg->mhdr, mbuf, next);
    uint8_t ch;
    uint32_t narg = 0,  integer = 0;
    bool got_migrate_status = false;
    uint8_t *real_content_start = 0, *arg_start = 0, *arg_end = 0, *p;
    for (p = mbuf->start; p < mbuf->last; p++) {
        ch = *p;
        switch(state){
            case SW_START:
                switch(ch){
                    case '*':
                        state = SW_MULTIBULK;
                        arg_start = p;
                        break;
                    case ':':
                        state = SW_INTEGER;
                        break;
                }
                break;
            case SW_MULTIBULK:
                if (ch == CR) {
                    arg_end = p;
                }else if(ch == LF) {
                    state = SW_START;
                }else if(isdigit(ch)) {
                    narg = narg * 10 + (uint32_t)(ch - '0');
                }else {
                    goto error;
                }
                break;
            case SW_INTEGER:
                if (ch == CR) {
                    if (!got_migrate_status){
                        if (integer == 0){
                            msg->migrate_status = RESULT_MOVED;
                        }else if(integer == 1){
                            msg->migrate_status = RESULT_RETRY;
                        }else if(integer == 2){
                            msg->migrate_status = RESULT_OK;
                        }else{
                            goto error;
                        }
                        got_migrate_status = true;
                        real_content_start = p + 2;
                        goto done;
                    }
                }else if (isdigit(ch)) {
                    integer = integer * 10 + (uint32_t)(ch - '0');
                }
                break;
            default:
                NOT_REACHED();
                goto error;
                break;
        }
    }

    if (mbuf->last - mbuf->start < 0){
        log_error("migrating response msg parsed failed.");
        goto error;
    }

done:
    narg -= 1;
    uint8_t * del_from;
    uint32_t del_size;
    if( narg == 0 ){
        return NC_OK;
    }else if (narg == 1){
        del_from = arg_start;
        del_size = (uint32_t)(real_content_start - arg_start);
    }else if (narg > 1){
        del_from = arg_end;
        del_size = (uint32_t)(real_content_start - arg_end -2);
    }

    mbuf_delete_space(&msg->mhdr, del_from, del_size); 

    return NC_OK;

error:
    log_error("unwrapper migrating response failed.");
    return NC_ERROR;
}

rstatus_t 
migrate_prepare(struct string* manage_cmd, struct string* result)
{
    log_info("migrate prepare...");
    string_set_text(result, "OK");
    return NC_OK;
}

rstatus_t 
migrate_begin(struct string* manage_cmd, struct string* result)
{
    struct context *ctx = global_nci.ctx;
    if (ctx->migrating == 1){
        log_info("aready in migrating mode.");
        string_set_text(result, "ERR:Aready in migrating mode");
        return NC_ERROR;
    }
    log_info("migrate begin.");
    ctx->migrating = 1;
    ctx->migrating_slot = 0;

    if (server_double_conf(ctx) != NC_OK) {
        string_set_text(result, "ERR:Fail to reload update conf_file");
        return NC_ERROR;
    }

    string_set_text(result, "OK");
    return NC_OK;
}

rstatus_t 
migrate_slot(struct string* manage_cmd, struct string* result)
{
    uint8_t *split_pos = nc_strchr(manage_cmd->data, manage_cmd->data+manage_cmd->len, ':');
    uint32_t slot_length = (uint32_t)(manage_cmd->len - (split_pos - manage_cmd->data) - 1);
    struct context *ctx = global_nci.ctx;
    ctx->migrating_slot = nc_atoi(split_pos+1, slot_length);
    log_info("set migrating slot:%d", ctx->migrating_slot);
    string_set_text(result, "OK");
    return NC_OK;
}

rstatus_t 
migrate_discard(struct string* manage_cmd, struct string* result)
{
    struct context *ctx = global_nci.ctx;
    if (!ctx->migrating){
        string_set_text(result, "ERR:Not in MIGRATING MODE");
        return NC_ERROR;
    }
    ctx->migrating = 0;
    ctx->migrating_slot = 0;
    log_info("migrate migrate_discard.");
    // need reload old routing ?
    string_set_text(result, "OK");
    return NC_OK;
}

rstatus_t 
migrate_done(struct string* manage_cmd, struct string* result)
{
    struct context *ctx = global_nci.ctx;
    if (!ctx->migrating){
        string_set_text(result, "ERR:Not in MIGRATING MODE");
        return NC_ERROR;
    }
    ctx->migrating = 0;
    ctx->migrating_slot = 0;
    log_info("migrate done.");
    string_set_text(result, "OK");
    return NC_OK;
}

rstatus_t 
migrate_status(struct string* manage_cmd, struct string* result)
{
    struct context *ctx = global_nci.ctx;
    string_init(result);
    result->len = MIGRATE_STATUS_LEN;
    result->data = nc_zalloc(MIGRATE_STATUS_LEN+1);
    sprintf(result->data,"migrating:%d, migrating_slot:%d",
        ctx->migrating, ctx->migrating_slot);
    return NC_OK;
}

#define key_equal_cmd(cmd, key, keylen) (sizeof(cmd)==keylen+1&&nc_strncmp(cmd, key, keylen)==0)
int 
set_double_cluster_mode(struct context *ctx, struct string value)
{
    int type = MSG_RSP_REDIS_STATUS;
    if (key_equal_cmd("master", value.data, value.len)) {
        ctx->double_cluster_mode = MODE_MASTER;
    } else if (key_equal_cmd("slave", value.data, value.len)) {
        ctx->double_cluster_mode = MODE_SLAVE;
    } else if (key_equal_cmd("redirect", value.data, value.len)) {
        ctx->double_cluster_mode = MODE_REDIRECT;
    } else if (key_equal_cmd("readother", value.data, value.len)) {
        ctx->double_cluster_mode = MODE_READOTHER;
    } else if (key_equal_cmd("cache", value.data, value.len)) {
        ctx->double_cluster_mode = MODE_CACHE;
    } else {
        type = MSG_RSP_REDIS_ERROR;
    }
    return type;
}

rstatus_t 
double_cluster_mode(struct string* manage_cmd, struct string* result)
{
    int type;
    uint8_t *split_pos = nc_strchr(manage_cmd->data, manage_cmd->data + manage_cmd->len, ':');
    uint32_t arg_length = (uint32_t)(manage_cmd->len - (split_pos - manage_cmd->data) - 1);
    struct context *ctx = global_nci.ctx;
    struct string value;
    string_init(&value);
    value.len = arg_length;
    value.data = split_pos+1;
    type = set_double_cluster_mode(ctx, value);
    log_info("set doubleclustermode:'%.*s'", manage_cmd->len, manage_cmd->data);
    if (type != MSG_RSP_REDIS_STATUS){
        string_set_text(result, "ERR:unknown synccachedel argument");
        return NC_ERROR;
    }
    string_set_text(result, "OK");
    return NC_OK;
}