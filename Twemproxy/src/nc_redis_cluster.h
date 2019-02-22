#ifndef _NC_REDIS_CLUSTER_H_
#define _NC_REDIS_CLUSTER_H_

#include <nc_core.h>

rstatus_t redis_cluster_init(struct server_pool *pool);
rstatus_t redis_cluster_update(struct server_pool *pool);
rstatus_t redis_cluster_rebuild(struct server_pool *pool, const struct msg *m);
struct server *
redis_cluster_dispatch(struct server_pool *pool, uint8_t *key, uint32_t keylen);


#endif
