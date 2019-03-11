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

#include <stdio.h>
#include <nc_core.h>
#include <nc_server.h>
#include <hashkit/nc_hashkit.h>
#include <nc_blacklist.h>
#include <nc_slot.h>
#include <nc_redis_cluster.h>
#include <nc_conf.h>
#include <nc_migrate.h>
#include <gdpr.h>

void pool_req_redirect(struct msg *msg, struct server_pool *pool);

struct msg *
req_get(struct conn *conn)
{
    struct msg *msg;

    ASSERT(conn->client && !conn->proxy);

    msg = msg_get(conn, true, conn->redis);
    if (msg == NULL) {
        conn->err = errno;
    }

    return msg;
}

void
req_put(struct msg *msg)
{
    struct msg *pmsg; /* peer message (response) */

    ASSERT(msg->request);

    pmsg = msg->peer;
    if (pmsg != NULL) {
        ASSERT(!pmsg->request && pmsg->peer == msg);
        msg->peer = NULL;
        pmsg->peer = NULL;
        rsp_put(pmsg);
    }

    msg_tmo_delete(msg);

    msg_put(msg);
}

/*
 * Return true if request is done, false otherwise
 *
 * A request is done, if we received response for the given request.
 * A request vector is done if we received responses for all its
 * fragments.
 */
bool
req_done(struct conn *conn, struct msg *msg)
{
    struct msg *cmsg, *pmsg; /* current and previous message */
    uint64_t id;             /* fragment id */
    uint32_t nfragment;      /* # fragment */

    ASSERT(conn->client && !conn->proxy);
    ASSERT(msg->request);

    if (!msg->done) {
        return false;
    }

    id = msg->frag_id;
    if (id == 0) {
        return true;
    }

    if (msg->fdone) {
        /* request has already been marked as done */
        return true;
    }

    /* quickly check whether the all fragments are done */
    if (msg->frag_owner && msg->frag_owner->nfrag_done < msg->frag_owner->nfrag) {
        return false;
    }

    /* check all fragments of the given request vector are done */

    for (pmsg = msg, cmsg = TAILQ_PREV(msg, msg_tqh, c_tqe);
         cmsg != NULL && cmsg->frag_id == id;
         pmsg = cmsg, cmsg = TAILQ_PREV(cmsg, msg_tqh, c_tqe)) {

        if (!cmsg->done) {
            return false;
        }
    }

    for (pmsg = msg, cmsg = TAILQ_NEXT(msg, c_tqe);
         cmsg != NULL && cmsg->frag_id == id;
         pmsg = cmsg, cmsg = TAILQ_NEXT(cmsg, c_tqe)) {

        if (!cmsg->done) {
            return false;
        }
    }

    if (!pmsg->last_fragment) {
        return false;
    }

    /*
     * At this point, all the fragments including the last fragment have
     * been received.
     *
     * Mark all fragments of the given request vector to be done to speed up
     * future req_done calls for any of fragments of this request
     */

    msg->fdone = 1;
    nfragment = 1;

    for (pmsg = msg, cmsg = TAILQ_PREV(msg, msg_tqh, c_tqe);
         cmsg != NULL && cmsg->frag_id == id;
         pmsg = cmsg, cmsg = TAILQ_PREV(cmsg, msg_tqh, c_tqe)) {
        cmsg->fdone = 1;
        nfragment++;
    }

    for (pmsg = msg, cmsg = TAILQ_NEXT(msg, c_tqe);
         cmsg != NULL && cmsg->frag_id == id;
         pmsg = cmsg, cmsg = TAILQ_NEXT(cmsg, c_tqe)) {
        cmsg->fdone = 1;
        nfragment++;
    }

    ASSERT(msg->frag_owner->nfrag == nfragment);

    msg->post_coalesce(msg->frag_owner);

    log_debug(LOG_DEBUG, "req from c %d with fid %"PRIu64" and %"PRIu32" "
              "fragments is done", conn->sd, id, nfragment);

    return true;
}

/*
 * Return true if request is in error, false otherwise
 *
 * A request is in error, if there was an error in receiving response for the
 * given request. A multiget request is in error if there was an error in
 * receiving response for any its fragments.
 */
bool
req_error(struct conn *conn, struct msg *msg)
{
    struct msg *cmsg; /* current message */
    uint64_t id;
    uint32_t nfragment;

    ASSERT(msg->request && req_done(conn, msg));

    if (msg->error) {
        return true;
    }

    id = msg->frag_id;
    if (id == 0) {
        return false;
    }

    if (msg->ferror) {
        /* request has already been marked to be in error */
        return true;
    }

    /* quickly check whether the all fragments are error */
    if (msg->frag_owner && msg->frag_owner->nfrag_error == 0) {
        return false;
    }

    /* check if any of the fragments of the given request are in error */

    for (cmsg = TAILQ_PREV(msg, msg_tqh, c_tqe);
         cmsg != NULL && cmsg->frag_id == id;
         cmsg = TAILQ_PREV(cmsg, msg_tqh, c_tqe)) {

        if (cmsg->error) {
            goto ferror;
        }
    }

    for (cmsg = TAILQ_NEXT(msg, c_tqe);
         cmsg != NULL && cmsg->frag_id == id;
         cmsg = TAILQ_NEXT(cmsg, c_tqe)) {

        if (cmsg->error) {
            goto ferror;
        }
    }

    return false;

ferror:

    /*
     * Mark all fragments of the given request to be in error to speed up
     * future req_error calls for any of fragments of this request
     */

    msg->ferror = 1;
    nfragment = 1;

    for (cmsg = TAILQ_PREV(msg, msg_tqh, c_tqe);
         cmsg != NULL && cmsg->frag_id == id;
         cmsg = TAILQ_PREV(cmsg, msg_tqh, c_tqe)) {
        cmsg->ferror = 1;
        nfragment++;
    }

    for (cmsg = TAILQ_NEXT(msg, c_tqe);
         cmsg != NULL && cmsg->frag_id == id;
         cmsg = TAILQ_NEXT(cmsg, c_tqe)) {
        cmsg->ferror = 1;
        nfragment++;
    }

    log_debug(LOG_DEBUG, "req from c %d with fid %"PRIu64" and %"PRIu32" "
              "fragments is in error", conn->sd, id, nfragment);

    return true;
}

void
req_server_enqueue_imsgq(struct context *ctx, struct conn *conn, struct msg *msg)
{
    ASSERT(msg->request);
    ASSERT(!conn->client && !conn->proxy);

    /*
     * timeout clock starts ticking the instant the message is enqueued into
     * the server in_q; the clock continues to tick until it either expires
     * or the message is dequeued from the server out_q
     *
     * noreply request are free from timeouts because client is not intrested
     * in the reponse anyway!
     */
    if (!msg->noreply) {
        msg_tmo_insert(msg, conn);
    }

    TAILQ_INSERT_TAIL(&conn->imsg_q, msg, s_tqe);

    stats_server_incr(ctx, conn->owner, in_queue);
    stats_server_incr_by(ctx, conn->owner, in_queue_bytes, msg->mlen);
}

void
req_server_dequeue_imsgq(struct context *ctx, struct conn *conn, struct msg *msg)
{
    ASSERT(msg->request);
    ASSERT(!conn->client && !conn->proxy);

    TAILQ_REMOVE(&conn->imsg_q, msg, s_tqe);

    stats_server_decr(ctx, conn->owner, in_queue);
    stats_server_decr_by(ctx, conn->owner, in_queue_bytes, msg->mlen);
}

void
req_client_enqueue_omsgq(struct context *ctx, struct conn *conn, struct msg *msg)
{
    ASSERT(msg->request);
    ASSERT(conn->client && !conn->proxy);

    TAILQ_INSERT_TAIL(&conn->omsg_q, msg, c_tqe);
}

void
req_server_enqueue_omsgq(struct context *ctx, struct conn *conn, struct msg *msg)
{
    ASSERT(msg->request);
    ASSERT(!conn->client && !conn->proxy);

    TAILQ_INSERT_TAIL(&conn->omsg_q, msg, s_tqe);

    stats_server_incr(ctx, conn->owner, out_queue);
    stats_server_incr_by(ctx, conn->owner, out_queue_bytes, msg->mlen);
}

void
req_client_dequeue_omsgq(struct context *ctx, struct conn *conn, struct msg *msg)
{
    ASSERT(msg->request);
    ASSERT(conn->client && !conn->proxy);

    TAILQ_REMOVE(&conn->omsg_q, msg, c_tqe);
}

void
req_server_dequeue_omsgq(struct context *ctx, struct conn *conn, struct msg *msg)
{
    ASSERT(msg->request);
    ASSERT(!conn->client && !conn->proxy);

    msg_tmo_delete(msg);

    TAILQ_REMOVE(&conn->omsg_q, msg, s_tqe);

    stats_server_decr(ctx, conn->owner, out_queue);
    stats_server_decr_by(ctx, conn->owner, out_queue_bytes, msg->mlen);
}

struct msg *
req_recv_next(struct context *ctx, struct conn *conn, bool alloc)
{
    struct msg *msg;

    ASSERT(conn->client && !conn->proxy);

    if (conn->eof) {
        msg = conn->rmsg;

        /* client sent eof before sending the entire request */
        if (msg != NULL) {
            conn->rmsg = NULL;

            ASSERT(msg->peer == NULL);
            ASSERT(msg->request && !msg->done);

            log_error("eof c %d discarding incomplete req %"PRIu64" len "
                      "%"PRIu32"", conn->sd, msg->id, msg->mlen);

            req_put(msg);
        }

        /*
         * TCP half-close enables the client to terminate its half of the
         * connection (i.e. the client no longer sends data), but it still
         * is able to receive data from the proxy. The proxy closes its
         * half (by sending the second FIN) when the client has no
         * outstanding requests
         */
        if (ctx->no_async || !conn->active(conn)) {
            conn->done = 1;
            log_info("c %d is done", conn->sd);
        }
        return NULL;
    }

    msg = conn->rmsg;
    if (msg != NULL) {
        ASSERT(msg->request);
        return msg;
    }

    if (!alloc) {
        return NULL;
    }

    msg = req_get(conn);
    if (msg != NULL) {
        conn->rmsg = msg;
    }

    return msg;
}

#define key_equal_cmd(cmd, key, keylen) (sizeof(cmd)==keylen+1&&nc_strncmp(cmd, key, keylen)==0)
static void
req_sync_cache_del_rsp(struct context *ctx, struct conn *conn, struct msg *msg)
{
    const char *key = (const char*)msg->key_start;
    size_t keylen = (size_t)(msg->key_end - msg->key_start);
    int type = msg->redis ? MSG_RSP_REDIS_STATUS : MSG_RSP_MC_VALUE;
    const char *ret = msg->redis ? "+OK\r\n" : "OK\r\n";
    struct msg *rmsg = NULL;
    rstatus_t status;

    ASSERT(msg->type == MSG_REQ_REDIS_SYNCCACHEDEL ||
           msg->type == MSG_REQ_MC_SYNCCACHEDEL);

    if (key_equal_cmd("enable", key, keylen)) {
        conn->sync_cache_del = 1;
    } else if (key_equal_cmd("disable", key, keylen)) {
        conn->sync_cache_del = 0;
    } else if (key_equal_cmd("status", key, keylen)) {
        if (msg->redis) {
            ret = conn->sync_cache_del ? "+yes\r\n" : "+no\r\n";
        } else {
            ret = conn->sync_cache_del ? "YES\r\n" : "NO\r\n";
        }
    } else {
        if (msg->redis) {
            type = MSG_RSP_REDIS_ERROR;
            ret = "-ERR unknown synccachedel argument\r\n";
        } else {
            type = MSG_RSP_MC_CLIENT_ERROR;
            ret = "CLIENT_ERROR unknown synccachedel argument\r\n";
        }
    }
    rmsg = msg_get(NULL, false, true);
    if (!rmsg) {
        conn->err = errno;
        return;
    }
    rmsg->type = type;
    if (!mbuf_append(&rmsg->mhdr, ret, strlen(ret))) {
        conn->err = errno;
        msg_put(rmsg);
        return;
    }

    conn->enqueue_outq(ctx, conn, msg);
    msg->done = 1;
    msg->peer = rmsg;
    rmsg->peer = msg;

    if (req_done(conn, TAILQ_FIRST(&conn->omsg_q))) {
        status = event_add_out(ctx->evb, conn);
        if (status != NC_OK) {
            conn->err = errno;
        }
    }
}

static void
req_sync_store_write_rsp(struct context *ctx, struct conn *conn, struct msg *msg)
{
    int type = MSG_RSP_REDIS_STATUS;
    char ret[64] = "+OK\r\n";
    struct msg *rmsg = NULL;
    rstatus_t status;
    struct string value;

    string_init(&value);
    value.len = (size_t)(msg->key_end - msg->key_start);
    value.data = (const char*)msg->key_start;
    ASSERT(msg->type == MSG_REQ_REDIS_SYNCSTOREWRITE);

    if (key_equal_cmd("status", value.data, value.len)) {
        if (conn->sync_store_write) {
            sprintf(ret, "+yes\r\n");
        } else {
            sprintf(ret, "+no\r\n");
        }
    } else if (key_equal_cmd("enable", value.data, value.len)) {
        conn->sync_store_write = 1;
    } else if (key_equal_cmd("disable", value.data, value.len)) {
        conn->sync_store_write = 0;
    } else {
        type = MSG_RSP_REDIS_ERROR;
        sprintf(ret, "-ERR unknown syncstorewrite argument\r\n");
    }
    rmsg = msg_get(NULL, false, true);
    if (!rmsg) {
        conn->err = errno;
        return;
    }
    rmsg->type = type;
    if (!mbuf_append(&rmsg->mhdr, ret, strlen(ret))) {
        conn->err = errno;
        msg_put(rmsg);
        return;
    }

    conn->enqueue_outq(ctx, conn, msg);
    msg->done = 1;
    msg->peer = rmsg;
    rmsg->peer = msg;

    if (req_done(conn, TAILQ_FIRST(&conn->omsg_q))) {
        status = event_add_out(ctx->evb, conn);
        if (status != NC_OK) {
            conn->err = errno;
        }
    }
}

static void
req_double_cluster_mode_rsp(struct context *ctx, struct conn *conn, struct msg *msg)
{
    int type = MSG_RSP_REDIS_STATUS;
    char ret[64] = "+OK\r\n";
    struct msg *rmsg = NULL;
    rstatus_t status;
    struct string value;

    string_init(&value);
    value.len = (size_t)(msg->key_end - msg->key_start);
    value.data = (const char*)msg->key_start;
    ASSERT(msg->type == MSG_REQ_REDIS_DOUBLECLUSTERMODE);

	/* deal request */
    if (key_equal_cmd("status", value.data, value.len)) {
        snprintf(ret, sizeof(ret), "+%d\r\n", ctx->double_cluster_mode);
    } else {
        type = set_double_cluster_mode(ctx, value);
    }
    if (type != MSG_RSP_REDIS_STATUS) {
        snprintf(ret, sizeof(ret), "-ERR unknown synccachedel argument\r\n");
    }

    /* make response */
    rmsg = msg_get(NULL, false, true);
    if (!rmsg) {
        conn->err = errno;
        return;
    }
    rmsg->type = type;
    if (!mbuf_append(&rmsg->mhdr, ret, strlen(ret))) {
        conn->err = errno;
        msg_put(rmsg);
        return;
    }

    conn->enqueue_outq(ctx, conn, msg);
    msg->done = 1;
    msg->peer = rmsg;
    rmsg->peer = msg;

    if (req_done(conn, TAILQ_FIRST(&conn->omsg_q))) {
        status = event_add_out(ctx->evb, conn);
        if (status != NC_OK) {
            conn->err = errno;
        }
    }
}

static inline void req_deliver_response(struct context *ctx, struct conn *conn,
										struct msg *msg, int type,
										const struct string *rsp_str) {
	struct msg *rmsg = NULL;
	rmsg = msg_get(NULL, false, true);
	if (!rmsg) {
		conn->err = errno;
		return;
	}
	rmsg->type = type;
	if (!mbuf_append(&rmsg->mhdr, (char *)rsp_str->data, rsp_str->len)) {
		conn->err = errno;
		msg_put(rmsg);
		return;
	}
	conn->enqueue_outq(ctx, conn, msg);
	msg->done = 1;
	msg->peer = rmsg;
	rmsg->peer = msg;
	if (req_done(conn, TAILQ_FIRST(&conn->omsg_q))) {
		if (event_add_out(ctx->evb, conn) != NC_OK) {
			conn->err = errno;
		}
	}
}

static bool
req_filter(struct context *ctx, struct conn *conn, struct msg *msg)
{
    ASSERT(conn->client && !conn->proxy);

    if (msg_empty(msg)) {
        ASSERT(conn->rmsg == NULL);
        log_debug(LOG_VERB, "filter empty req %"PRIu64" from c %d", msg->id,
                  conn->sd);
        req_put(msg);
        return true;
    }

    /*
     * Handle "quit\r\n", which is the protocol way of doing a
     * passive close
     */
    if (msg->quit) {
        ASSERT(conn->rmsg == NULL);
        log_info("filter quit req %"PRIu64" from c %d", msg->id, conn->sd);
        conn->eof = 1;
        conn->recv_ready = 0;
        req_put(msg);
        return true;
    }

    if (msg->type == MSG_REQ_REDIS_SYNCCACHEDEL ||
        msg->type == MSG_REQ_MC_SYNCCACHEDEL) {
        req_sync_cache_del_rsp(ctx, conn, msg);
        return true;
    }

    if (msg->type == MSG_REQ_REDIS_SYNCSTOREWRITE) {
        req_sync_store_write_rsp(ctx, conn, msg);
        return true;
    }

    if (msg->type == MSG_REQ_REDIS_DOUBLECLUSTERMODE) {
        req_double_cluster_mode_rsp(ctx, conn, msg);
        return true;
    }
	if (msg->type == MSG_REQ_REDIS_DPSINFO) {
		req_deliver_response(ctx, conn, msg, MSG_RSP_REDIS_MULTIBULK,
							 gdpr_info_raw_response(&ctx->cf->gdpr));
		return true;
	}
	if (msg->type == MSG_REQ_REDIS_DPSAUTH) {
		const gdpr_auth_msg_t *auth_msg =
			gdpr_auth_check(ctx->cf, msg, conn->sd);
		conn->auth_state = auth_msg->result;
		req_deliver_response(ctx, conn, msg,
							 auth_msg->result == GDPR_AUTH_FAIL
								 ? MSG_RSP_REDIS_ERROR
								 : MSG_RSP_REDIS_STATUS,
							 &auth_msg->reason);
		return true;
	}
	if (conn->auth_state == GDPR_AUTH_FAIL) {
		if (ctx->cf->gdpr.mode == GDPR_GRANT_STRICT) {
			req_deliver_response(ctx, conn, msg, MSG_RSP_REDIS_ERROR,
								 gdpr_require_auth_raw_response());
			return true;
		}
		conn->auth_state = GDPR_AUTH_SUCC;
		gdpr_add_peer_client(nc_unresolve_peer_desc(conn->sd), 'f');
		return false;
	}

	return false;
}

static void
req_forward_error(struct context *ctx, struct conn *conn, struct msg *msg)
{
    rstatus_t status;

    ASSERT(conn->client && !conn->proxy);

    log_info("forward req %"PRIu64" len %"PRIu32" type %d from "
             "c %d failed: %s", msg->id, msg->mlen, msg->type, conn->sd,
             strerror(errno));

    msg->done = 1;
    if (msg->frag_owner) {
        msg->frag_owner->nfrag_done++;
        msg->frag_owner->nfrag_error++;
    }

    msg->error = 1;
    msg->err = errno;

    /* noreply request don't expect any response */
    if (msg->noreply || msg->inside) {
        req_put(msg);
        return;
    }

    if (req_done(conn, TAILQ_FIRST(&conn->omsg_q))) {
        status = event_add_out(ctx->evb, conn);
        if (status != NC_OK) {
            conn->err = errno;
        }
    }
}

static void
req_forward_stats(struct context *ctx, struct server *server, struct msg *msg)
{
    ASSERT(msg->request);

    if (msg->inside) {
        if (ctx->sync_cache_del == msg->owner->owner) {
            stats_server_incr(ctx, server, sync_requests);
        }else if (ctx->sync_write_buf == msg->owner->owner){
            stats_server_incr(ctx, server, sync_wbuf_requests);
        }
        switch (msg->type) {
        case MSG_REQ_REDIS_CLUSTER:
            stats_server_incr(ctx, server, cluster_requests);
            break;
        case MSG_REQ_REDIS_ASKING:
            stats_server_incr(ctx, server, asking_requests);
            break;
        case MSG_REQ_REDIS_READONLY:
            stats_server_incr(ctx, server, readonly_requests);
            break;
        default:
            break;
        }
    } else {
        stats_server_incr(ctx, server, requests);
        stats_server_incr_by(ctx, server, request_bytes, msg->mlen);
    }
}

static void
req_response(struct context *ctx, struct conn *conn, struct msg *req,
             struct msg *rsp)
{
    rstatus_t status;

    req->done = 1;
    if (req->frag_owner) {
        req->frag_owner->nfrag_done++;
    }
    req->peer = rsp;
    rsp->peer = req;

    rsp->pre_coalesce(rsp);

    if (req_done(conn, TAILQ_FIRST(&conn->omsg_q))) {
        status = event_add_out(ctx->evb, conn);
        if (status != NC_OK) {
            conn->err = errno;
        }
    }
}

static void
req_forward(struct context *ctx, struct conn *c_conn, struct msg *msg)
{
    rstatus_t status;
    struct conn *s_conn;
    struct server_pool *pool;
    uint8_t *key;
    uint32_t keylen;
    struct msg *readonly = NULL;

    ASSERT(c_conn->client && !c_conn->proxy);

    /* enqueue message (request) into client outq, if response is expected */
    if (!msg->noreply && !msg->inside) {
        c_conn->enqueue_outq(ctx, c_conn, msg);
    }

    pool = c_conn->owner;
    key = NULL;
    keylen = 0;

    /*
     * If hash_tag: is configured for this server pool, we use the part of
     * the key within the hash tag as an input to the distributor. Otherwise
     * we use the full key
     */
    if (!string_empty(&pool->hash_tag)) {
        struct string *tag = &pool->hash_tag;
        uint8_t *tag_start, *tag_end;

        tag_start = nc_strchr(msg->key_start, msg->key_end, tag->data[0]);
        if (tag_start != NULL) {
            tag_end = nc_strchr(tag_start + 1, msg->key_end, tag->data[1]);
            if (tag_end != NULL) {
                key = tag_start + 1;
                keylen = (uint32_t)(tag_end - key);
            }
        }
    }

    if (keylen == 0) {
        key = msg->key_start;
        keylen = (uint32_t)(msg->key_end - msg->key_start);
    }

    if ((msg->type == MSG_REQ_MC_GET || msg->type == MSG_REQ_MC_GETS) &&
        blacklist_has(key, keylen)) {
        struct msg *rsp;

        if (msg->noreply) {
            return;
        }

        rsp = msg_get_empty((const char*)key, keylen);
        if (!rsp) {
            req_forward_error(ctx, c_conn, msg);
            return;
        }
        req_response(ctx, c_conn, msg, rsp);

        return;
    }

    /*
     * connection hint: c_conn->sd maybe biased after server connection error.
     * so we use a random value before we have a better idea.
     */
    struct server_pool * active_pool = get_access_pool(ctx, c_conn, &key, keylen, msg);
    s_conn = server_pool_conn(ctx, active_pool, key, keylen,
                              msg_type_is_read(msg->type), (uint32_t)rand(),
                              c_conn);
    if (s_conn == NULL) {
        req_forward_error(ctx, c_conn, msg);
        return;
    }
    ASSERT(!s_conn->client && !s_conn->proxy);
    if (pool->dist_type == DIST_REDIS_CLUSTER) {
        struct server *server = s_conn->owner;
        struct mbuf *buf;
        if (server->slave && !s_conn->readonly_slave) {
            readonly = msg_get(c_conn, true, true);
            if (!readonly) {
                req_forward_error(ctx, c_conn, msg);
                return;
            }
            readonly->type = MSG_REQ_REDIS_READONLY;
            readonly->inside = 1;
            buf = mbuf_append(&readonly->mhdr, REDIS_CLUSTER_READONLY,
                              REDIS_CLUSTER_READONLY_LEN);
            if (!buf) {
                msg_put(readonly);
                req_forward_error(ctx, c_conn, msg);
                return;
            }
            readonly->mlen = mbuf_length(buf);
        }
    }

    /* enqueue the message (request) into server inq */
    if (TAILQ_EMPTY(&s_conn->imsg_q)) {
        status = event_add_out(ctx->evb, s_conn);
        if (status != NC_OK) {
            req_forward_error(ctx, c_conn, msg);
            s_conn->err = errno;
            return;
        }
    }
    if (readonly) {
        s_conn->enqueue_inq(ctx, s_conn, readonly);
        s_conn->readonly_slave = 1;
        req_forward_stats(ctx, s_conn->owner, readonly);
    }
    s_conn->enqueue_inq(ctx, s_conn, msg);

    req_forward_stats(ctx, s_conn->owner, msg);

    log_debug(LOG_VERB, "forward from c %d to s %d req %"PRIu64" len %"PRIu32
              " type %d with key '%.*s'", c_conn->sd, s_conn->sd, msg->id,
              msg->mlen, msg->type, keylen, key);
}


/***
 * sync queue message format
 * {"Cluster":"xxx", "Cmd":"xxx", "Args":["xxx",...], "Timestamp":xxx}
 ***/
#define SYNC_MSG_QUEUE                  "Pending"
#define SYNC_MSG_QUEUE_KEYLEN           "7" /* sizeof(SYNC_MSG_QUEUE)-1 */
#define SYNC_MSG_HDR                    "*3\r\n$5\r\nLPUSH\r\n$"SYNC_MSG_QUEUE_KEYLEN"\r\n"SYNC_MSG_QUEUE"\r\n"
#define SYNC_MSG_HDR_LEN                (sizeof(SYNC_MSG_HDR) - 1)
#define SYNC_MSG_CLUSTER_TAG            "{\"Cluster\":\""
#define SYNC_MSG_CLUSTER_TAG_LEN        (sizeof(SYNC_MSG_CLUSTER_TAG) - 1)
#define SYNC_MSG_REDIS_TAG              "\",\"Redis\":1"
#define SYNC_MSG_REDIS_TAG_LEN          (sizeof(SYNC_MSG_REDIS_TAG) - 1)
#define SYNC_MSG_MC_TAG                 "\",\"Redis\":0"
#define SYNC_MSG_MC_TAG_LEN             (sizeof(SYNC_MSG_MC_TAG) - 1)
#define SYNC_MSG_CMD_TAG                ",\"Cmd\":\""
#define SYNC_MSG_CMD_TAG_LEN            (sizeof(SYNC_MSG_CMD_TAG) - 1)
#define SYNC_MSG_ARGS_TAG               "\",\"Args\":["
#define SYNC_MSG_ARGS_TAG_LEN           (sizeof(SYNC_MSG_ARGS_TAG) - 1)
#define SYNC_MSG_TS_TAG                 "],\"Timestamp\":"
#define SYNC_MSG_TS_TAG_LEN             (sizeof(SYNC_MSG_TS_TAG) - 1)
#define SYNC_MSG_END_TAG                "}\r\n"
#define SYNC_MSG_END_TAG_LEN            (sizeof(SYNC_MSG_END_TAG) - 1)
static struct mbuf*
msg_append_args(struct msg* dst, struct msg* src, int first)
{
    struct mbuf *buf = NULL;
    if (src->type == MSG_REQ_MC_DELETE || src->type == MSG_REQ_REDIS_DEL) {
        if (first) {
            if (!mbuf_append(&dst->mhdr, "\"", 1)) {
                return NULL;
            }
            first = 0;
        } else if (!mbuf_append(&dst->mhdr, ",\"", 2)) {
            return NULL;
        }
        if (!mbuf_append_json(&dst->mhdr, (const char*)src->key_start,
                    (size_t)(src->key_end - src->key_start))) {
            return NULL;
        }
        buf = mbuf_append(&dst->mhdr, "\"", 1);
    } else if (src->redis) {
        char* s = (char*)src->narg_start;
        int n = atoi(s + 1);
        int i = 0;
        char* p = strstr(s, "\r\n$");
        int len = 0;
        if (!p) {
            return NULL;
        }
        s = p + 3;
        for (i = 0; i < n; ++i) {
            len = atoi(s);
            if (len <= 0) {
                return NULL;
            }
            p = strstr(s, "\r\n");
            if (!p) {
                return NULL;
            }
            s = p + 2;
            if (i > 0) {
                if (first) {
                    if (!mbuf_append(&dst->mhdr, "\"", 1)) {
                        return NULL;
                    }
                    first = 0;
                } else if (!mbuf_append(&dst->mhdr, ",\"", 2)) {
                    return NULL;
                }
                if (!mbuf_append_json(&dst->mhdr, s, (size_t)len)) {
                    return NULL;
                }
                if ((buf = mbuf_append(&dst->mhdr, "\"", 1)) == NULL) {
                    return NULL;
                }
            }
            s += len + 3;
        }
    }
    return buf;
}

static bool 
is_write_command(struct msg *msg)
{
    switch (msg->type) {
        case MSG_REQ_REDIS_DEL:
        case MSG_REQ_REDIS_EXPIRE:
        case MSG_REQ_REDIS_EXPIREAT:
        case MSG_REQ_REDIS_PEXPIRE:
        case MSG_REQ_REDIS_PEXPIREAT:
        case MSG_REQ_REDIS_PERSIST:
        case MSG_REQ_REDIS_APPEND:                 /* redis requests - string */
        case MSG_REQ_REDIS_DECR:
        case MSG_REQ_REDIS_DECRBY:
        case MSG_REQ_REDIS_GETSET:
        case MSG_REQ_REDIS_INCR:
        case MSG_REQ_REDIS_INCRBY:
        case MSG_REQ_REDIS_INCRBYFLOAT:
        case MSG_REQ_REDIS_PSETEX:
        case MSG_REQ_REDIS_SET:
        case MSG_REQ_REDIS_SETBIT:
        case MSG_REQ_REDIS_SETEX:
        case MSG_REQ_REDIS_SETNX:
        case MSG_REQ_REDIS_SETRANGE:
        case MSG_REQ_REDIS_STRLEN:
        case MSG_REQ_REDIS_HDEL:                   /* redis requests - hashes */
        case MSG_REQ_REDIS_HINCRBY:
        case MSG_REQ_REDIS_HINCRBYFLOAT:
        case MSG_REQ_REDIS_HMSET:
        case MSG_REQ_REDIS_HSET:
        case MSG_REQ_REDIS_HSETNX:                /* redis requests - lists */
        case MSG_REQ_REDIS_LINSERT:
        case MSG_REQ_REDIS_LPOP:
        case MSG_REQ_REDIS_LPUSH:
        case MSG_REQ_REDIS_LPUSHX:
        case MSG_REQ_REDIS_LREM:
        case MSG_REQ_REDIS_LSET:
        case MSG_REQ_REDIS_LTRIM:
        case MSG_REQ_REDIS_RPOP:
        case MSG_REQ_REDIS_RPOPLPUSH:
        case MSG_REQ_REDIS_RPUSH:
        case MSG_REQ_REDIS_RPUSHX:
        case MSG_REQ_REDIS_SADD:                   /* redis requests - sets */
        case MSG_REQ_REDIS_SDIFFSTORE:
        case MSG_REQ_REDIS_SINTERSTORE:
        case MSG_REQ_REDIS_SMOVE:
        case MSG_REQ_REDIS_SPOP:
        case MSG_REQ_REDIS_SREM:
        case MSG_REQ_REDIS_SUNIONSTORE:
        case MSG_REQ_REDIS_ZADD:                 /* redis requests - sorted sets */
        case MSG_REQ_REDIS_ZINCRBY:
        case MSG_REQ_REDIS_ZINTERSTORE:
        case MSG_REQ_REDIS_ZREM:
        case MSG_REQ_REDIS_ZREMRANGEBYRANK:
        case MSG_REQ_REDIS_ZREMRANGEBYSCORE:
        case MSG_REQ_REDIS_ZREVRANK:
        case MSG_REQ_REDIS_ZUNIONSTORE:
        case MSG_REQ_REDIS_CSET:
        case MSG_REQ_REDIS_CINCR:
        case MSG_REQ_REDIS_CINCRBY:
        case MSG_REQ_REDIS_CDECR:
        case MSG_REQ_REDIS_CDECRBY:
            return true;
        default:
            return false;
    }
    return false;
}

static void
collpse_fragment(struct msg *dst,  struct msg *pmsg, struct msg *src)
{
    if(pmsg->frag_id == 0){
        return;
    }
    if (pmsg && pmsg->frag_id != src->frag_id) {
        return;
    }

	if (pmsg->first_fragment){
        struct mbuf *msg_buf;
        bool first = true;
        STAILQ_FOREACH(msg_buf, &pmsg->mhdr, next) {
            uint8_t *p, *q;
            long int len;
			// the first frament msg has two char need rm (*2) eg:
			// cmd:del aa bb cccc
			// will be split to 3 msg:
			//*2*4\r\n$3\r\ndel\r\n$2\r\naa\r\n (first fragment)
			//*2*3\r\n$3\r\ndel\r\n$2\r\nbb\r\n
			//$3\r\ndel\r\n$3\r\nccc\r\n  (last fragment)
			if (first){
                p = msg_buf->start +2;
                first = false;
            }else{
                p = msg_buf->start;
            }
            q = msg_buf->last;
            len = q - p;
			mbuf_append(&dst->mhdr, (const char *)p, len);
		}
        return;
    }

    struct msg * stack_msg;
    stack_msg = pmsg;
	pmsg = TAILQ_PREV(pmsg, msg_tqh, c_tqe);
	//Recursively call: collpse from the first segment
    collpse_fragment(dst, pmsg, src);

    size_t key_len = (size_t)(stack_msg->key_end - stack_msg->key_start);
    char sizebuf[32];
    int len;
    len = snprintf(sizebuf, sizeof(sizebuf), "$%d\r\n", key_len);
	mbuf_append(&dst->mhdr, sizebuf, strlen(sizebuf));
	// +2 means add \r\n
    mbuf_append(&dst->mhdr, (const char*)stack_msg->key_start, (size_t)(stack_msg->key_end - stack_msg->key_start)+2);
}

/**
 * write commands sync to write buffer(implement by redis list) 
 * redis List key name: redis_write_buffer
 * body:
 * {Body": "*3\r\n...", Timestamp:"123843433"}
 * *3\r\n$5\r\nLPUSH\r\n$18\r\nredis_write_buffer\r\n$(command_len)\r\n(command_content)\r\n
 */
#define SYNC_WRITE_BUF_HDR                    "*3\r\n$5\r\nLPUSH\r\n$"
#define SYNC_WRITE_BUF_HDR_LEN                (sizeof(SYNC_WRITE_BUF_HDR) - 1)
#define SYNC_WRITE_BUF_BODY_TAG               "{\"Body\":\""
#define SYNC_WRITE_BUF_BODY_TAG_LEN           (sizeof(SYNC_WRITE_BUF_BODY_TAG) - 1)
#define SYNC_WRITE_BUF_TS                     "\",\"Timestamp\":"
#define SYNC_WRITE_BUF_TS_LEN                 (sizeof(SYNC_WRITE_BUF_TS) - 1)
#define SYNC_WRITE_BUF_END_TAG                "}\r\n"
#define SYNC_WRITE_BUF_END_TAG_LEN            (sizeof(SYNC_WRITE_BUF_END_TAG) - 1)
#define SYNC_WRITE_BUF_LINE_BREAK             "\r\n"
#define SYNC_WRITE_BUF_LINE_BREAK_LEN         (sizeof(SYNC_WRITE_BUF_LINE_BREAK) - 1)

static void
req_sync_write_buf(struct context *ctx, struct conn *c_conn, struct msg *msg)
{
    struct conn *buf_conn;
    struct msg *nmsg;// *pmsg;
    struct mbuf *header_buf, *copy_write_buf;
    struct server_pool *pool = (struct server_pool*)(c_conn->owner);

    ASSERT(c_conn->client && !c_conn->proxy);

    //同步过来的请求，无需再同步
    if (!c_conn->sync_store_write) {
        return;
    }

    if (!ctx->sync_write_buf) {
        return;
    }
    if (!is_write_command(msg)){
        return;
    }
    if (msg->frag_id != 0 && !msg->last_fragment) {
        return;
    }
    buf_conn = TAILQ_FIRST(&ctx->sync_write_buf->c_conn_q);
    ASSERT(buf_conn);
    nmsg = msg_get(buf_conn, msg->request, true);
    if (!nmsg) {
        return;
    }

    // add msg header
    nmsg->type = MSG_REQ_REDIS_LPUSH;
    nmsg->inside = 1;
    header_buf = mbuf_append(&nmsg->mhdr, SYNC_WRITE_BUF_HDR, SYNC_WRITE_BUF_HDR_LEN);
    if(!header_buf) {
        goto NOMEM_FOR_WRITE_BUF;
    }
    //add keys
    // 保证同连接命令有序
    if (c_conn->write_queue_no < 0) {
        c_conn->write_queue_no = (nc_usec_now() + c_conn->sd) % pool->sync_write_queue_num;
        log_warn("conn %d init write_queue_no %d", c_conn->sd, c_conn->write_queue_no);
        log_warn("%.*s#%.*s#%d", pool->name.len, pool->name.data, pool->addrstr.len, pool->addrstr.data, c_conn->write_queue_no);
    }
    char key_str[512] = {0};
    sprintf(key_str, "%.*s#%.*s#%d", pool->name.len, pool->name.data, pool->addrstr.len, pool->addrstr.data, c_conn->write_queue_no);
    char key_len_str[10] = {0};
    sprintf(key_len_str, "%d", strlen(key_str));
    if (!mbuf_append(&nmsg->mhdr, key_len_str, strlen(key_len_str))) {
        goto NOMEM_FOR_WRITE_BUF;
    }
    if (!mbuf_append(&nmsg->mhdr, SYNC_WRITE_BUF_LINE_BREAK, SYNC_WRITE_BUF_LINE_BREAK_LEN)) {
        goto NOMEM_FOR_WRITE_BUF;
    }
    if (!mbuf_append(&nmsg->mhdr, key_str, strlen(key_str))) {
        goto NOMEM_FOR_WRITE_BUF;
    }
    if (!mbuf_append(&nmsg->mhdr, SYNC_WRITE_BUF_LINE_BREAK, SYNC_WRITE_BUF_LINE_BREAK_LEN)) {
        goto NOMEM_FOR_WRITE_BUF;
    }
    nmsg->key_start = (uint8_t*)strstr((char*)header_buf->start, key_str);
    nmsg->key_end = nmsg->key_start + strlen(key_str);

    //add body(write commands)
    copy_write_buf = mbuf_get();
    if (!copy_write_buf) {
        goto NOMEM_FOR_WRITE_BUF;
    }
    mbuf_insert(&nmsg->mhdr, copy_write_buf);
    if (!mbuf_append(&nmsg->mhdr, SYNC_WRITE_BUF_BODY_TAG, SYNC_WRITE_BUF_BODY_TAG_LEN)) {
        goto NOMEM_FOR_WRITE_BUF;
    }

	struct mbuf *msg_buf;
    if(msg->frag_id != 0){
        if (msg->type != MSG_REQ_REDIS_DEL){
            log_error("not support coalesce msg type.");
            return;
        }
        collpse_fragment(nmsg, msg, msg);
    }else{
        STAILQ_FOREACH(msg_buf, &msg->mhdr, next) {
            uint8_t *p, *q;
            long int len;
            p = msg_buf->start;
            q = msg_buf->last;
            len = q - p;
            mbuf_append(&nmsg->mhdr, (const char*)p, len);
        }
    }
    // add timestamp
    if (mbuf_append(&nmsg->mhdr, SYNC_WRITE_BUF_TS, SYNC_WRITE_BUF_TS_LEN)) {
        int64_t now = nc_usec_now() / 1000000;
        int len;
        char buf[32];
        len = snprintf(buf, sizeof(buf), "%ld", now);
        if (!mbuf_append(&nmsg->mhdr, buf, (size_t)len)) {
            goto NOMEM_FOR_WRITE_BUF;
        }
    } else {
        goto NOMEM_FOR_WRITE_BUF;
    }
    // add end tag
    if (!mbuf_append(&nmsg->mhdr, SYNC_WRITE_BUF_END_TAG, SYNC_WRITE_BUF_END_TAG_LEN)) {
        goto NOMEM_FOR_WRITE_BUF;
    }

    // add body length (redis protocal)
    nmsg->mlen = mbuf_length(copy_write_buf);
    while  ((copy_write_buf =  STAILQ_NEXT(copy_write_buf, next)) != NULL) {
        nmsg->mlen += mbuf_length(copy_write_buf);
    }
    char sizebuf[32];
    int len;
    len = snprintf(sizebuf, sizeof(sizebuf), "$%d\r\n", nmsg->mlen - 2);/* minus strlen(\r\n) */
    mbuf_copy(header_buf, (uint8_t*)sizebuf, (size_t)len);
    nmsg->mlen += mbuf_length(header_buf);
    req_forward(ctx, buf_conn, nmsg);
    return;
NOMEM_FOR_WRITE_BUF:
    log_notice("sync to write buffer generate msg fail");
    msg_put(nmsg);
}

static void
req_sync_double_write(struct context *ctx, struct msg *msg)
{
    struct conn * buf_conn;
    struct msg *nmsg;// *pmsg;
    struct mbuf *msg_buf;
    if (!ctx->sync_cache_peer) {
        return;
    }
    if (msg_type_is_read(msg->type)){
        return;
    }
    buf_conn = TAILQ_FIRST(&ctx->sync_cache_peer->c_conn_q);
    ASSERT(buf_conn);
    nmsg = msg_get(buf_conn, msg->request, true);
    if (!nmsg) {
        return;
    }

    // add msg header
    nmsg->type = msg->type;
    nmsg->inside = 1;
    STAILQ_FOREACH(msg_buf, &msg->mhdr, next) {
        uint8_t *p, *q;
        long int len;
        p = msg_buf->start;
        q = msg_buf->last;
        len = q - p;
        mbuf_append(&nmsg->mhdr, (const char*)p, len);
    }
    nmsg->key_start  = msg->key_start;
    nmsg->key_end = msg->key_end;
    nmsg->mlen =  msg->mlen;

    req_forward(ctx, buf_conn, nmsg);
    return;
}

static void
req_sync(struct context *ctx, struct conn *c_conn, struct msg *msg)
{
    struct conn *d_conn;
    struct server_pool *pool = c_conn->owner;
    struct msg *nmsg, *pmsg;
    struct mbuf *nbuf, *vbuf;
    const char* type_str = NULL;
    char buf[32];
    int len;
    int first = 1;

    ASSERT(c_conn->client && !c_conn->proxy);

    if (!c_conn->sync_cache_del) {
        return;
    }

    if (!ctx->sync_cache_del) {
        return;
    }

    switch (msg->type) {
        case MSG_REQ_REDIS_DEL:
        //case MSG_REQ_REDIS_HDEL:    // ies要求双删只处理del和delete的命令
        //case MSG_REQ_REDIS_SREM:
        //case MSG_REQ_REDIS_ZREM:
        case MSG_REQ_MC_DELETE:
            break;
        default:
            return;
    }
    type_str = msg_type_string(msg->type);

    if (msg->frag_id != 0 && !msg->last_fragment) {
        return;
    }
    nbuf = STAILQ_FIRST(&msg->mhdr);
    d_conn = TAILQ_FIRST(&ctx->sync_cache_del->c_conn_q);
    ASSERT(d_conn);
    nmsg = msg_get(d_conn, msg->request, true);
    if (!nmsg) {
        return;
    }

    nmsg->type = MSG_REQ_REDIS_LPUSH;
    nmsg->inside = 1;
    nbuf = mbuf_append(&nmsg->mhdr, SYNC_MSG_HDR, SYNC_MSG_HDR_LEN);
    if (!nbuf) {
        goto NOMEM_FOR_BUF;
    }
    nmsg->key_start = (uint8_t*)strstr((char*)nbuf->start, SYNC_MSG_QUEUE);
    nmsg->key_end = nmsg->key_start + 7;

    vbuf = mbuf_get();
    if (!vbuf) {
        goto NOMEM_FOR_BUF;
    }
    mbuf_insert(&nmsg->mhdr, vbuf);
    if (!mbuf_append(&nmsg->mhdr, SYNC_MSG_CLUSTER_TAG, SYNC_MSG_CLUSTER_TAG_LEN)) {
        goto NOMEM_FOR_BUF;
    }
    if (!mbuf_append_json(&nmsg->mhdr, (const char*)pool->name.data, pool->name.len)) {
        goto NOMEM_FOR_BUF;
    }
    if (msg->redis) {
        if (!mbuf_append(&nmsg->mhdr, SYNC_MSG_REDIS_TAG, SYNC_MSG_REDIS_TAG_LEN)) {
            goto NOMEM_FOR_BUF;
        }
    } else {
        if (!mbuf_append(&nmsg->mhdr, SYNC_MSG_MC_TAG, SYNC_MSG_MC_TAG_LEN)) {
            goto NOMEM_FOR_BUF;
        }
    }
    if (!mbuf_append(&nmsg->mhdr, SYNC_MSG_CMD_TAG, SYNC_MSG_CMD_TAG_LEN)) {
        goto NOMEM_FOR_BUF;
    }
    if (!mbuf_append(&nmsg->mhdr, type_str, strlen(type_str))) {
        goto NOMEM_FOR_BUF;
    }
    if (!mbuf_append(&nmsg->mhdr, SYNC_MSG_ARGS_TAG, SYNC_MSG_ARGS_TAG_LEN)) {
        goto NOMEM_FOR_BUF;
    }
    for (pmsg = msg; pmsg != NULL; ) {
        if (!msg_append_args(nmsg, pmsg, first)) {
            goto NOMEM_FOR_BUF;
        }
        first = 0;

        if (msg->frag_id != 0) {
            pmsg = TAILQ_PREV(pmsg, msg_tqh, c_tqe);
            if (pmsg && pmsg->frag_id != msg->frag_id) {
                break;
            }
        } else {
            break;
        }
    }
    if (mbuf_append(&nmsg->mhdr, SYNC_MSG_TS_TAG, SYNC_MSG_TS_TAG_LEN)) {
        int64_t now = nc_usec_now() / 1000000;
        len = snprintf(buf, sizeof(buf), "%ld", now);
        if (!mbuf_append(&nmsg->mhdr, buf, (size_t)len)) {
            goto NOMEM_FOR_BUF;
        }
    } else {
        goto NOMEM_FOR_BUF;
    }
    if (!mbuf_append(&nmsg->mhdr, SYNC_MSG_END_TAG, SYNC_MSG_END_TAG_LEN)) {
        goto NOMEM_FOR_BUF;
    }

    nmsg->mlen = mbuf_length(vbuf);
    while ((vbuf = STAILQ_NEXT(vbuf, next)) != NULL) {
        nmsg->mlen += mbuf_length(vbuf);
    }
    len = snprintf(buf, sizeof(buf), "$%d\r\n", nmsg->mlen - 2);/* minus strlen(\r\n) */
    mbuf_copy(nbuf, (uint8_t*)buf, (size_t)len);
    nmsg->mlen += mbuf_length(nbuf);

    req_forward(ctx, d_conn, nmsg);
    return;
NOMEM_FOR_BUF:
    log_notice("sync_cache_del generate msg fail");
    msg_put(nmsg);
}

void
req_recv_done(struct context *ctx, struct conn *conn, struct msg *msg,
              struct msg *nmsg)
{
    ASSERT(conn->client && !conn->proxy);
    ASSERT(msg->request);
    ASSERT(msg->owner == conn);
    ASSERT(conn->rmsg == msg);
    ASSERT(nmsg == NULL || nmsg->request);

    /* enqueue next message (request), if any */
    conn->rmsg = nmsg;

    if (req_filter(ctx, conn, msg)) {
        return;
    }

    req_forward(ctx, conn, msg);

    if (ctx->sync_cache_del) {
        req_sync(ctx, conn, msg);
    }
    if (ctx->sync_write_buf){
        req_sync_write_buf(ctx, conn, msg);
    }
    if (ctx->double_cluster_mode == MODE_CACHE){
        req_sync_double_write(ctx, msg);
    }
}

struct msg *
req_send_next(struct context *ctx, struct conn *conn)
{
    rstatus_t status;
    struct msg *msg, *nmsg; /* current and next message */

    // 意味着这个函数不是给client以及proxy用的
    ASSERT(!conn->client && !conn->proxy);

    if (conn->connecting) {
        int err = nc_get_soerror(conn->sd);
        if (err) {
            conn->err = err;
            return NULL;
        }
        server_connected(ctx, conn);
    }

    for (;;) {
        nmsg = TAILQ_FIRST(&conn->imsg_q);
        if (nmsg == NULL) {
            /* nothing to send as the server inq is empty */
            status = event_del_out(ctx->evb, conn);
            if (status != NC_OK) {
                conn->err = errno;
            }
            return NULL;
        }
        if (nmsg->swallow && !nmsg->sending && msg_type_is_read(nmsg->type)) {
            log_info("ignore swallowed req %"PRIu64" len %"PRIu32
                     " type %d on s %d", nmsg->id, nmsg->mlen, nmsg->type,
                     conn->sd);
            stats_server_incr(ctx, conn->owner, swallows);
            conn->dequeue_inq(ctx, conn, nmsg);
            req_put(nmsg);
        } else {
            break;
        }
    }

    msg = conn->smsg;
    if (msg != NULL) {
        ASSERT(msg->request && !msg->done);
        for (;;) {
            nmsg = TAILQ_NEXT(msg, s_tqe);
            if (!nmsg)
                break;
            if (nmsg->swallow && !nmsg->sending &&
                msg_type_is_read(nmsg->type)) {
                log_info("ignore swallowed req %"PRIu64
                         " len %"PRIu32" type %d on s %d", nmsg->id,
                         nmsg->mlen, nmsg->type, conn->sd);
                stats_server_incr(ctx, conn->owner, swallows);
                conn->dequeue_inq(ctx, conn, nmsg);
                req_put(nmsg);
            } else {
                break;
            }
        }
    }

    conn->smsg = nmsg;

    if (nmsg == NULL) {
        return NULL;
    }

    ASSERT(nmsg->request && !nmsg->done);

    log_debug(LOG_VVERB, "send next req %"PRIu64" len %"PRIu32" type %d on "
              "s %d", nmsg->id, nmsg->mlen, nmsg->type, conn->sd);

    return nmsg;
}

void
req_send_done(struct context *ctx, struct conn *conn, struct msg *msg)
{
    ASSERT(!conn->client && !conn->proxy);
    ASSERT(msg != NULL && conn->smsg == NULL);
    ASSERT(msg->request && !msg->done);
    ASSERT(msg->owner != conn);

    log_debug(LOG_VVERB, "send done req %"PRIu64" len %"PRIu32" type %d on "
              "s %d", msg->id, msg->mlen, msg->type, conn->sd);

    msg->sending = false;

    /* dequeue the message (request) from server inq */
    conn->dequeue_inq(ctx, conn, msg);

    /*
     * noreply request instructs the server not to send any response. So,
     * enqueue message (request) in server outq, if response is expected.
     * Otherwise, free the noreply request
     */
    if (!msg->noreply) {
        conn->enqueue_outq(ctx, conn, msg);
    } else {
        req_put(msg);
    }
}

void req_redirect(struct msg *msg, req_redirect_type_t type, const struct string *addr)
{
    struct conn *s_conn;
    struct conn *c_conn = msg->owner;
    //struct server_pool *pool = (struct server_pool*)(c_conn->owner);
    struct context *ctx = conn_to_ctx(c_conn);
    struct mbuf *buf;
    struct msg *ask = NULL;
    uint8_t *key;
    uint32_t keylen;
    bool is_read = msg_type_is_read(msg->type);
    key = msg->key_start;
    keylen = (uint32_t)(msg->key_end - msg->key_start);
    struct server_pool * active_pool = get_access_pool(ctx, c_conn, &key, keylen, msg);
    if (addr) {
        log_info("req redirect with(%d, %.*s)", type, addr->len, addr->data);
    }
    buf = STAILQ_FIRST(&msg->mhdr);
    while (buf) {
        buf->pos = buf->begin ? buf->begin : buf->start;
        buf = STAILQ_NEXT(buf, next);
    }
    if (type == MOVED_REDIRECT) {
        redis_cluster_update(active_pool);
    } else if (type == ASK_REDIRECT) {
        ask = msg_get(c_conn, true, true);
        if (!ask) {
            req_forward_error(ctx, c_conn, msg);
            return;
        }
        ask->type = MSG_REQ_REDIS_ASKING;
        ask->inside = 1;
        buf = mbuf_append(&ask->mhdr, REDIS_CLUSTER_ASKING,
                          REDIS_CLUSTER_ASKING_LEN);
        if (!buf) {
            msg_put(ask);
            req_forward_error(ctx, c_conn, msg);
            return;
        }
        ask->mlen = mbuf_length(buf);
    }
    s_conn = server_pool_redirect_conn(ctx, active_pool, addr, is_read);
    if (s_conn == NULL) {
        if (ask) {
            msg_put(ask);
        }
        errno = EAGAIN;
        req_forward_error(ctx, c_conn, msg);
        return;
    }
    ASSERT(!s_conn->client && !s_conn->proxy);

    /* enqueue the message (request) into server inq */
    if (TAILQ_EMPTY(&s_conn->imsg_q)) {
        rstatus_t status = event_add_out(ctx->evb, s_conn);
        if (status != NC_OK) {
            if (ask) {
                msg_put(ask);
            }
            req_forward_error(ctx, c_conn, msg);
            s_conn->err = errno;
            return;
        }
    }
    if (ask) {
        s_conn->enqueue_inq(ctx, s_conn, ask);
        req_forward_stats(ctx, s_conn->owner, ask);
    }
    s_conn->enqueue_inq(ctx, s_conn, msg);

    req_forward_stats(ctx, s_conn->owner, msg);

    log_debug(LOG_VERB, "forward from c %d to s %d req %"PRIu64" len %"PRIu32
              " type %d with key '%.*s'", c_conn->sd, s_conn->sd, msg->id,
              msg->mlen, msg->type, msg->key_end - msg->key_start, msg->key_start);
}

void migrate_req_redirect(struct msg *msg, rsp_migrate_status_t type)
{
    struct conn *c_conn = msg->owner;
    struct context *ctx = conn_to_ctx(c_conn);
    struct server_pool *pool;
    if(type == RESULT_RETRY){
        if(msg->retry_count > 3){
            core_close(ctx, c_conn);
            return;
        }
        msg->retry_count += 1;
        pool = ctx->migrating_pool;
    }else if(type == RESULT_MOVED){
        unwrapper_migrating_key(msg);
		pool = (struct server_pool *)(c_conn->owner);
	}

    pool_req_redirect(msg, pool);
}

void pool_req_redirect(struct msg *msg, struct server_pool *pool)
{
    struct conn *c_conn = msg->owner;
    struct context *ctx = conn_to_ctx(c_conn);
    uint8_t *key = msg->key_start;
    uint32_t keylen = (uint32_t)(msg->key_end - msg->key_start);
    struct mbuf *buf;

    /* update mbuf read marker */
    buf = STAILQ_FIRST(&msg->mhdr);
    while (buf) {
        buf->pos = buf->begin ? buf->begin : buf->start;
        buf = STAILQ_NEXT(buf, next);
    }

    /* choise server&s_conn */
    struct conn *s_conn = server_pool_conn(ctx, pool, key, keylen,
                              msg_type_is_read(msg->type), (uint32_t)rand(),
                              c_conn);
    if (s_conn == NULL) {
        req_forward_error(ctx, c_conn, msg);
        return;
    }
    ASSERT(!s_conn->client && !s_conn->proxy);

    /* enqueue the message (request) into server inq */
    if (TAILQ_EMPTY(&s_conn->imsg_q)) {
        rstatus_t status = event_add_out(ctx->evb, s_conn);
        if (status != NC_OK) {
            req_forward_error(ctx, c_conn, msg);
            s_conn->err = errno;
            return;
        }
    }
    s_conn->enqueue_inq(ctx, s_conn, msg);
}
