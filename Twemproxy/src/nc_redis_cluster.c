#include <nc_server.h>
#include <nc_slot.h>
#include <nc_redis_cluster.h>

rstatus_t redis_cluster_init(struct server_pool *pool)
{
    rstatus_t st = NC_OK;
    struct slot_pool *slots = nc_alloc(sizeof(struct slot_pool));
    if (!slots) {
        return NC_ENOMEM;
    }
    st = slot_pool_init(slots, pool);
    if (st != NC_OK) {
        nc_free(slots);
        return st;
    }
    pool->slots = slots;
    return NC_OK;
}

rstatus_t redis_cluster_update(struct server_pool *pool)
{
    if (!pool->slots) {
        return redis_cluster_init(pool);
    }
    return slot_pool_update(pool->slots, NULL);
}

rstatus_t redis_cluster_rebuild(struct server_pool *pool, const struct msg *m)
{
    return slot_pool_rebuild(pool->slots, m);
}

struct server *
redis_cluster_dispatch(struct server_pool *pool, uint8_t *key, uint32_t keylen)
{
    struct server *serv = slot_pool_dispatch(pool, key, keylen);
    if (!serv) {
        uint32_t n = array_n(&pool->server);
        serv = array_get(&pool->server, (uint32_t)random() % n);
        log_debug(LOG_VERB, "key '%.*s' no slot random maps to server '%.*s'", keylen,
                  key, serv->pname.len, serv->pname.data);
    }
    return serv;
}
