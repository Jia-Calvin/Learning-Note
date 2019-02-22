//
// Created by yunfan on 16/5/26.
//

#ifndef TWEMPROXY_METRIC_H
#define TWEMPROXY_METRIC_H

#include <nc_core.h>

#ifdef HAVE_METRICS
int nc_init_metric(struct instance *ins);

void nc_emit_timer(msg_type_t type, uint64_t val);

void nc_destroy_metric(void);
#else
static inline int nc_init_metric(struct instance *ins) {
    return NC_OK;
}

static inline void nc_emit_timer(msg_type_t type, uint64_t val) {
}

static inline void nc_destroy_metric(void) {
}
#endif

#endif //TWEMPROXY_METRIC_H
