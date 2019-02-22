#ifndef _NC_ROUTE_H_
#define _NC_ROUTE_H_

#include <nc_core.h>

#include <sys/queue.h>

struct route_node {
    uint32_t net;
    uint32_t mask;
};

struct route_iterator {
    uint32_t node_id;
    uint32_t metric;
};

struct route_graph {
    struct array nodes;
    struct array distance_matrix;
    struct array prefer_cube;
};

struct route_graph *route_graph_load(const char *filename);
void route_graph_free(struct route_graph *g);
void route_graph_test(void);
void route_graph_dump(struct route_graph *g);

bool route_graph_seek(struct route_graph *g, uint32_t local_ip,
                      struct route_iterator *it);
bool route_graph_match(struct route_graph *g, uint32_t peer_ip, struct
                       route_iterator *it);
bool route_graph_next(struct route_graph *g, struct route_iterator *it);

#endif
