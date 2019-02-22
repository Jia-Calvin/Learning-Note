//
// Created by yunfan on 16/5/26.
//
#include <nc_metric.h>

#ifdef HAVE_METRICS

#include <abase/metric/c.h>

#include <string.h>
#include <nc_message.h>

const char *const TWEMPROXY_METRIC_PREFIX = "inf.twemproxy";

static void *g_metric_database;
static void **g_metric_cli;

#define TAGS_COUNT 2

void nc_destroy_metric() {
    uint i;
    if (g_metric_cli) {
        for (i = 0; i < RSP_MSG_TYPE_LEN; i++) {
            if (g_metric_cli[i]) {
                abase_metric_free_metric(g_metric_cli[i]);
            }
        }
        free(g_metric_cli);
        g_metric_cli = NULL;
    }

    if (g_metric_database) {
        abase_metric_close_database(g_metric_database);
        g_metric_database = NULL;
    }
}

int nc_init_metric(struct instance *ins) {
    uint i;
    const char *tags_key[TAGS_COUNT] = {"cmd", "cluster"};
    const char *tags_val[TAGS_COUNT] = {NULL, ins->cluster_name};

    g_metric_cli = (void **)calloc(RSP_MSG_TYPE_LEN, sizeof(void *));
    if (g_metric_cli == NULL) {
        return NC_ERROR;
    }
    g_metric_database = abase_metric_open_database(TWEMPROXY_METRIC_PREFIX,
                                                   "online", false);
    if (g_metric_database == NULL) {
        nc_destroy_metric();
        return NC_ERROR;
    }
    /* both init redis and mc metric */
    for (i = 0; i < RSP_MSG_TYPE_LEN; i++) {
        msg_type_info_t *pMsgInfo = &type_info[i];
        tags_val[0] = pMsgInfo->str;
        g_metric_cli[i] = abase_metric_create_metric(g_metric_database,
                                                     "latency", "timer",
                                                     tags_key, tags_val, 2);
        if (!g_metric_cli[i]) {
            nc_destroy_metric();
            return NC_ERROR;
        }
    }

    return NC_OK;
}

void nc_emit_timer(msg_type_t type, uint64_t val) {
    if (g_metric_database) {
        abase_metric_emit(g_metric_cli[type], val);
    }
}
#endif
