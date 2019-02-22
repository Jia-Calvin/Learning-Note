#ifndef _NC_SLOTS_H_
#define _NC_SLOTS_H_

#include <nc_core.h>

#define REDIS_CLUSTER_SLOTS_NUM 16384
#define REDIS_CLUSTER_NODES         "*2\r\n$7\r\ncluster\r\n$5\r\nnodes\r\n"
#define REDIS_CLUSTER_NODES_LEN     (sizeof(REDIS_CLUSTER_NODES)-1)
#define REDIS_CLUSTER_SLOTS         "*2\r\n$7\r\ncluster\r\n$5\r\nslots\r\n"
#define REDIS_CLUSTER_SLOTS_LEN     (sizeof(REDIS_CLUSTER_SLOTS)-1)
#define REDIS_CLUSTER_ASKING        "*1\r\n$6\r\nasking\r\n"
#define REDIS_CLUSTER_ASKING_LEN    (sizeof(REDIS_CLUSTER_ASKING)-1)
#define REDIS_CLUSTER_READONLY      "*1\r\n$8\r\nreadonly\r\n"
#define REDIS_CLUSTER_READONLY_LEN  (sizeof(REDIS_CLUSTER_READONLY)-1)
enum slot_redirect_type {
    MOVED = 1,
    ASK,
};

struct slot {
    struct server *server;
};

struct slot_pool {
    struct server_pool *owner;
    struct slot slot[REDIS_CLUSTER_SLOTS_NUM];
    struct hash_table *server;
    struct conn *dummy_cli;
    int64_t next_rebuild;
};

rstatus_t slot_pool_init(struct slot_pool *slots, struct server_pool *pool);
rstatus_t slot_pool_update(struct slot_pool *slots, const struct string *addr);

struct server *
slot_pool_dispatch(struct server_pool *pool, uint8_t *key, uint32_t keylen);

struct server *
slot_pool_server(struct slot_pool *slots, const struct string *addr);
rstatus_t slot_pool_rebuild(struct slot_pool *slots, const struct msg *msg);

#endif
