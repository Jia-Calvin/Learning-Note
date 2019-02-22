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
#include <nc_conf.h>
#include <nc_server.h>
#include <nc_proxy.h>
#include <nc_manage.h>
#include <nc_signal.h>

static uint32_t ctx_id; /* context generation */
struct instance global_nci;

static rstatus_t
core_calc_connections(struct context *ctx)
{
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE,&limit) == -1) {
        log_crit("getrlimit failed: %s", strerror(errno));
        return NC_ERROR;
    }

    ctx->rlimit_nofile = (uint32_t)limit.rlim_cur;
    ctx->max_ncconn = ctx->rlimit_nofile - ctx->max_nsconn - RESERVED_FDS;
    log_notice("current rlimit nofile %d, max_nsconn %d, max_ncconn %d",
               ctx->rlimit_nofile, ctx->max_nsconn, ctx->max_ncconn);

    return NC_OK;
}

static struct context *
core_ctx_create(struct instance *nci)
{
    rstatus_t status;
    struct context *ctx;

    ctx = nc_alloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->id = ++ctx_id;
    ctx->cf = NULL;
    ctx->stats = NULL;
    ctx->evb = NULL;
    array_null(&ctx->pool);
    ctx->max_timeout = nci->stats_interval;
    ctx->timeout = ctx->max_timeout;
    ctx->rlimit_nofile = 0;
    ctx->max_ncconn = 0;
    ctx->max_nsconn = 0;
    ctx->reuse_port = nci->reuse_port;
    ctx->no_async = nci->no_async;
    ctx->lpm_mask = nci->lpm_mask;
    ctx->retry_ts = 0;
    ctx->sync_cache_del = NULL;
    ctx->sync_write_buf = NULL;
    ctx->sync_cache_peer = NULL;
    ctx->double_cluster_mode = nci->double_cluster_mode;
    ctx->conf_filename = nci->conf_filename;
    ctx->migrating = 0;

    /* parse and create configuration */
    ctx->cf = conf_create(nci->conf_filename);
    if (ctx->cf == NULL) {
        nc_free(ctx);
        return NULL;
    }

    /* initialize server pool from configuration */
    status = server_pool_init(&ctx->pool, &ctx->cf->pool, ctx);
    if (status != NC_OK) {
        conf_destroy(ctx->cf);
        nc_free(ctx);
        return NULL;
    }

    /* get rlimit and calc max client connections after got server pool 
     * connections number */
    status = core_calc_connections(ctx);
    if (status != NC_OK) {
        server_pool_deinit(&ctx->pool);
        conf_destroy(ctx->cf);
        nc_free(ctx);
        return NULL;
    }

    /* create stats per server pool */
    ctx->stats = stats_create(nci->stats_port, nci->stats_addr,
                              nci->stats_interval, nci->hostname, &ctx->pool);
    if (ctx->stats == NULL) {
        server_pool_deinit(&ctx->pool);
        conf_destroy(ctx->cf);
        nc_free(ctx);
        return NULL;
    }

    /* initialize event handling for client, proxy and server */
    ctx->evb = event_base_create(EVENT_SIZE, &core_core, num_signals,
                                 signals, &signal_handler);
    if (ctx->evb == NULL) {
        stats_destroy(ctx->stats);
        server_pool_deinit(&ctx->pool);
        conf_destroy(ctx->cf);
        nc_free(ctx);
        return NULL;
    }

    /* preconnect? servers in server pool */
    status = server_pool_preconnect(ctx);
    if (status != NC_OK) {
        server_pool_disconnect(ctx);
        event_base_destroy(ctx->evb);
        stats_destroy(ctx->stats);
        server_pool_deinit(&ctx->pool);
        conf_destroy(ctx->cf);
        nc_free(ctx);
        return NULL;
    }

    /* initialize proxy per server pool */
    status = proxy_init(ctx);
    if (status != NC_OK) {
        server_pool_disconnect(ctx);
        event_base_destroy(ctx->evb);
        stats_destroy(ctx->stats);
        server_pool_deinit(&ctx->pool);
        conf_destroy(ctx->cf);
        nc_free(ctx);
        return NULL;
    }
    
    /* init manager */
    status = manager_init(ctx, nci->manage_addr);
    if (status != NC_OK) {
        log_error("manager init failed.");
    }

    ctx->route = nci->route;

    log_debug(LOG_VVERB, "created ctx %p id %"PRIu32"", ctx, ctx->id);

    return ctx;
}

static void
core_ctx_destroy(struct context *ctx)
{
    log_debug(LOG_VVERB, "destroy ctx %p id %"PRIu32"", ctx, ctx->id);
    proxy_deinit(ctx);
    manager_deinit(ctx);
    server_pool_disconnect(ctx);
    event_base_destroy(ctx->evb);
    stats_destroy(ctx->stats);
    server_pool_deinit(&ctx->pool);
    conf_destroy(ctx->cf);
    nc_free(ctx);
}

struct context *
core_start(struct instance *nci)
{
    struct context *ctx;

    if (!msg_type_check()) {
        return NULL;
    }

    mbuf_init(nci);
    msg_init(nci);
    conn_init();

    // 创建struct context变量ctx，nc使用结构体struct context
    // 变量ctx保存运行时的上下文
    ctx = core_ctx_create(nci);
    if (ctx != NULL) {
        nci->ctx = ctx;
        return ctx;
    }

    conn_deinit();
    msg_deinit();
    mbuf_deinit();

    return NULL;
}

void
core_stop(struct context *ctx)
{
    conn_deinit();
    msg_deinit();
    mbuf_deinit();
    core_ctx_destroy(ctx);
}

static rstatus_t
core_recv(struct context *ctx, struct conn *conn)
{
    rstatus_t status;

    status = conn->recv(ctx, conn);
    if (status != NC_OK) {
        log_info("recv on %c %d failed: %s",
                 conn->client ? 'c' : (conn->proxy ? 'p' : 's'), conn->sd,
                 strerror(errno));
    }

    return status;
}

static rstatus_t
core_send(struct context *ctx, struct conn *conn)
{
    rstatus_t status;

    status = conn->send(ctx, conn);
    if (status != NC_OK) {
        log_info("send on %c %d failed: %s",
                 conn->client ? 'c' : (conn->proxy ? 'p' : 's'), conn->sd,
                 strerror(errno));
    }

    return status;
}

void
core_close(struct context *ctx, struct conn *conn)
{
    rstatus_t status;
    char type, *addrstr;

    ASSERT(conn->sd > 0);

    if (conn->client) {
        type = 'c';
        addrstr = nc_unresolve_peer_desc(conn->sd);
    } else {
        type = conn->proxy ? 'p' : 's';
        addrstr = nc_unresolve_addr(conn->addr, conn->addrlen);
    }
    log_debug(type == 'c' ? LOG_NOTICE : LOG_WARN,
              "close %c %d '%s' on event %04"PRIX32" eof %d done "
              "%d rb %zu sb %zu%c %s", type, conn->sd, addrstr, conn->events,
              conn->eof, conn->done, conn->recv_bytes, conn->send_bytes,
              conn->err ? ':' : ' ', conn->err ? strerror(conn->err) : "");

    status = event_del_conn(ctx->evb, conn);
    if (status < 0) {
        log_warn("event del conn %c %d failed, ignored: %s",
                 type, conn->sd, strerror(errno));
    }

    conn->close(ctx, conn);
}

static void
core_error(struct context *ctx, struct conn *conn)
{
    rstatus_t status;
    char type = conn->client ? 'c' : (conn->proxy ? 'p' : 's');

    status = nc_get_soerror(conn->sd);
    if (status < 0) {
        log_warn("get soerr on %c %d failed, ignored: %s", type, conn->sd,
                  strerror(errno));
    }
    conn->err = errno;

    core_close(ctx, conn);
}

static void
core_timeout(struct context *ctx)
{
    for (;;) {
        struct msg *msg;
        struct conn *conn;
        int64_t now, then;

        msg = msg_tmo_min();
        if (msg == NULL) {
            ctx->timeout = ctx->max_timeout;
            return;
        }

        /* skip over req that are in-error or done */

        if (msg->error || msg->done) {
            log_info("core_timeout: msg error:%d, done:%d", msg->error, msg->done);
            msg_tmo_delete(msg);
            continue;
        }

        /*
         * timeout expired req and all the outstanding req on the timing
         * out server
         */

        conn = msg->tmo_rbe.data;
        then = msg->tmo_rbe.key;

        now = nc_usec_now();
        if (now < then) {
            int delta = ((int)(then - now)) / 1000;
            ctx->timeout = MIN(delta, ctx->max_timeout);
            return;
        }

        log_info("req %"PRIu64" on s %d timedout", msg->id, conn->sd);

        msg_tmo_delete(msg);
        conn->err = ETIMEDOUT;

        core_close(ctx, conn);
    }
}

static rstatus_t
core_retry_server(void *elemt, void *data)
{
    struct server *s = elemt;
    struct server_pool *pool = s->owner;
    int64_t now = nc_usec_now();

    if (s->failure_count >= pool->server_failure_limit &&
        s->next_retry < nc_usec_now() && !s->trying) {
        struct conn *conn;
        rstatus_t status;

        log_info("trying to connect to server: '%.*s'", s->pname.len,
                 s->pname.data);
        conn = server_conn(s);
        if (!conn) {
            log_warn("allocate a connection to server '%.*s' failed",
                     s->pname.len, s->pname.data);
        } else {
            s->trying = true;
            status = server_connect(pool->ctx, s, conn);
            if (status != NC_OK) {
                server_close(pool->ctx, conn);
            }
        }
    }

    if (array_n(&s->slave_pool) != 0) {
        array_each(&s->slave_pool, &core_retry_server, NULL);
    }

    return NC_OK;
}

static rstatus_t
core_retry_pool(void *elemt, void *data)
{
    struct server_pool *sp = elemt;

    array_each(&sp->server, &core_retry_server, NULL);

    return NC_OK;
}

static rstatus_t
core_retry(struct context *ctx)
{
    time_t now = time(NULL);

    if (now != ctx->retry_ts) {
        ctx->retry_ts = now;
        array_each(&ctx->pool, &core_retry_pool, NULL);
    }

    return NC_OK;
}

rstatus_t
core_core(void *arg, uint32_t events)
{
    rstatus_t status;
    struct conn *conn = arg;
    struct context *ctx = conn_to_ctx(conn);

    log_debug(LOG_VVERB, "event %04"PRIX32" on %c %d", events,
              conn->client ? 'c' : (conn->proxy ? 'p' : 's'), conn->sd);

    conn->events = events;

    /* error takes precedence over read | write */
    if (events & EVENT_ERR) {
        core_error(ctx, conn);
        return NC_ERROR;
    }

    /* read takes precedence over write */
    if (events & EVENT_READ) {
        status = core_recv(ctx, conn);
        if (status != NC_OK || conn->done || conn->err) {
            core_close(ctx, conn);
            return NC_ERROR;
        }
    }

    if (events & EVENT_WRITE) {
        status = core_send(ctx, conn);
        if (status != NC_OK || conn->done || conn->err) {
            core_close(ctx, conn);
            return NC_ERROR;
        }
    }

    return NC_OK;
}

rstatus_t
core_loop(struct context *ctx)
{
    int nsd;

    nsd = event_wait(ctx->evb, ctx->timeout);
    if (nsd < 0) {
        return nsd;
    }

    core_timeout(ctx);

    stats_swap(ctx->stats);

    core_retry(ctx);

    return NC_OK;
}
