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
#include <nc_server.h>
#include <nc_metric.h>
#include <nc_redis_cluster.h>

struct msg *
rsp_get(struct conn *conn)
{
    struct msg *msg;

    ASSERT(!conn->client && !conn->proxy);

    msg = msg_get(conn, false, conn->redis);
    if (msg == NULL) {
        conn->err = errno;
    }

    return msg;
}

void
rsp_put(struct msg *msg)
{
    ASSERT(!msg->request);
    ASSERT(msg->peer == NULL);
    msg_put(msg);
}

static struct msg *
rsp_make_error(struct context *ctx, struct conn *conn, struct msg *msg)
{
    struct msg *pmsg;        /* peer message (response) */
    struct msg *cmsg, *nmsg; /* current and next message (request) */
    uint64_t id;
    err_t err;

    ASSERT(conn->client && !conn->proxy);
    ASSERT(msg->request && req_error(conn, msg));
    ASSERT(msg->owner == conn);

    id = msg->frag_id;
    if (id != 0) {
        for (err = 0, cmsg = TAILQ_NEXT(msg, c_tqe);
             cmsg != NULL && cmsg->frag_id == id;
             cmsg = nmsg) {
            nmsg = TAILQ_NEXT(cmsg, c_tqe);

            /* dequeue request (error fragment) from client outq */
            conn->dequeue_outq(ctx, conn, cmsg);
            if (err == 0 && cmsg->err != 0) {
                err = cmsg->err;
            }

            req_put(cmsg);
        }
    } else {
        err = msg->err;
    }

    pmsg = msg->peer;
    if (pmsg != NULL) {
        ASSERT(!pmsg->request && pmsg->peer == msg);
        msg->peer = NULL;
        pmsg->peer = NULL;
        rsp_put(pmsg);
    }

    return msg_get_error(conn->redis, err);
}

static void 
set_migrating_flag(struct msg* msg, struct conn *conn)
{
    struct msg *pmsg = TAILQ_FIRST(&conn->omsg_q);
    if(pmsg != NULL && pmsg->migrate_wrapper){
        msg->migrate_wrapper = 1;
    }
    else{
        msg->migrate_wrapper = 0;
    }
}

struct msg *
rsp_recv_next(struct context *ctx, struct conn *conn, bool alloc)
{
    struct msg *msg;

    ASSERT(!conn->client && !conn->proxy);

    if (conn->eof) {
        msg = conn->rmsg;

        /* server sent eof before sending the entire request */
        if (msg != NULL) {
            conn->rmsg = NULL;

            ASSERT(msg->peer == NULL);
            ASSERT(!msg->request);

            log_error("eof s %d discarding incomplete rsp %"PRIu64" len "
                      "%"PRIu32"", conn->sd, msg->id, msg->mlen);

            rsp_put(msg);
        }

        /*
         * We treat TCP half-close from a server different from how we treat
         * those from a client. On a FIN from a server, we close the connection
         * immediately by sending the second FIN even if there were outstanding
         * or pending requests. This is actually a tricky part in the FA, as
         * we don't expect this to happen unless the server is misbehaving or
         * it crashes
         */
        conn->done = 1;
        log_error("s %d active %d is done", conn->sd, conn->active(conn));

        return NULL;
    }

    msg = conn->rmsg;
    if (msg != NULL) {
        ASSERT(!msg->request);
        set_migrating_flag(msg, conn);
        return msg;
    }

    if (!alloc) {
        return NULL;
    }

    msg = rsp_get(conn);
    if (msg != NULL) {
        conn->rmsg = msg;
    }

    //add migrating tag
    set_migrating_flag(msg, conn);

    return msg;
}

static bool
rsp_filter(struct context *ctx, struct conn *conn, struct msg *msg)
{
    struct msg *pmsg;
	struct server *server = conn->owner;

    ASSERT(!conn->client && !conn->proxy);

    if (msg_empty(msg)) {
        ASSERT(conn->rmsg == NULL);
        log_debug(LOG_VERB, "filter empty rsp %"PRIu64" on s %d", msg->id,
                  conn->sd);
        rsp_put(msg);
        return true;
    }

    pmsg = TAILQ_FIRST(&conn->omsg_q);
    if (pmsg == NULL) {
        log_hexdump(LOG_ERROR, STAILQ_FIRST(&msg->mhdr)->pos,
                    mbuf_length(STAILQ_FIRST(&msg->mhdr)),
                    "filter stray rsp %"PRIu64" len %"PRIu32" on s %d",
                    msg->id, msg->mlen, conn->sd);
        rsp_put(msg);

        /*
         * Memcached server can respond with an error response before it has
         * received the entire request. This is most commonly seen for set
         * requests that exceed item_size_max. IMO, this behavior of memcached
         * is incorrect. The right behavior for update requests that are over
         * item_size_max would be to either:
         * - close the connection Or,
         * - read the entire item_size_max data and then send CLIENT_ERROR
         *
         * We handle this stray packet scenario in nutcracker by closing the
         * server connection which would end up sending SERVER_ERROR to all
         * clients that have requests pending on this server connection. The
         * fix is aggresive, but not doing so would lead to clients getting
         * out of sync with the server and as a result clients end up getting
         * responses that don't correspond to the right request.
         *
         * See: https://github.com/twitter/twemproxy/issues/149
         */
        conn->err = EINVAL;
        conn->done = 1;
        return true;
    }
    ASSERT(pmsg->peer == NULL);
    ASSERT(pmsg->request && !pmsg->done);

    if (pmsg->inside) {
        if (pmsg->owner->owner == ctx->sync_cache_del) {
            stats_server_incr(ctx, conn->owner, sync_responses);
        }
        if(pmsg->owner->owner == ctx->sync_write_buf){
            stats_server_incr(ctx, conn->owner, sync_wbuf_responses);
        }
        if (pmsg->type == MSG_REQ_REDIS_CLUSTER) {
            redis_cluster_rebuild(server->owner, msg);
        } else if (pmsg->type == MSG_REQ_REDIS_READONLY) {
            if (msg->type != MSG_RSP_REDIS_STATUS) {
                if (msg->type == MSG_RSP_REDIS_ERROR) {
                    log_warn("server connection readonly return error, "
                    " do server_failure, type:%d, server:%.*s", 
                    msg->type, server->pname.len, server->pname.data);
                    server_failure(ctx, conn->owner);
                }
                log_warn("server connection readonly fail, type:%d, server:%.*s", 
                msg->type, server->pname.len, server->pname.data);
                conn->readonly_slave = 0;
            }
        }
        conn->dequeue_outq(ctx, conn, pmsg);
        rsp_put(msg);
        req_put(pmsg);
        return true;
    }

    if (pmsg->swallow) {
	    int64_t timeo = pmsg->tmo_rbe.key;
        conn->dequeue_outq(ctx, conn, pmsg);
        pmsg->done = 1;
        if (pmsg->frag_owner) {
            pmsg->frag_owner->nfrag_done++;
        }

	    int64_t time = nc_usec_now() + server_timeout(conn) - timeo;
        log_info("swallow rsp %"PRIu64" len %"PRIu32" of req "
                 "%"PRIu64" on s %d s %s t %"PRId64"us", msg->id, msg->mlen, pmsg->id,
                 conn->sd, server->pname.data, time);

        stats_server_incr(ctx, conn->owner, swallows);
        rsp_put(msg);
        req_put(pmsg);
        return true;
    }

    return false;
}

static void
rsp_forward_stats(struct context *ctx, struct server *server, struct msg *msg)
{
    ASSERT(!msg->request);

    stats_server_incr(ctx, server, responses);
    stats_server_incr_by(ctx, server, response_bytes, msg->mlen);
}

static int64_t get_used_time(struct msg *rsp_msg, int64_t timeo) {
    int64_t time = 0;
    if (rsp_msg->owner) {
        time = nc_usec_now() + server_timeout(rsp_msg->owner) - timeo;
    }
    return time;
}

static void
rsp_forward_log(struct msg *req_msg, struct msg *rsp_msg, int64_t used_time)
{
    if (req_msg == NULL || rsp_msg == NULL) {
        return;
    }
    struct conn *c_conn = req_msg->owner;
    if (c_conn == NULL) {
        return;
    }

    uint32_t key_len = (uint32_t)(req_msg->key_end - req_msg->key_start);
    struct server *server = NULL;
    if (rsp_msg->owner) {
        server = rsp_msg->owner->owner;
    }
    log_access("ACCESS %s %s %.*s%s rb %"PRIu32" sb %"PRIu32" e %d s %s t %"PRId64"us",
         nc_unresolve_peer_desc(c_conn->sd), 
         msg_type_string(req_msg->type),
         (key_len < 512 ? key_len : 512), req_msg->key_start,
         (key_len > 512 ? "..." : ""),
         req_msg->mlen, rsp_msg->mlen, req_msg->error,
         server ? (const char*)server->pname.data : "", used_time);
}

static void
rsp_forward(struct context *ctx, struct conn *s_conn, struct msg *msg)
{
    rstatus_t status;
    struct msg *pmsg;
    struct conn *c_conn;

    ASSERT(!s_conn->client && !s_conn->proxy);

    if (msg->type != MSG_RSP_REDIS_ERROR) {
        /* response from server implies that server is ok and heartbeating */
        server_ok(ctx, s_conn);
    }

    /* dequeue peer message (request) from server */
    pmsg = TAILQ_FIRST(&s_conn->omsg_q);
    ASSERT(pmsg != NULL && pmsg->peer == NULL);
    ASSERT(pmsg->request && !pmsg->done);

    s_conn->dequeue_outq(ctx, s_conn, pmsg);
    pmsg->done = 1;
    if (pmsg->frag_owner) {
        pmsg->frag_owner->nfrag_done++;
    }

    /* establish msg <-> pmsg (response <-> request) link */
    pmsg->peer = msg;
    msg->peer = pmsg;

    msg->pre_coalesce(msg);

    c_conn = pmsg->owner;
    ASSERT(c_conn->client && !c_conn->proxy);

    if (req_done(c_conn, TAILQ_FIRST(&c_conn->omsg_q))) {
        status = event_add_out(ctx->evb, c_conn);
        if (status != NC_OK) {
            c_conn->err = errno;
        }
    }
    if (req_done(c_conn, pmsg)) {
        int64_t timeo;
        if (pmsg->frag_owner) {
            timeo = pmsg->frag_owner->tmo_rbe.key;
        } else {
            timeo = pmsg->tmo_rbe.key;
        }
        int64_t used_time = get_used_time(msg, timeo);
        // print request and response info into access log
        rsp_forward_log(pmsg, msg, used_time);
        nc_emit_timer(pmsg->type, (uint64_t)used_time);
    }
    rsp_forward_stats(ctx, s_conn->owner, msg);
}

static bool
rsp_move_or_ask(struct context *ctx, struct conn *conn, struct msg *msg)
{
    //struct mbuf *b = STAILQ_LAST(&msg->mhdr, mbuf, next);
    struct mbuf *b = STAILQ_FIRST(&msg->mhdr);
    bool moved = nc_strncmp(b->pos, "-MOVED ", 7) == 0;
    bool ask = nc_strncmp(b->pos, "-ASK ", 5) == 0;
    if (msg->type != MSG_RSP_REDIS_ERROR) {
        return false;
    }
    if (moved || ask) {
        struct msg *pmsg = TAILQ_FIRST(&conn->omsg_q);
        struct string addr;
        uint8_t *last = b->last;
        uint8_t *p = nc_strchr(b->pos, last, ' ');
        if (!p) {
            return false;
        }
        if ((p = nc_strchr(p + 1, last, ' ')) == NULL) {
            return false;
        }
        addr.data = ++p;
        if ((p = nc_strchr(p, last, '\r')) == NULL) {
            return false;
        }
        addr.len = (uint8_t)(p - addr.data);
        if (!pmsg) {
            return false;
        }
        if (++(pmsg->nredirect) > 2) {
            stats_server_incr(ctx, conn->owner, redirect_limit);
            return false;
        }
        if (moved) {
            conn->readonly_slave = 0;
            stats_server_incr(ctx, conn->owner, move_responses);
        } else {
            stats_server_incr(ctx, conn->owner, ask_responses);
        }
        conn->dequeue_outq(ctx, conn, pmsg);
        req_redirect(pmsg, ask ? ASK_REDIRECT : MOVED_REDIRECT,
                     &addr);
        rsp_put(msg);
        return true;
    }
    return false;
}

static bool
rsp_migrate_done(struct context *ctx, struct conn *conn, struct msg *msg)
{
    if(!ctx->migrating){
        return false;
    }
    struct msg *pmsg = TAILQ_FIRST(&conn->omsg_q);
    if(!pmsg->migrate_wrapper || !msg->migrate_wrapper){
        return false;
    }
    switch(msg->migrate_status){
        case RESULT_RETRY:
            log_info("access value in migrting, try again." );
            stats_server_incr(ctx, conn->owner, mig_retry_response);
            conn->dequeue_outq(ctx, conn, pmsg);
            migrate_req_redirect(pmsg, msg->migrate_status);
            rsp_put(msg);
            return true;
            break;
        case RESULT_MOVED:
            log_info("access value migrting/ moved or not exist, try the other one.");
            stats_server_incr(ctx, conn->owner, mig_move_response); 
            conn->dequeue_outq(ctx, conn, pmsg);
            migrate_req_redirect(pmsg, msg->migrate_status);
            rsp_put(msg);
            return true;
            break;
        case RESULT_OK:
            log_info("got right value. go ahead.");
            return false;
            break;
    }
    return false;
}

static uint32_t
server_get_idx(struct server *ser)
{
    uint32_t i, j;
    struct server_pool *pool;
    struct server *s, *ss;
    pool = ser->owner;
    for (i = 0; i < array_n(&pool->server); i++)
    {
        s = array_get(&pool->server, i);
        for (j = 0; j < array_n(&s->slave_pool); j++)
        {
            ss = array_get(&s->slave_pool, j);
            if (string_compare(&s->pname, &ss->pname) == 0) {
                return i;
            }
        }
    }
    return 10000;
}

static bool
rsp_double_read(struct context *ctx, struct conn *conn, struct msg *msg)
{
    if (!double_read) {
        return false;
    }
    struct msg *pmsg = TAILQ_FIRST(&conn->omsg_q);
    struct server *ser = conn->owner;
    uint32_t index;
    if (ser->slave) {
        index = server_get_idx(ser);
    } else {
        index = ser->idx;
    }
    if (index < array_n(&ctx->migrating_pool->server))
    {
        /* old server */
        log_info("recv rsp from old server slave %d index %d nnewserver %d", ser->slave, index, array_n(&ctx->migrating_pool->server));
        return false;
    }
    log_info("pmsg type %d", pmsg->type);
    switch (pmsg->type) {
        case MSG_REQ_REDIS_DEL:
        case MSG_REQ_REDIS_HDEL:
        case MSG_REQ_REDIS_ZREM:
        case MSG_REQ_REDIS_ZREMRANGEBYSCORE:
        case MSG_REQ_MC_DELETE:
            if (msg->type != MSG_RSP_MC_SERVER_ERROR && msg->type != MSG_RSP_REDIS_ERROR) { //delete success
                log_info("del new server suc, begin to del old server rsp_type %d", msg->type);
                conn->dequeue_outq(ctx, conn, pmsg);
                pool_req_redirect(pmsg, ctx->migrating_pool);
                rsp_put(msg);
            } else {
                return false;
            }
            break;
        case MSG_REQ_REDIS_GET:
        case MSG_REQ_REDIS_MGET:
        case MSG_REQ_REDIS_HGET:
        case MSG_REQ_REDIS_HMGET:      // *3$1a$-1$-1 cannot handle
        //case MSG_REQ_REDIS_ZSCORE:    // ies那边要求去掉这个命令的双读，有时不用exist，用这个命令做哨兵判断回源
        case MSG_REQ_REDIS_ZRANGEBYSCORE:
        case MSG_REQ_REDIS_ZREVRANGEBYSCORE:
        case MSG_REQ_REDIS_CGET:
            if (msg->miss) { //todo: rsp is $-1 or *0
                log_info("read new server miss, begin to read old server rsp_type %d", msg->type);
                conn->dequeue_outq(ctx, conn, pmsg);
                pool_req_redirect(pmsg, ctx->migrating_pool);
                rsp_put(msg);
            } else {
                return false;
            }
            break;
        case MSG_REQ_MC_GET:
        case MSG_REQ_MC_GETS:
            if (msg->type == MSG_RSP_MC_END) {  //todo: rsp is only END
                log_info("read new server miss, begin to read old server rsp_type %d", msg->type);
                conn->dequeue_outq(ctx, conn, pmsg);
                pool_req_redirect(pmsg, ctx->migrating_pool);
                rsp_put(msg);
            } else {
                return false;
            }
            break;
        default:
            return false;
    }
    return true;
}

void
rsp_recv_done(struct context *ctx, struct conn *conn, struct msg *msg,
              struct msg *nmsg)
{
    ASSERT(!conn->client && !conn->proxy);
    ASSERT(msg != NULL && conn->rmsg == msg);
    ASSERT(!msg->request);
    ASSERT(msg->owner == conn);
    ASSERT(nmsg == NULL || !nmsg->request);

    /* enqueue next message (response), if any */
    conn->rmsg = nmsg;
    log_info("get rsp msg msg-type %d miss: %d", msg->type, msg->miss);

    if (rsp_filter(ctx, conn, msg)) {
        return;
    }
    if (rsp_move_or_ask(ctx, conn, msg)) {
        return;
    }
    if (rsp_migrate_done(ctx, conn, msg)) {
        return;
    }
    if (rsp_double_read(ctx, conn, msg)) {
        return;
    }

    rsp_forward(ctx, conn, msg);
}

struct msg *
rsp_send_next(struct context *ctx, struct conn *conn)
{
    rstatus_t status;
    struct msg *msg, *pmsg; /* response and it's peer request */

    ASSERT(conn->client && !conn->proxy);

    pmsg = TAILQ_FIRST(&conn->omsg_q);
    if (pmsg == NULL || !req_done(conn, pmsg)) {
        /* nothing is outstanding, initiate close? */
        if (pmsg == NULL && conn->eof) {
            conn->done = 1;
            log_info("c %d is done", conn->sd);
        }

        status = event_del_out(ctx->evb, conn);
        if (status != NC_OK) {
            conn->err = errno;
        }

        return NULL;
    }

    msg = conn->smsg;
    if (msg != NULL) {
        ASSERT(!msg->request && msg->peer != NULL);
        ASSERT(req_done(conn, msg->peer));
        pmsg = TAILQ_NEXT(msg->peer, c_tqe);
    }

    if (pmsg == NULL || !req_done(conn, pmsg)) {
        conn->smsg = NULL;
        return NULL;
    }
    ASSERT(pmsg->request && !pmsg->swallow);

    if (req_error(conn, pmsg)) {
        msg = rsp_make_error(ctx, conn, pmsg);
        if (msg == NULL) {
            conn->err = errno;
            return NULL;
        }
        msg->peer = pmsg;
        pmsg->peer = msg;
        stats_pool_incr(ctx, conn->owner, forward_error);
        
        // print request and response info into access log
        rsp_forward_log(pmsg, msg, 0);
    } else {
        msg = pmsg->peer;
    }
    ASSERT(!msg->request);

    conn->smsg = msg;

    log_debug(LOG_VVERB, "send next rsp %"PRIu64" on c %d", msg->id, conn->sd);

    return msg;
}

void
rsp_send_done(struct context *ctx, struct conn *conn, struct msg *msg)
{
    struct msg *pmsg; /* peer message (request) */

    ASSERT(conn->client && !conn->proxy);
    ASSERT(conn->smsg == NULL);

    log_debug(LOG_VVERB, "send done rsp %"PRIu64" on c %d", msg->id, conn->sd);

    pmsg = msg->peer;

    ASSERT(!msg->request && pmsg->request);
    ASSERT(pmsg->peer == msg);
    ASSERT(pmsg->done && !pmsg->swallow);

    /* dequeue request from client outq */
    conn->dequeue_outq(ctx, conn, pmsg);

    req_put(pmsg);
}
