#include <hashkit/nc_hashkit.h>
#include <nc_hash_table.h>
#include <nc_array.h>
#include <nc_conf.h>
#include <nc_util.h>
#include <nc_slot.h>
#include <nc_server.h>

static rstatus_t slots_add_server(struct slot_pool *slots, struct server *serv)
{
    rstatus_t st;
    struct string addr = serv->pname;
    uint8_t *e = nc_strrchr(addr.data + addr.len - 1, addr.data, ':');
    addr.len = (uint32_t)(e - addr.data);
    st = hash_table_set(slots->server, addr, serv);
    return st;
}

rstatus_t slot_pool_init(struct slot_pool *slots, struct server_pool *pool)
{
    rstatus_t st = NC_OK;
    struct hash_table *htb = nc_alloc(sizeof(struct hash_table));
    struct conn* dummy_cli = NULL;
    uint32_t i;
    uint32_t n;
    if (!htb) {
        return NC_ENOMEM;
    }
    if ((dummy_cli = conn_get(pool, true, pool->redis)) == NULL) {
        nc_free(htb);
        return NC_ENOMEM;
    }
    st = hash_table_init(htb, 128);
    if (st != NC_OK) {
        nc_free(htb);
        conn_put(dummy_cli);
        return st;
    }
    slots->owner = pool;
    for (i = 0; i < REDIS_CLUSTER_SLOTS_NUM; ++i) {
        slots->slot[i].server = NULL;
    }
    slots->server = htb;
    slots->dummy_cli = dummy_cli;
    slots->next_rebuild = 0;
    n = array_n(&(pool->server));
    for (i = 0; i < n; ++i) {
        struct server *serv = array_get(&(pool->server), i);
        uint32_t j;
        st = slots_add_server(slots, serv);
        if (st != NC_OK) {
            return st;
        }
        for (j = 0; j < array_n(&serv->slave_pool); ++j) {
            struct server *s = array_get(&serv->slave_pool, j);
            st = slots_add_server(slots, s);
            if (st != NC_OK) {
                return st;
            }
        }
    }
    return NC_OK;
}

rstatus_t slot_pool_update(struct slot_pool *slots, const struct string *addr)
{
    int64_t now = nc_usec_now();
    struct msg *msg = NULL;
    struct conn *c_conn = slots->dummy_cli;
    struct mbuf *buf = NULL;
    if (now < slots->next_rebuild) {
        return NC_OK;
    }
    msg = msg_get(c_conn, true, true);
    if (!msg) {
        return NC_ENOMEM;
    }
    msg->type = MSG_REQ_REDIS_CLUSTER;
    msg->inside = 1;
    buf = mbuf_append(&msg->mhdr, REDIS_CLUSTER_NODES, REDIS_CLUSTER_NODES_LEN);
    //buf = mbuf_append(&m->mhdr, REDIS_CLUSTER_SLOTS, REDIS_CLUSTER_SLOTS_LEN);
    if (!buf) {
        msg_put(msg);
        return NC_ENOMEM;
    }
    slots->next_rebuild = now + 10000000;
    msg->mlen = mbuf_length(buf);
    log_debug(LOG_VERB, "send cluster nodes request");
    req_redirect(msg, FORCE_REDIRECT, addr);

    return NC_OK;
}

struct server *
slot_pool_dispatch(struct server_pool *pool, uint8_t *key, uint32_t keylen)
{
    int i = hash_crc16((const char*)key, keylen) % REDIS_CLUSTER_SLOTS_NUM;
    struct slot *s = pool->slots->slot + i;
    if (s->server) {
        log_info("slot pool dispatch key(%.*s) to slot %d %.*s",
                keylen, key, i, s->server->name.len, s->server->name.data);
    }
    return s->server;
}

struct server *
slot_pool_server(struct slot_pool *slots, const struct string *addr)
{
    struct server *s = NULL;
    if (addr) {
        s = hash_table_get(slots->server, *addr);
        if (s) {
            log_info("slot pool get server:%.*s",
                    s->name.len, s->name.data);
            return s;
        } else {
            log_info("can't get server for addr:%.*s", addr->len, addr->data);
            return NULL;
        }
    } else {
        struct server_pool *pool = slots->owner;
        uint32_t n = array_n(&pool->server);
        s = array_get(&pool->server, (uint32_t)random() % n);
        log_info("slot pool random get server:%.*s without addr",
                 s->name.len, s->name.data);
    }
    return s;
}

enum redis_cluster_nodes_parser_state {
    NODE_HEAD,
    NODE_LINE,
    NODE_ID,
    NODE_ADDR,
    NODE_FLAGS,
    NODE_MASTER,
    NODE_PING_SENT,
    NODE_PONG_RECV,
    NODE_CONFIG_EPOCH,
    NODE_LINK_STATE,
    NODE_SLOT,
    NODE_NEXT_SLOT,
    NODE_CR,
    NODE_END
};

enum redis_cluster_nodes_parser_result {
    PARSE_SLOT,
    PARSE_LINE,
    PARSE_ERROR,
    PARSE_END
};

struct redis_cluster_nodes_parser
{
    const struct msg *msg;
    struct mbuf *buf;
    uint8_t *p;
    int id_len;
    int addr_len;
    int flags_len;
    int master_len;
    int ping_sent_len;
    int pong_recv_len;
    int epoch_len;
    int link_state_len;
    int slot_len;
    uint8_t id[48];
    uint8_t addr[48];
    uint8_t flags[64];
    uint8_t master[48];
    uint8_t ping_sent[24];
    uint8_t pong_recv[24];
    uint8_t epoch[16];
    uint8_t link_state[16];
    uint8_t slot[64];
    int state;
};

static
void redis_cluster_nodes_parser_init(struct redis_cluster_nodes_parser *p,
                                          const struct msg *msg)
{
    p->msg = msg;
    p->buf = STAILQ_FIRST(&msg->mhdr);
    p->p = p->buf->start;
    p->state = NODE_HEAD;
}

static int redis_cluster_nodes_parse(struct redis_cluster_nodes_parser *p)
{
    while (true) {
        switch (p->state) {
        case NODE_HEAD:
            if (*p->p == '\n') {
                p->state = NODE_LINE;
            }
            break;
        case NODE_LINE:
            p->id_len = 0;
            p->addr_len = 0;
            p->flags_len = 0;
            p->master_len = 0;
            p->ping_sent_len = 0;
            p->pong_recv_len = 0;
            p->epoch_len = 0;
            p->link_state_len = 0;
            p->slot_len = 0;
            if (*p->p != '\n') {
                p->id[p->id_len++] = *p->p;
            }
            p->state = NODE_ID;
            break;
        case NODE_ID:
            if (*p->p == ' ') {
                p->state = NODE_ADDR;
            } else if (*p->p == '\r') {
                p->state = NODE_CR;
            } else {
                p->id[p->id_len++] = *p->p;
            }
            break;
        case NODE_ADDR:
            if (*p->p == ' ') {
                p->state = NODE_FLAGS;
            } else {
                p->addr[p->addr_len++] = *p->p;
            }
            break;
        case NODE_FLAGS:
            if (*p->p == ' ') {
                p->state = NODE_MASTER;
            } else {
                p->flags[p->flags_len++] = *p->p;
            }
            break;
        case NODE_MASTER:
            if (*p->p == ' ') {
                p->state = NODE_PING_SENT;
            } else {
                p->master[p->master_len++] = *p->p;
            }
            break;
        case NODE_PING_SENT:
            if (*p->p == ' ') {
                p->state = NODE_PONG_RECV;
            } else {
                p->ping_sent[p->ping_sent_len++] = *p->p;
            }
            break;
        case NODE_PONG_RECV:
            if (*p->p == ' ') {
                p->state = NODE_CONFIG_EPOCH;
            } else {
                p->pong_recv[p->pong_recv_len++] = *p->p;
            }
            break;
        case NODE_CONFIG_EPOCH:
            if (*p->p == ' ') {
                p->state = NODE_LINK_STATE;
            } else {
                p->epoch[p->epoch_len++] = *p->p;
            }
            break;
        case NODE_LINK_STATE:
            if (*p->p == ' ') {
                p->state = NODE_SLOT;
            } else if (*p->p == '\n') {
                p->state = NODE_LINE;
                return PARSE_LINE;
            } else {
                p->link_state[p->link_state_len++] = *p->p;
            }
            break;
        case NODE_SLOT:
            if (*p->p == ' ' || *p->p == '\n') {
                p->slot[p->slot_len] = '\0';
                p->state = (*p->p == ' ' ? NODE_NEXT_SLOT : NODE_LINE);
                return PARSE_SLOT;
            } else {
                p->slot[p->slot_len++] = *p->p;
            }
            break;
        case NODE_NEXT_SLOT:
            p->slot_len = 0;
            p->state = NODE_SLOT;
            break;
        case NODE_CR:
            if (*p->p != '\n') {
                return PARSE_ERROR;
            } else {
                p->state = NODE_END;
            }
            break;
        default:
            return PARSE_ERROR;
            break;
        }
        if (++(p->p) >= p->buf->last) {
            p->buf = STAILQ_NEXT(p->buf, next);
            if (p->buf) {
                p->p = p->buf->start;
            } else {
                break;
            }
        }
    }
    return p->state == NODE_END ? PARSE_END : PARSE_ERROR;
}

rstatus_t slot_pool_rebuild(struct slot_pool *slots, const struct msg *msg)
{
    struct redis_cluster_nodes_parser p;
    redis_cluster_nodes_parser_init(&p, msg);
    while (true) {
        int st = redis_cluster_nodes_parse(&p);
        if (st == PARSE_SLOT) {
            struct string addr = {(uint32_t)p.addr_len, p.addr};
            struct server *serv = slot_pool_server(slots, &addr);
            uint8_t *importing = nc_strchr(p.slot, p.slot + p.slot_len, '<');
            uint8_t *migrating = nc_strchr(p.slot, p.slot + p.slot_len, '>');
            uint8_t *step = nc_strchr(p.slot, p.slot + p.slot_len, '-');
            if (!serv) {
                log_warn("can't get server for addr(%.*s)", addr.len, addr.data);
                continue;
            }
            if (importing) {
            } else if (migrating) {
            } else if (step) {
                int start = nc_atoi(p.slot, (uint8_t)(step++ - p.slot));
                int end = nc_atoi(step, (size_t)((int)(p.slot - step) + p.slot_len));
                while (start <= end) {
                    if (start >= 0 && start < REDIS_CLUSTER_SLOTS_NUM) {
                        slots->slot[start++].server = serv;
                    }
                }
            } else {
                int num = nc_atoi(p.slot, p.slot_len);
                if (num >= 0 && num < REDIS_CLUSTER_SLOTS_NUM) {
                    slots->slot[num].server = serv;
                }
            }
            log_info("slot %.*s %.*s %.*s %.*s", p.id_len, p.id, p.addr_len, p.addr, p.slot_len, p.slot, serv->runid.len, serv->runid.data);
            if (p.state == NODE_LINE) {
                if (string_empty(&serv->runid)) {
                    string_copy(&serv->runid, p.id, (uint32_t)p.id_len);
                }
                hash_table_set(slots->server, serv->runid, serv);
                serv->slave = false;
                serv->slaves.nelem = 0;
                log_info("master %.*s %.*s", p.id_len, p.id, p.addr_len, p.addr);
            }
        } else if (st == PARSE_LINE) {
            if (p.master[0] == '-') {
                struct string addr = {(uint32_t)p.addr_len, p.addr};
                struct server *serv = slot_pool_server(slots, &addr);
                if (!serv) {
                    log_warn("can't get server for addr(%.*s)", addr.len, addr.data);
                    continue;
                }
                if (string_empty(&serv->runid)) {
                    string_copy(&serv->runid, p.id, (uint32_t)p.id_len);
                }
                hash_table_set(slots->server, serv->runid, serv);
                serv->slave = false;
                serv->slaves.nelem = 0;
                log_info("master %.*s %.*s", p.id_len, p.id, p.addr_len, p.addr);
            }
        } else if (st == PARSE_ERROR) {
            log_info("parse error----------------");
            return NC_ERROR;
        } else {
            log_info("parse end*****************");
            break;
        }
    }

    redis_cluster_nodes_parser_init(&p, msg);
    while (true) {
        int st = redis_cluster_nodes_parse(&p);
        if (st == PARSE_LINE) {
            struct string addr = {(uint32_t)p.addr_len, p.addr};
            struct server *serv = slot_pool_server(slots, &addr);
            struct string master = {(uint32_t)p.master_len, p.master};
            struct server *m;
            struct server **slave;
            if (!serv) {
                log_warn("can't get server for addr(%.*s)", addr.len, addr.data);
                continue;
            }
            if (p.master[0] == '-') {//master node
                continue;
            }
            m = hash_table_get(slots->server, master);
            if (!m) {
                log_warn("can't find master(%.*s) for slave(%.*s)",
                          master.len, master.data, addr.len, addr.data);
                continue;
            }
            if (string_empty(&serv->runid)) {
                string_copy(&serv->runid, p.id, (uint32_t)p.id_len);
            }
            hash_table_set(slots->server, serv->runid, serv);
            serv->slave = true;
            log_info("slave %.*s %.*s master(%.*s)", p.id_len, p.id, p.addr_len, p.addr, master.len, master.data);
            if ((slave = array_push(&m->slaves)) != NULL) {
                *slave = serv;
            }
        } else if (st == PARSE_ERROR) {
        } else if (st == PARSE_END) {
            break;
        }
    }
    return NC_OK;
}
