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

#include <stdlib.h>
#include <unistd.h>

#include <nc_core.h>
#include <nc_server.h>
#include <nc_conf.h>
#include <nc_redis_cluster.h>
#include <nc_slot.h>

#define SERVER_MAX_CANDIDATES 64 /* should be enough when route read requests */

void
server_ref(struct conn *conn, void *owner)
{
    struct server *server = owner;

    ASSERT(conn != NULL && owner != NULL);
    ASSERT(!conn->client && !conn->proxy);
    ASSERT(conn->owner == NULL);

    conn->family = server->family;
    conn->addrlen = server->addrlen;
    conn->addr = server->addr;

    if (conn->is_read) {
        server->ns_conn_q_rd++;
        TAILQ_INSERT_TAIL(&server->s_conn_q_rd, conn, conn_tqe);
        ASSERT(!TAILQ_EMPTY(&server->s_conn_q_rd));
    } else {
        server->ns_conn_q_wr++;
        TAILQ_INSERT_TAIL(&server->s_conn_q_wr, conn, conn_tqe);
        ASSERT(!TAILQ_EMPTY(&server->s_conn_q_wr));
    }

    conn->owner = owner;

    log_debug(LOG_VVERB, "ref conn %p owner %p into '%.*s", conn, server,
              server->pname.len, server->pname.data);
}

void
server_unref(struct conn *conn)
{
    struct server *server;

    ASSERT(!conn->client && !conn->proxy);
    ASSERT(conn->owner != NULL);

    server = conn->owner;
    conn->owner = NULL;

    if (conn->is_read) {
        ASSERT(server->ns_conn_q_rd != 0);
        server->ns_conn_q_rd--;
        TAILQ_REMOVE(&server->s_conn_q_rd, conn, conn_tqe);
    } else {
        ASSERT(server->ns_conn_q_wr != 0);
        server->ns_conn_q_wr--;
        TAILQ_REMOVE(&server->s_conn_q_wr, conn, conn_tqe);
    }

    log_debug(LOG_VVERB, "unref conn %p owner %p from '%.*s'", conn, server,
              server->pname.len, server->pname.data);
}

int
server_timeout(struct conn *conn)
{
    struct server *server;
    struct server_pool *pool;

    ASSERT(!conn->client && !conn->proxy);

    server = conn->owner;
    pool = server->owner;

    return pool->timeout;
}

bool
server_active(struct conn *conn)
{
    ASSERT(!conn->client && !conn->proxy);

    if (!TAILQ_EMPTY(&conn->imsg_q)) {
        log_debug(LOG_VVERB, "s %d is active", conn->sd);
        return true;
    }

    if (!TAILQ_EMPTY(&conn->omsg_q)) {
        log_debug(LOG_VVERB, "s %d is active", conn->sd);
        return true;
    }

    if (conn->rmsg != NULL) {
        log_debug(LOG_VVERB, "s %d is active", conn->sd);
        return true;
    }

    if (conn->smsg != NULL) {
        log_debug(LOG_VVERB, "s %d is active", conn->sd);
        return true;
    }

    log_debug(LOG_VVERB, "s %d is inactive", conn->sd);

    return false;
}

/**
 * 将cs加入server数组，返回新加入的server地址
 */
static struct server* 
server_add(struct array *server, struct conf_server *cs, uint32_t slave_pool_size)
{
    rstatus_t status;
    struct server *ss;
    ss = array_push(server);
    ASSERT(ss != NULL);

    ss->idx = array_idx(server, ss);
    status = conf_server_transform(cs, ss, slave_pool_size);
    if (status != NC_OK) {
        server_deinit(server);
        return NULL;
    }
    ASSERT(array_n(&ss->slave_pool) == 0);

    log_debug(LOG_VERB, "server init server %"PRIu32" stub '%.*s' %s",
            ss->idx, ss->pname.len, ss->pname.data,
            (ss->slave == 0 ? "master" : "slave"));
    return ss;
}

rstatus_t
server_init(struct array *server, struct array *conf_server,
            struct server_pool *sp)
{
    /*
     * Originaly, every server will occupy a position in the pool distribution.
     * In order to support slave hosts, we combine the master and all its
     * slaves into a group, and this group will occupy one position in the pool
     * distribution. If so, distribution calcuation will stay the same.
     *
     * The master should be the stub server of its group, and all its slaves
     * will be appended into the slave_pool.
     * If the master is not configed, the first slave will act as the stub
     * server. But don't worry, the write request will not routed to it,
     * because we would check the slave flag. Moreover, this slave will NOT
     * pushed into the slave_pool.
     */

    ASSERT(array_n(conf_server) != 0);
    ASSERT(array_n(server) == 0);

    rstatus_t status;
    uint32_t i, nstub, nserver, slave_pool_size;
    struct conf_server *cs;
    struct server *s, *last_stub;
    struct server **sptr;

    /* We precalloc the server array to make sure not realloc, which may waste
     * some memory. The same for slave_pool_size. */
    sp->server_number = 0;
    slave_pool_size = array_n(conf_server);
    status = array_init(server, array_n(conf_server), sizeof(struct server));
    if (status != NC_OK) {
        return status;
    }

    nstub = 0;
    nserver = 0;
    last_stub = NULL;
    /* conf_server is already sorted by name+slave in conf_validate_server */
    for (i = 0; i < array_n(conf_server); i++) {
        cs = array_get(conf_server, i);

        if (last_stub == NULL
            || string_compare(&cs->name, &last_stub->name) != 0) {
            /* this server belongs a new name */
            s = array_push(server);
            ASSERT(s != NULL);
            nstub++;
            last_stub = s;

            s->idx = array_idx(server, s);
            s->sidx = nserver++;
            s->owner = sp;
            status = conf_server_transform(cs, s, slave_pool_size);
            if (status != NC_OK) {
                server_deinit(server);
                return status;
            }
            ASSERT(array_n(&s->slave_pool) == 0);

            if (s->local) {
                last_stub->local_server = s; /* last local win */
            }

            log_debug(LOG_VERB, "server init server %"PRIu32" stub '%.*s' %s",
                      s->idx, s->pname.len, s->pname.data,
                      (s->slave == 0 ? "master" : "slave"));
        } else {
            /* this server belongs the same name */
            s = array_push(&last_stub->slave_pool);
            ASSERT(s != NULL);

            s->idx = array_idx(&last_stub->slave_pool, s);
            s->sidx = nserver++;
            s->owner = sp;
            status = conf_server_transform(cs, s, 0);
            if (status != NC_OK) {
                server_deinit(server);
                return status;
            }

            if (s->local) {
                last_stub->local_server = s; /* last local win */
            }

            sptr = array_push(&last_stub->slaves);
            *sptr = s;

            log_debug(LOG_VERB, "server init server %"PRIu32" slave_pool "
                      "%"PRIu32" '%.*s' %s",
                      last_stub->idx, s->idx, s->pname.len, s->pname.data,
                      (s->slave == 0 ? "master" : "slave"));
        }
    }
    sp->server_number = nserver;
    ASSERT(array_n(server) == nstub);
    ASSERT(array_n(conf_server) == nserver);

    log_debug(LOG_DEBUG, "init %"PRIu32" servers in pool %"PRIu32" '%.*s'",
              nserver, sp->idx, sp->name.len, sp->name.data);

    return NC_OK;
}

void
server_deinit(struct array *server)
{
    uint32_t i, nserver;

    for (i = 0, nserver = array_n(server); i < nserver; i++) {
        struct server *s;

        s = array_pop(server);
        ASSERT(TAILQ_EMPTY(&s->s_conn_q_rd) && s->ns_conn_q_rd == 0);
        ASSERT(TAILQ_EMPTY(&s->s_conn_q_wr) && s->ns_conn_q_wr == 0);

        server_deinit(&s->slave_pool);
        array_deinit(&s->slave_pool);
    }

    array_deinit(server);
}

static void
update_tailq_head_addr(struct conn_tqh *s_conn_q){
    struct conn * conn = TAILQ_FIRST(s_conn_q);
    if (conn != NULL){
        conn->conn_tqe.tqe_prev = &s_conn_q->tqh_first;
    }else{
        s_conn_q->tqh_last = &s_conn_q->tqh_first;
    }
}

static void 
server_conn_update(struct server_pool *pool)
{
    struct server *server, *slave;
    struct conn *conn;
    uint32_t i, j;

    for (i = 0; i < array_n(&pool->server); i++) {
        server = array_get(&pool->server, i);
        update_tailq_head_addr(&server->s_conn_q_rd);
        update_tailq_head_addr(&server->s_conn_q_wr);
        TAILQ_FOREACH(conn, &server->s_conn_q_rd, conn_tqe) {
            conn->owner = server;
        }
        TAILQ_FOREACH(conn, &server->s_conn_q_wr, conn_tqe) {
            conn->owner = server;
        }
        /* deal slave_pool */
        if (server->slave_pool.nelem > 0) {
            for (j = 0; j < array_n(&server->slave_pool); j++) {
                slave = array_get(&server->slave_pool, j);
                update_tailq_head_addr(&slave->s_conn_q_rd);
                update_tailq_head_addr(&slave->s_conn_q_wr);
                TAILQ_FOREACH(conn, &slave->s_conn_q_rd, conn_tqe) {
                    conn->owner = slave;
                }
                TAILQ_FOREACH(conn, &slave->s_conn_q_wr, conn_tqe) {
                    conn->owner = slave;
                }
            }
        }
    }

}

struct conn *
server_conn2(struct server *server, bool is_read)
{
    struct server_pool *pool;
    struct conn *conn;
    unsigned int max_connections;
    uint32_t ns_conn_q;
    struct conn_tqh *s_conn_q;

    pool = server->owner;
    if (is_read) {
        max_connections = pool->server_connections - pool->server_connections_wr;
        ns_conn_q = server->ns_conn_q_rd;
        s_conn_q = &server->s_conn_q_rd;
    } else {
        max_connections = pool->server_connections_wr;
        ns_conn_q = server->ns_conn_q_wr;
        s_conn_q = &server->s_conn_q_wr;
    }
    if (max_connections < 1)
        max_connections = 1;

    /*
     * FIXME: handle multiple server connections per server and do load
     * balancing on it. Support multiple algorithms for
     * 'server_connections:' > 0 key
     */

    if (ns_conn_q < max_connections) {
        return conn_get4(server, false, pool->redis, is_read);
    }
    ASSERT(ns_conn_q == max_connections);

    /*
     * Pick a server connection from the head of the queue and insert
     * it back into the tail of queue to maintain the lru order
     */
    conn = TAILQ_FIRST(s_conn_q);
    ASSERT(!conn->client && !conn->proxy);

    TAILQ_REMOVE(s_conn_q, conn, conn_tqe);
    TAILQ_INSERT_TAIL(s_conn_q, conn, conn_tqe);

    /* update owner */
    con->owner = server;

    return conn;
}

struct conn *
server_conn(struct server *server)
{
    return server_conn2(server, true);
}

static rstatus_t
server_each_preconnect(void *elem, void *data)
{
    rstatus_t status;
    struct server *server;
    struct server_pool *pool;
    struct conn *conn;

    server = elem;
    pool = server->owner;

    if (array_n(&server->slave_pool)) {
        status = array_each(&server->slave_pool, server_each_preconnect, NULL);
        if (status != NC_OK) {
            return status;
        }
    }

    /* read */
    conn = server_conn(server);
    if (conn == NULL) {
        return NC_ENOMEM;
    }

    status = server_connect(pool->ctx, server, conn);
    if (status != NC_OK) {
        log_warn("connect to server '%.*s' failed, ignored: %s",
                 server->pname.len, server->pname.data, strerror(errno));
        server_close(pool->ctx, conn);
    }

    return NC_OK;
}

static rstatus_t
server_each_disconnect(void *elem, void *data)
{
    struct server *server;
    struct server_pool *pool;

    server = elem;
    pool = server->owner;

    while (!TAILQ_EMPTY(&server->s_conn_q_rd)) {
        struct conn *conn;

        ASSERT(server->ns_conn_q_rd > 0);

        conn = TAILQ_FIRST(&server->s_conn_q_rd);
        conn->close(pool->ctx, conn);
    }

    while (!TAILQ_EMPTY(&server->s_conn_q_wr)) {
        struct conn *conn;

        ASSERT(server->ns_conn_q_wr > 0);

        conn = TAILQ_FIRST(&server->s_conn_q_wr);
        conn->close(pool->ctx, conn);
    }

    return NC_OK;
}

static void
_server_failure(struct context *ctx, struct server *server, bool update_pool)
{
    struct server_pool *pool = server->owner;
    int64_t now, next;
    rstatus_t status;

    server->failure_count++;

    log_debug(LOG_VERB, "server '%.*s' failure count %"PRIu32" limit %"PRIu32,
              server->pname.len, server->pname.data, server->failure_count,
              pool->server_failure_limit);

    if (server->failure_count < pool->server_failure_limit) {
        return;
    }

    now = nc_usec_now();
    if (now < 0) {
        return;
    }

    stats_server_set_ts(ctx, server, server_ejected_at, now);
    stats_pool_incr(ctx, pool, server_ejects);

    next = now + pool->server_retry_timeout;
    server->next_retry = next;
    server->trying = false;

    if (!update_pool) {
        return;
    }

    log_warn("update pool %"PRIu32" '%.*s' to delete server '%.*s' "
             "for next %"PRIu32" secs", pool->idx, pool->name.len,
             pool->name.data, server->pname.len, server->pname.data,
             pool->server_retry_timeout / 1000 / 1000);

    status = server_pool_run(pool);
    if (status != NC_OK) {
        log_error("updating pool %"PRIu32" '%.*s' failed: %s", pool->idx,
                  pool->name.len, pool->name.data, strerror(errno));
    }
}

void
server_failure(struct context *ctx, struct server *server)
{
    struct server_pool *pool = server->owner;

    if (server->slave) {
        _server_failure(ctx, server, false);
    } else if (pool->auto_eject_hosts) {
        _server_failure(ctx, server, true);
    }
}

static void
server_close_stats(struct context *ctx, struct server *server, err_t err,
                   unsigned eof, unsigned connected)
{
    if (connected) {
        stats_server_decr(ctx, server, server_connections);
    }

    if (eof) {
        stats_server_incr(ctx, server, server_eof);
        return;
    }

    switch (err) {
    case ETIMEDOUT:
        stats_server_incr(ctx, server, server_timedout);
        break;
    case EPIPE:
    case ECONNRESET:
    case ECONNABORTED:
    case ECONNREFUSED:
    case ENOTCONN:
    case ENETDOWN:
    case ENETUNREACH:
    case EHOSTDOWN:
    case EHOSTUNREACH:
    default:
        stats_server_incr(ctx, server, server_err);
        break;
    }
}

void
server_close(struct context *ctx, struct conn *conn)
{
    rstatus_t status;
    struct msg *msg, *nmsg; /* current and next message */
    struct conn *c_conn;    /* peer client connection */

    ASSERT(!conn->client && !conn->proxy);

    server_close_stats(ctx, conn->owner, conn->err, conn->eof,
                       conn->connected);

    if (conn->sd < 0) {
        server_failure(ctx, conn->owner);
        conn->unref(conn);
        conn_put(conn);
        return;
    }

    for (msg = TAILQ_FIRST(&conn->imsg_q); msg != NULL; msg = nmsg) {
        nmsg = TAILQ_NEXT(msg, s_tqe);

        /* dequeue the message (request) from server inq */
        conn->dequeue_inq(ctx, conn, msg);

        /*
         * Don't send any error response, if
         * 1. request is tagged as noreply or,
         * 2. client has already closed its connection
         */
        if (msg->swallow || msg->noreply || msg->inside) {
            if (msg->swallow) {
                stats_server_incr(ctx, conn->owner, swallows);
            }
            log_info("close s %d swallow req %"PRIu64" len %"PRIu32
                     " type %d", conn->sd, msg->id, msg->mlen, msg->type);
            req_put(msg);
        } else {
            c_conn = msg->owner;
            ASSERT(c_conn->client && !c_conn->proxy);

            msg->done = 1;
            msg->error = 1;
            if (msg->frag_owner) {
                msg->frag_owner->nfrag_done++;
                msg->frag_owner->nfrag_error++;
            }
            msg->err = conn->err;

            if (req_done(c_conn, TAILQ_FIRST(&c_conn->omsg_q))) {
                event_add_out(ctx->evb, msg->owner);
            }

            log_info("close s %d schedule error for req %"PRIu64" "
                     "len %"PRIu32" type %d from c %d%c %s", conn->sd, msg->id,
                     msg->mlen, msg->type, c_conn->sd, conn->err ? ':' : ' ',
                     conn->err ? strerror(conn->err): " ");
        }
    }
    ASSERT(TAILQ_EMPTY(&conn->imsg_q));

    for (msg = TAILQ_FIRST(&conn->omsg_q); msg != NULL; msg = nmsg) {
        nmsg = TAILQ_NEXT(msg, s_tqe);

        /* dequeue the message (request) from server outq */
        conn->dequeue_outq(ctx, conn, msg);

        if (msg->swallow || msg->noreply || msg->inside) {
            if (msg->swallow) {
                stats_server_incr(ctx, conn->owner, swallows);
            }
            log_info("close s %d swallow req %"PRIu64" len %"PRIu32
                     " type %d", conn->sd, msg->id, msg->mlen, msg->type);
            req_put(msg);
        } else {
            c_conn = msg->owner;
            ASSERT(c_conn->client && !c_conn->proxy);

            msg->done = 1;
            msg->error = 1;
            if (msg->frag_owner) {
                msg->frag_owner->nfrag_done++;
                msg->frag_owner->nfrag_error++;
            }
            msg->err = conn->err;

            if (req_done(c_conn, TAILQ_FIRST(&c_conn->omsg_q))) {
                event_add_out(ctx->evb, msg->owner);
            }

            log_info("close s %d schedule error for req %"PRIu64" "
                     "len %"PRIu32" type %d from c %d%c %s", conn->sd, msg->id,
                     msg->mlen, msg->type, c_conn->sd, conn->err ? ':' : ' ',
                     conn->err ? strerror(conn->err): " ");
        }
    }
    ASSERT(TAILQ_EMPTY(&conn->omsg_q));

    msg = conn->rmsg;
    if (msg != NULL) {
        conn->rmsg = NULL;

        ASSERT(!msg->request);
        ASSERT(msg->peer == NULL);

        rsp_put(msg);

        log_info("close s %d discarding rsp %"PRIu64" len %"PRIu32" "
                 "in error", conn->sd, msg->id, msg->mlen);
    }

    ASSERT(conn->smsg == NULL);

    server_failure(ctx, conn->owner);

    conn->unref(conn);

    status = close(conn->sd);
    if (status < 0) {
        log_error("close s %d failed, ignored: %s", conn->sd, strerror(errno));
    }
    conn->sd = -1;

    conn_put(conn);
}

rstatus_t
server_connect(struct context *ctx, struct server *server, struct conn *conn)
{
    rstatus_t status;

    ASSERT(!conn->client && !conn->proxy);
    
    if (conn->sd > 0) {
        /* already connected on server connection */
        return NC_OK;
    }

    log_debug(LOG_VVERB, "connect to server '%.*s'", server->pname.len,
              server->pname.data);

    conn->sd = socket(conn->family, SOCK_STREAM, 0);
    if (conn->sd < 0) {
        log_error("socket for server '%.*s' failed: %s", server->pname.len,
                  server->pname.data, strerror(errno));
        status = NC_ERROR;
        goto error;
    }

    status = nc_set_nonblocking(conn->sd);
    if (status != NC_OK) {
        log_error("set nonblock on s %d for server '%.*s' failed: %s",
                  conn->sd,  server->pname.len, server->pname.data,
                  strerror(errno));
        goto error;
    }

    if (server->pname.data[0] != '/') {
        status = nc_set_tcpnodelay(conn->sd);
        if (status != NC_OK) {
            log_warn("set tcpnodelay on s %d for server '%.*s' failed, ignored: %s",
                     conn->sd, server->pname.len, server->pname.data,
                     strerror(errno));
        }
        status = nc_set_tcpsyncnt(conn->sd, 1);
        if (status != NC_OK) {
            log_warn("set tcpsyncnt to 1 on s %d for server '%.*s' failed, ignored: %s",
                     conn->sd, server->pname.len, server->pname.data,
                     strerror(errno));
        }
    }

    status = event_add_conn(ctx->evb, conn);
    if (status != NC_OK) {
        log_error("event add conn s %d for server '%.*s' failed: %s",
                  conn->sd, server->pname.len, server->pname.data,
                  strerror(errno));
        goto error;
    }

    ASSERT(!conn->connecting && !conn->connected);

    conn->connecting = 1;
    status = connect(conn->sd, conn->addr, conn->addrlen);
    if (status != NC_OK) {
        if (errno == EINPROGRESS) {
            log_debug(LOG_DEBUG, "connecting on s %d to server '%.*s'",
                      conn->sd, server->pname.len, server->pname.data);
            //server->trying = false;
            return NC_OK;
        }

        log_error("connect on s %d to server '%.*s' failed: %s", conn->sd,
                  server->pname.len, server->pname.data, strerror(errno));

        goto error;
    }

    server_connected(ctx, conn);

    return NC_OK;

error:
    conn->err = errno;
    return status;
}

void
server_connected(struct context *ctx, struct conn *conn)
{
    struct server *server = conn->owner;

    ASSERT(!conn->client && !conn->proxy);
    ASSERT(conn->connecting && !conn->connected);

    stats_server_incr(ctx, server, server_connections);

    conn->connecting = 0;
    conn->connected = 1;

    log_info("connected on s %d to server '%.*s'", conn->sd,
             server->pname.len, server->pname.data);

    if (server->trying) {
        server_ok(ctx, conn);
    }
}

void
server_ok(struct context *ctx, struct conn *conn)
{
    struct server *server = conn->owner;

    ASSERT(!conn->client && !conn->proxy);
    ASSERT(conn->connected);

    if (server->failure_count != 0) {
        log_warn("reset server '%.*s' failure count from %"PRIu32
                  " to 0", server->pname.len, server->pname.data,
                  server->failure_count);
        server->failure_count = 0;
        server->next_retry = 0LL;
        server->trying = false;
    }
}

static rstatus_t
server_pool_update(struct server_pool *pool)
{
    rstatus_t status;
    int64_t now;
    uint32_t pnlive_server; /* prev # live server */

    if (!pool->auto_eject_hosts) {
        return NC_OK;
    }

    now = nc_usec_now();
    if (now < 0) {
        return NC_ERROR;
    }

    if (pool->dist_type != DIST_REDIS_CLUSTER) {
        /* cache */
        if (pool->next_rebuild == 0LL) {
            return NC_OK;
        }
        if (now <= pool->next_rebuild) {
            if (pool->nlive_server == 0) {
                errno = ECONNREFUSED;
                return NC_ERROR;
            }
            return NC_OK;
        }
    }
    else {
        /* cluster */
        if (now < pool->slots->next_rebuild) {
            return NC_OK;
        }
    }

    pnlive_server = pool->nlive_server;

    status = server_pool_run(pool);
    if (status != NC_OK) {
        log_error("updating pool %"PRIu32" with dist %d failed: %s", pool->idx,
                  pool->dist_type, strerror(errno));
        return status;
    }

    log_info("update pool %"PRIu32" '%.*s' to add %"PRIu32" servers",
             pool->idx, pool->name.len, pool->name.data,
             pool->nlive_server - pnlive_server);


    return NC_OK;
}

static uint32_t
server_pool_hash(struct server_pool *pool, uint8_t *key, uint32_t keylen)
{
    ASSERT(array_n(&pool->server) != 0);

    if (array_n(&pool->server) == 1) {
        return 0;
    }

    ASSERT(key != NULL && keylen != 0);

    return pool->key_hash((char *)key, keylen);
}

struct server *
server_pool_server(struct server_pool *pool, uint8_t *key, uint32_t keylen)
{
    struct server *server;
    uint32_t hash, idx;

    ASSERT(array_n(&pool->server) != 0);
    ASSERT(key != NULL && keylen != 0);

    switch (pool->dist_type) {
    case DIST_KETAMA:
        hash = server_pool_hash(pool, key, keylen);
        idx = ketama_dispatch(pool->continuum, pool->ncontinuum, hash);
        break;

    case DIST_MODULA:
        hash = server_pool_hash(pool, key, keylen);
        idx = modula_dispatch(pool->continuum, pool->ncontinuum, hash);
        break;

    case DIST_RANDOM:
        idx = random_dispatch(pool->continuum, pool->ncontinuum, 0);
        break;

    case DIST_REDIS_CLUSTER:
        return redis_cluster_dispatch(pool, key, keylen);
    default:
        NOT_REACHED();
        return NULL;
    }
    ASSERT(idx < array_n(&pool->server));

    server = array_get(&pool->server, idx);

    log_debug(LOG_VERB, "key '%.*s' on dist %d maps to server '%.*s'", keylen,
              key, pool->dist_type, server->pname.len, server->pname.data);

    return server;
}

struct route_ctx {
    bool use_route;
    struct route_graph *route;
    struct route_iterator it;
    uint32_t lpm_mask;
};

static void route_ctx_init(struct route_ctx *r_ctx, struct context *ctx,
                           uint32_t local_ip)
{
    if (ctx->route) {
        r_ctx->use_route = route_graph_seek(ctx->route, local_ip, &r_ctx->it);
        r_ctx->route = ctx->route;
    } else {
        r_ctx->use_route = false;
    }
    r_ctx->lpm_mask = ctx->lpm_mask;
}

static bool route_ctx_next(struct route_ctx *ctx)
{
    if (ctx->use_route) {
        return route_graph_next(ctx->route, &ctx->it);
    } else if (ctx->lpm_mask) {
        ctx->lpm_mask = 0;
        return true;
    }
    return false;
}

static inline bool route_ctx_match(struct route_ctx *ctx, struct server *server,
                                   struct conn *conn) {
    ASSERT(server->family == AF_INET);
    ASSERT(server->addr->sa_family == AF_INET);
    struct sockaddr_in *serv_addr = (struct sockaddr_in *)server->addr;
    if (ctx->use_route) {
        return route_graph_match(ctx->route, ntohl(serv_addr->sin_addr.s_addr),
                                 &ctx->it);
    } else {
        ASSERT(conn->client);
        ASSERT(conn->family == AF_INET);
        ASSERT(conn->addr->sa_family == AF_INET);
        struct sockaddr_in *clnt_addr = (struct sockaddr_in *)conn->addr;

        return (ntohl(clnt_addr->sin_addr.s_addr ^ serv_addr->sin_addr.s_addr) & ctx->lpm_mask) == 0;
    }
}


static struct server *
server_pool_server_balance(struct server_pool *pool, struct server *stub,
                           bool is_read, uint32_t hint, struct conn *conn)
{
    uint32_t i, num;
    int64_t now;
    now = nc_usec_now();
    if (now < 0) {
        return NULL;
    }
    uint32_t failure_limit = pool->server_failure_limit;

    if (!is_read) { /* write request */
        if (stub->slave) {
            log_warn("no server for write request in pool '%.*s'",
                     pool->name.len, pool->name.data);
            return NULL;
        } else {
            log_debug(LOG_DEBUG, "write request balance to server '%.*s'",
                      stub->pname.len, stub->pname.data);
            return stub;
        }
    } else { /* read request */
        if (pool->read_prefer == READ_PREFER_MASTER) { /* prefer master */
            if (!stub->slave) {
                log_debug(LOG_DEBUG, "read request balance to master server '%.*s'",
                stub->pname.len, stub->pname.data);
                return stub;
            }
        }
    }

    /* now, we try to filter servers available and select it based on client */
    struct server *candidates[SERVER_MAX_CANDIDATES] = {NULL};
    struct route_ctx r_ctx;
    
    for (;;) {
        num = 0;
        /* Add stub into candidates, if stub is master and we don't prefer the slave */
        if (stub->failure_count < failure_limit && pool->read_prefer != READ_PREFER_SLAVE ) {
            /* read_local_first */
            if ((pool->read_local_first && stub->local)||!(pool->read_local_first))
            {
                candidates[num++] = stub;
                log_debug(LOG_VERB, "read request balance add candidate '%.*s'",
                      stub->pname.len, stub->pname.data);
            }
        }
        /* Add slaves into candidates */
        log_debug(LOG_DEBUG, "server '%.*s' has %"PRIu32" slave.", stub->pname.len, stub->pname.data, array_n(&stub->slaves));
        for (i = 0; i < array_n(&stub->slaves); i++) {
            if (num >= SERVER_MAX_CANDIDATES) {
                break;
            }
            struct server *s = *(struct server **)array_get(&stub->slaves, i);
            ASSERT(s != NULL);
            log_debug(LOG_DEBUG, "server '%.*s' failure_count %"PRIu32"", s->pname.len, s->pname.data, s->failure_count);
            if (s->failure_count < failure_limit) {
                /* read_local_first */
                if ((pool->read_local_first && s->local)||!(pool->read_local_first))
                {
                    candidates[num++] = s;
                    log_debug(LOG_VERB, "read request balance add candidate '%.*s'",
                                s->pname.len, s->pname.data);
                }
            }
        }

        if (num > 0) {
            struct server *s = candidates[hint % num];
            log_debug(LOG_DEBUG, "read request balance to server '%.*s' "
                      "hint %"PRIu32, s->pname.len,
                      s->pname.data, hint);
            return s;
        } else {
            log_debug(LOG_DEBUG, "read request balance to server '%.*s', slaves failover", 
                      stub->pname.len, stub->pname.data);
            return stub;
        }
    }

    /* Failover to stub if no server is available. Maybe stub is in failure,
     * but we have no better idea */
    log_debug(LOG_DEBUG, "no slave for read request, failover to '%.*s'",
              stub->pname.len, stub->pname.data);
    return stub;
}

void server_dump(struct server_pool *pool)
{
    uint32_t i, j, nserver;
    struct server *s, *ss;

    log_info("server array addr:%d", &pool->server);
    nserver = array_n(&pool->server);
    for(i = 0; i < nserver; i++) {
        s = array_get(&pool->server, i);
        log_info("server addr:%d", s);
        log_info("server index: %d", s->idx);
        //log_info("server stats index: %d", s->sidx);
        log_info("server pname: %s", s->pname.data);
        log_info("server name: %s", s->name.data);
        log_info("server slave: %d", s->slave);

        if(s->slave_pool.nelem != 0) {
            for(j = 0; j < array_n(&s->slave_pool); j++) {
                ss = array_get(&s->slave_pool, j);
                log_info("  [slave_pool] server index: %d", ss->idx);
                log_info("  [slave_pool] server stats index: %d", ss->sidx);
                log_info("  [slave_pool] server pname: %s", ss->pname.data);
                log_info("  [slave_pool] server name: %s", ss->name.data);
                log_info("  [slave_pool] server slave: %d", ss->slave);
            }
        }

        /*
        if(s->slaves.nelem != 0) {
            for(j = 0; j < array_n(&s->slaves); j++) {
                ss = array_get(&s->slaves, j);
                log_info("  [slaves] server index: %d", ss->idx);
                log_info("  [slaves] server stats index: %d", ss->sidx);
                log_info("  [slaves] server pname: %s", ss->pname.data);
                log_info("  [slaves] server name: %s", ss->name.data);
                log_info("  [slaves] server slave: %d", ss->slave);
            }
        }
        */
    }
    log_info("ncontinuum:%d, nserver_continuum:%d, continuum idx:%d, continuum value:%d",
            pool->ncontinuum, pool->nserver_continuum, pool->continuum->index,
            pool->continuum->value);
}

rstatus_t
server_conf_reload(struct context *ctx, struct server_pool *pool)
{
    rstatus_t status;
     /* reload conf_pool from config file */
	// FIXME: memleak?
	conf_destroy(ctx->cf);
	ctx->cf = conf_create(ctx->conf_filename);
    log_info("reload conf_pool from config file succeed");

    /* reload array server_pool from array conf_pool, maybe alter the addr of the server (array_push) */
    status = server_pool_reload(&ctx->pool, &ctx->cf->pool, ctx);
    if (status != NC_OK) {
        log_error("reload array server_pool failed: %s", strerror(errno));
        return status;
    }
    log_info("reload array server_pool succeed. server %d server_continuum %d", pool->server_number, pool->nserver_continuum);

    server_conn_update(pool);

    //preconnect, update exist conn->owner
    status = server_pool_preconnect(ctx);
    if (status != NC_OK) {
        server_pool_disconnect(ctx);
        event_base_destroy(ctx->evb);
        stats_destroy(ctx->stats);
        server_pool_deinit(&ctx->pool);
        conf_destroy(ctx->cf);
        nc_free(ctx);
        return status;
    }

    if (pool->slots != NULL) {
        hash_table_deinit(pool->slots->server);
        nc_free(pool->slots->server);
        conn_put(pool->slots->dummy_cli);
        nc_free(pool->slots);
        pool->slots = NULL;
    }

    /* update the hash of server_pool */
    status = server_pool_run(pool);
    if (status != NC_OK) {
        log_error("updating pool %"PRIu32" with dist %d failed: %s", pool->idx,
                pool->dist_type, strerror(errno));
        return status;
    }
    log_info("update the hash of server_pool succeed.");

    status = stats_rebuild(ctx);
    if (status != NC_OK) {
        log_error("stats rebuild failed: %s", pool->idx,
                pool->dist_type, strerror(errno));
        return status;
    }

    reload_config = 0;

    return status;
}

static void
recovery_origin_servers(struct array *old, struct array *new, size_t origin_number)
{
    old->nelem = (uint32_t)origin_number;
    old->elem = new->elem;
}

rstatus_t
server_double_conf(struct context *ctx)
{
    // cp server_pool[0] to migrating_pool
    ctx->migrating_pool = (struct server_pool *)nc_alloc(sizeof(*ctx->migrating_pool));
    struct server_pool *sp = (struct server_pool *)array_top(&ctx->pool);
    memcpy(ctx->migrating_pool, sp, sizeof(*ctx->migrating_pool));
    ctx->migrating_pool->continuum = (struct continuum *)nc_alloc(sizeof(*sp->continuum) * sp->ncontinuum);
    memcpy(ctx->migrating_pool->continuum, sp->continuum, sizeof(*sp->continuum) * sp->ncontinuum);

    // save old master server number
    size_t master_nelem = sp->server.nelem;

    //reload conf
    if (server_conf_reload(ctx, sp) != NC_OK) {
        log_info("fail to reload update conf_file");
        return NC_ERROR;
    }

    log_info("after reload, old pool server num:%d", array_n(&ctx->migrating_pool->server));
    ASSERT(ctx->migrating_pool->server_number <= sp->server_number);

    //recover old servers
    struct array * old_servers = &ctx->migrating_pool->server;
    struct array * new_servers = &sp->server;
    recovery_origin_servers(old_servers, new_servers, master_nelem);

    return NC_OK;
}

struct conn *
server_pool_conn(struct context *ctx, struct server_pool *pool, uint8_t *key,
                 uint32_t keylen, bool is_read, uint32_t hint, struct conn *c)
{
    rstatus_t status;
    struct server *server;
    struct conn *conn;

    if (reload_config == 1)
    {
        log_info("recv signal reload_config, begin reload conf_file");
        status = server_conf_reload(ctx, pool);
        if (status != NC_OK) {
            log_info("fail to reload conf_file");
            return NULL;
        }
        log_info("end to reload conf_file");
        server_dump(pool);
    }

    if (double_conf == 1)
    {
        log_info("recv signal double_read, begin reload conf_file");
        status = server_double_conf(ctx);
        if (status != NC_OK) {
            log_info("double_read: fail to reload conf_file");
            return NULL;
        }
        double_conf = 0;
        double_read = 1;
        log_info("double_read: end to reload double_conf %d double_read %d", double_conf, double_read);
    }

    status = server_pool_update(pool);
    if (status != NC_OK) {
        return NULL;
    }

    /* from a given {key, keylen} pick a server from pool */
    server = server_pool_server(pool, key, keylen);
    if (server == NULL) {
        log_info("can't get server for key:%.*s", keylen, key);
        return NULL;
    }
    log_info("get server %s for key:%.*s", server->pname.data, keylen, key);

    /* route read request to slaves if possible */
    server = server_pool_server_balance(pool, server, is_read, hint, c);
    if (server == NULL) {
        return NULL;
    }
    log_info("get server %s for balance", server->pname.data);

    /* pick a connection to a given server */
    conn = server_conn2(server, is_read);
    if (conn == NULL) {
        return NULL;
    }

    status = server_connect(ctx, server, conn);
    if (status != NC_OK) {
        server_close(ctx, conn);
        return NULL;
    }

    return conn;
}

struct conn *
server_pool_redirect_conn(struct context *ctx, struct server_pool *pool,
                          const struct string *addr, bool is_read)
{
    rstatus_t status;
    struct server *server = slot_pool_server(pool->slots, addr);
    struct conn *conn;
    if (server == NULL) {
        return NULL;
    }

    conn = server_conn2(server, is_read);
    if (conn == NULL) {
        return NULL;
    }

    status = server_connect(ctx, server, conn);
    if (status != NC_OK) {
        server_close(ctx, conn);
        return NULL;
    }

    return conn;
}

static rstatus_t
server_pool_each_preconnect(void *elem, void *data)
{
    rstatus_t status;
    struct server_pool *sp = elem;

    if (!sp->preconnect) {
        return NC_OK;
    }

    status = array_each(&sp->server, server_each_preconnect, NULL);
    if (status != NC_OK) {
        return status;
    }

    return NC_OK;
}

rstatus_t
server_pool_preconnect(struct context *ctx)
{
    rstatus_t status;

    status = array_each(&ctx->pool, server_pool_each_preconnect, NULL);
    if (status != NC_OK) {
        return status;
    }

    return NC_OK;
}

static rstatus_t
server_pool_each_disconnect(void *elem, void *data)
{
    rstatus_t status;
    struct server_pool *sp = elem;

    status = array_each(&sp->server, server_each_disconnect, NULL);
    if (status != NC_OK) {
        return status;
    }

    return NC_OK;
}

void
server_pool_disconnect(struct context *ctx)
{
    array_each(&ctx->pool, server_pool_each_disconnect, NULL);
}

static rstatus_t
server_pool_each_set_owner(void *elem, void *data)
{
    struct server_pool *sp = elem;
    struct context *ctx = data;

    sp->ctx = ctx;

    return NC_OK;
}

static rstatus_t
server_pool_set_sync_cache_del(void *elem, void *data)
{
    struct server_pool *sp = elem;

    if (nc_strncmp("sync_cache_del", sp->name.data, sp->name.len) == 0) {
        struct context *ctx = data;
        ctx->sync_cache_del = sp;
    }

    return NC_OK;
}

static rstatus_t
server_pool_set_sync_write_buf(void *elem, void *data)
{
    struct server_pool *sp = elem;
    
    if (nc_strncmp("sync_write_buf", sp->name.data, sp->name.len) == 0) {
        struct context *ctx = data;
        ctx->sync_write_buf = sp;
    }

    return NC_OK;
}

static rstatus_t
server_pool_set_sync_cache_peer(void *elem, void *data)
{
    struct server_pool *sp = elem;

    if (nc_strncmp("sync_cache_peer", sp->name.data, sp->name.len) == 0) {
        struct context *ctx = data;
        ctx->sync_cache_peer = sp;
    }

    return NC_OK;
}

static rstatus_t
server_pool_each_calc_connections(void *elem, void *data)
{
    struct server_pool *sp = elem;
    struct context *ctx = data;

    ctx->max_nsconn += sp->server_connections * array_n(&sp->server);
    ctx->max_nsconn += 1; /* pool listening socket */

    return NC_OK;
}

rstatus_t
server_pool_run(struct server_pool *pool)
{
    ASSERT(array_n(&pool->server) != 0);

    switch (pool->dist_type) {
    case DIST_KETAMA:
        return ketama_update(pool);

    case DIST_MODULA:
        return modula_update(pool);

    case DIST_RANDOM:
        return random_update(pool);

    case DIST_REDIS_CLUSTER:
        return redis_cluster_update(pool);

    default:
        NOT_REACHED();
        return NC_ERROR;
    }

    return NC_OK;
}

static rstatus_t
server_pool_each_run(void *elem, void *data)
{
    return server_pool_run(elem);
}

// server_pool 和 conf_pool 都是自建结构体数组
rstatus_t
server_pool_init(struct array *server_pool, struct array *conf_pool,
                 struct context *ctx)
{
    rstatus_t status;
    uint32_t npool;

    npool = array_n(conf_pool);
    ASSERT(npool != 0);
    ASSERT(array_n(server_pool) == 0);

    // 初始化多个服务池，多少个解析配置池就有多少个服务池
    // 对于lf+hl来说： 有2个
    status = array_init(server_pool, npool, sizeof(struct server_pool));
    if (status != NC_OK) {
        return status;
    }

    /* transform conf pool to server pool */
    status = array_each(conf_pool, conf_pool_each_transform, server_pool);
    if (status != NC_OK) {
        server_pool_deinit(server_pool);
        return status;
    }
    ASSERT(array_n(server_pool) == npool);

    /* set ctx as the server pool owner */
    status = array_each(server_pool, server_pool_each_set_owner, ctx);
    if (status != NC_OK) {
        server_pool_deinit(server_pool);
        return status;
    }

    /* setup sync_cache_del pool */
    status = array_each(server_pool, server_pool_set_sync_cache_del, ctx);
    if (status != NC_OK) {
        server_pool_deinit(server_pool);
        return status;
    }

    status = array_each(server_pool, server_pool_set_sync_write_buf, ctx);
    if (status != NC_OK) {
        server_pool_deinit(server_pool);
        return status;
    }

    /* setup sync_cache_peer pool, another cluster for seperate write/read request */
    status = array_each(server_pool, server_pool_set_sync_cache_peer, ctx);
    if (status != NC_OK) {
        server_pool_deinit(server_pool);
        return status;
    }

    /* calculate max server connections */
    ctx->max_nsconn = 0;
    status = array_each(server_pool, server_pool_each_calc_connections, ctx);
    if (status != NC_OK) {
        server_pool_deinit(server_pool);
        return status;
    }

    /* update server pool continuum */
    status = array_each(server_pool, server_pool_each_run, NULL);
    if (status != NC_OK) {
        server_pool_deinit(server_pool);
        return status;
    }

    log_debug(LOG_DEBUG, "init %"PRIu32" pools", npool);

    return NC_OK;
}

void
server_pool_deinit(struct array *server_pool)
{
    uint32_t i, npool;

    for (i = 0, npool = array_n(server_pool); i < npool; i++) {
        struct server_pool *sp;

        sp = array_pop(server_pool);
        ASSERT(sp->p_conn == NULL);
        ASSERT(TAILQ_EMPTY(&sp->c_conn_q) && sp->nc_conn_q == 0);

        if (sp->continuum != NULL) {
            nc_free(sp->continuum);
            sp->ncontinuum = 0;
            sp->nserver_continuum = 0;
            sp->nlive_server = 0;
        }

        server_deinit(&sp->server);

        log_debug(LOG_DEBUG, "deinit pool %"PRIu32" '%.*s'", sp->idx,
                  sp->name.len, sp->name.data);
    }

    array_deinit(server_pool);

    log_debug(LOG_DEBUG, "deinit %"PRIu32" pools", npool);
}

/**
 * 重建server_pool
 */
rstatus_t 
server_pool_reload(struct array *server_pool, struct array *conf_pool,
                 struct context *ctx)
{
    rstatus_t status;
    status = array_each(conf_pool, conf_pool_each_diff, server_pool);
    return status;
}

struct server*
server_pool_new_server(struct server_pool *pool, const struct string *addr)
{
    rstatus_t st;
    struct context *ctx = pool->ctx;
    struct conf_pool *cp = array_get(&(ctx->cf->pool), pool->idx);
    struct conf_server *cs;
    struct string host = *addr;
    int port = 0;
    uint8_t *host_end = nc_strchr(host.data, host.data + host.len, ':');
    struct server *s;
    if (host_end) {
        host.len = (uint8_t)(host_end - host.data);
        port = nc_atoi(host_end + 1, addr->len - host.len - 1);
    }
    s = array_push(&pool->server);
    if (!s) {
        log_warn("slot pool update for(%.*s) create server fail",
                  addr->len, addr->data);
        return NULL;
    }
    ASSERT(cp);
    cs = array_push(&cp->server);
    if (!cs) {
        log_warn("slot pool update for(%.*s) create conf_server fail",
                  addr->len, addr->data);
        return NULL;
    }
    conf_server_init(cs);
    if (string_duplicate(&cs->pname, addr) != NC_OK) {
        log_warn("slot pool update for(%.*s) duplicate pname fail",
                  addr->len, addr->data);
        return NULL;
    }
    if (string_duplicate(&cs->name, addr) != NC_OK) {
        log_warn("slot pool update for(%.*s) duplicate name fail",
                  addr->len, addr->data);
        return NULL;
    }
    if (host_end) {
        *host_end = 0;
    }
    if (nc_resolve(&host, port, &cs->info) != 0) {
        log_warn("slot pool update for(%.*s) resolve addr fail",
                  addr->len, addr->data);
        if (host_end) {
            *host_end = ':';
        }
        return NULL;
    }
    if (host_end) {
        *host_end = ':';
    }
    cs->port = port;
    cs->weight = 1;
    cs->valid = 1;
    cs->local = 0;
    st = conf_server_transform(cs, s, 0);
    if (st != NC_OK) {
        log_warn("slot pool update for(%.*s) conf transform fail",
                  addr->len, addr->data);
        return NULL;
    }
    s->idx = array_n(&pool->server) - 1;
    s->sidx = pool->server_number;
    s->owner = pool;
    pool->server_number += 1;
    log_info("server pool create server:%.*s", s->name.len, s->name.data);

    return s;
}

rstatus_t
server_array_update(struct array *server, struct array *conf_server,
            struct server_pool *sp)
{
    ASSERT(array_n(conf_server) != 0);

    rstatus_t status;
    uint32_t i, nstub, nserver, slave_pool_size;
    struct conf_server *cs;
    struct server *last_stub, *ss;
    struct server **sptr;
    uint32_t midx, sidx;

    sp->server_number = 0;
    slave_pool_size = array_n(conf_server);

    nstub = 0;
    nserver = 0;
    last_stub = NULL;
    midx = 0;       /* master server index in conf */
    sidx = 0;       /* slave server index in conf */

    if (sp->dist_type == DIST_REDIS_CLUSTER) {
        for (i = 0; i < array_n(conf_server); i++) {
            cs = array_get(conf_server, i);
            
            if (midx >= array_n(server)) {
                //new server
                ss = server_add(server, cs, slave_pool_size);
                ss->sidx = ss->idx;
                ss->owner = sp;
            }
            else {
                ss = array_get(server, midx);
                if (string_compare(&cs->pname, &ss->pname) != 0) {
                    //update server
                    log_info("update cluster server idx %d to pname %s", midx, cs->pname.data);
                    server_each_disconnect(ss, NULL);
                    status = conf_server_update(cs, ss);
                    if (status != NC_OK) {
                        server_deinit(server);
                        return status;
                    }
                }
            }
            
            nstub++;
            nserver++;
            midx++;
        }
    } else {
        for (i = 0; i < array_n(conf_server); i++) {
            cs = array_get(conf_server, i);

            if (last_stub == NULL || string_compare(&cs->name, &last_stub->name) != 0) {
                /* this server belongs a new name, master */
                if (midx >= array_n(server)) {
                    //new master server
                    ss = server_add(server, cs, slave_pool_size);
                    ss->sidx = nserver;
                    ss->owner = sp;
                    
                    if (ss->local) {
                        last_stub->local_server = ss; /* last local win */
                    }
                }
                else {
                    ss = array_get(server, midx);
                    if (string_compare(&cs->pname, &ss->pname) != 0) {
                        //update master server
                        log_info("update master server idx %d to pname %s", midx, cs->pname.data);
                        server_each_disconnect(ss, NULL);
                        status = conf_server_update(cs, ss);
                        if (status != NC_OK) {
                            server_deinit(server);
                            return status;
                        }
                    }
                }
                last_stub = ss;
                nstub++;
                midx++;
                sidx = 0;
            } else {
                /* this server belongs the same name, slave */
                if (sidx >= array_n(&last_stub->slave_pool)) {
                    //new slave server
                    ss = server_add(&last_stub->slave_pool, cs, 0);  
                    ss->sidx = nserver;
                    ss->owner = sp;
                    
                    if (ss->local) {
                        last_stub->local_server = ss; // last local win 
                    }

                    sptr = array_push(&last_stub->slaves);
                    *sptr = ss;
                } 
                else {
                    ss = array_get(&last_stub->slave_pool, sidx);
                    if (string_compare(&cs->pname, &ss->pname) != 0) {
                        //update master server
                        log_info("(last_stub %s)update slave server idx %d to pname %s", last_stub->pname.data, sidx, cs->pname.data);
                        server_each_disconnect(ss, NULL);
                        status = conf_server_update(cs, ss);
                        if (status != NC_OK) {
                            server_deinit(server);
                            return status;
                        }
                    } 
                }
                sidx++;
            }
            nserver++;
        }
    }
    sp->server_number = nserver;
    
    return NC_OK;
}
