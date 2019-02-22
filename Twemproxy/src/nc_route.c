#include <nc_route.h>
#include <stdio.h>
#include <assert.h>

#include <arpa/inet.h>

static rstatus_t route_graph_deinit_row(void *e, void *user)
{
    array_deinit(e);

    return NC_OK;
}

static rstatus_t route_graph_deinit_matrix(void *e, void *user)
{
    struct array *a = e;

    array_each(a, route_graph_deinit_row, NULL);
    array_deinit(a);

    return NC_OK;
}

void route_graph_free(struct route_graph *g)
{
    array_each(&g->prefer_cube, route_graph_deinit_matrix, NULL);
    array_deinit(&g->prefer_cube);
    route_graph_deinit_matrix(&g->distance_matrix, NULL);
    array_deinit(&g->nodes);
    free(g);
}

static struct route_graph *route_graph_create(void)
{
    struct route_graph *g = malloc(sizeof(*g));

    if (!g) {
        goto err;
    }
    if (array_init(&g->nodes, 4, sizeof(struct route_node)) != NC_OK) {
        goto err2;
    }
    if (array_init(&g->distance_matrix, 4, sizeof(struct array)) != NC_OK) {
        goto err3;
    }
    if (array_init(&g->prefer_cube, 4, sizeof(struct array)) != NC_OK) {
        goto err4;
    }

    return g;
err4:
    array_deinit(&g->distance_matrix);
err3:
    array_deinit(&g->nodes);
err2:
    free(g);
err:
    return NULL;
}

static int parse_node(struct route_node *node, char *node_str)
{
    char *delim = strchr(node_str, '/');
    unsigned long int prefix_len;
    char *endptr;

    if (!delim) {
        log_error("No delimiter '/': %s", node_str);
        goto err;
    }
    *delim++ = '\0';
    if (inet_pton(AF_INET, node_str, &node->net) != 1) {
        log_error("Invalid net: %s", node_str);
        goto err;
    }
    node->net = ntohl(node->net);
    if (!*delim) {
        log_error("No prefix length");
        goto err;
    }
    errno = 0;
    prefix_len = strtoul(delim, &endptr, 10);
    if (errno || *endptr) {
        log_error("Invalid prefix length: %s", delim);
        goto err;
    }
    if (prefix_len > 32) {
        log_error("Invalid prefix length: %lu", prefix_len);
        goto err;
    }
    node->mask = (uint32_t)(~((1ULL << (32 - prefix_len)) - 1));
    node->net = node->net & node->mask;

    return 0;
err:
    return -1;
}

static bool node_overlap(const struct route_node *n1,
                         const struct route_node *n2)
{
    uint32_t mask = MIN(n1->mask, n2->mask);
    return ((n1->net ^ n2->net) & mask) == 0;
}

static int route_graph_get_node_id(struct route_graph *g,
                                   const struct route_node *node)
{
    uint32_t id;
    struct route_node *n;

    for (id = 0; id < array_n(&g->nodes); ++id) {
        n = array_get(&g->nodes, id);
        if (n->net == node->net && n->mask == node->mask) {
            return (int)id;
        } else if (node_overlap(node, n)) {
            log_error("Nodes overlap");
            goto err;
        }
    }
    n = array_push(&g->nodes);
    if (!n) {
        log_error("oom");
        goto err;
    }
    *n = *node;

    return (int)id;
err:
    return -1;
}

static int route_graph_add_metric(struct route_graph *g, uint32_t id1,
                                  uint32_t id2, unsigned long metric)
{
    struct array *row;

    while (array_n(&g->distance_matrix) <= id1) {
        struct array *a = array_push(&g->distance_matrix);
        if (!a) {
            log_error("oom");
            goto err;
        }
        if (array_init(a, 4, sizeof(unsigned long)) != NC_OK) {
            log_error("oom");
            goto err;
        }
    }

    row = array_get(&g->distance_matrix, id1);
    while (array_n(row) <= id2) {
        unsigned long *distance = array_push(row);
        if (!distance) {
            log_error("omm");
            goto err;
        }
        *distance = ULONG_MAX;
    }

    *(unsigned long *)array_get(row, id2) = metric;

    return 0;
err:
    return -1;
}

static int route_graph_add_rule(struct route_graph *g, char *n1_str,
                                char *n2_str, const char *metric_str)
{
    struct route_node node1, node2;
    unsigned long metric;
    char *endptr;
    int id1, id2;

    if (parse_node(&node1, n1_str) || parse_node(&node2, n2_str)) {
        goto err;
    }
    if (!*metric_str) {
        log_error("No metric");
        goto err;
    }
    errno = 0;
    metric = strtoul(metric_str, &endptr, 10);
    if (errno || *endptr) {
        log_error("Invalid metric: %s", metric_str);
        goto err;
    }

    id1 = route_graph_get_node_id(g, &node1);
    if (id1 < 0) {
        goto err;
    }
    id2 = route_graph_get_node_id(g, &node2);
    if (id2 < 0) {
        goto err;
    }
    if (id1 == id2) {
        log_error("the same node");
        goto err;
    }
    if (route_graph_add_metric(g, (uint32_t)id1, (uint32_t)id2, metric) ||
        route_graph_add_metric(g, (uint32_t)id2, (uint32_t)id1, metric)) {
        goto err;
    }

    return 0;
err:
    return -1;
}

// See dijkstra algorithm.
static struct array *route_graph_find_distance(struct route_graph *g,
                                               uint32_t src)
{
    struct array *distance;
    struct array *q;
    uint32_t i;

    if (src >= array_n(&g->nodes)) {
        goto err;
    }

    distance = array_create(array_n(&g->nodes), sizeof(unsigned long));
    if (!distance) {
        log_error("oom");
        goto err;
    }
    q = array_create(array_n(&g->nodes), sizeof(uint32_t));
    if (!q) {
        log_error("oom");
        goto err2;
    }

    for (i = 0; i < array_n(&g->nodes); ++i) {
        *(uint32_t *)array_push(q) = i;
        *(unsigned long *)array_push(distance) = ULONG_MAX;
    }

    *(unsigned long *)array_get(distance, src) = 0;

    while (array_n(q) != 0) {
        uint32_t min_q_id = 0;
        unsigned long min_distance = *(unsigned long *)array_get(distance, *(uint32_t *)array_get(q, 0));
        uint32_t min_id;

        for (i = 1; i < array_n(q); ++i) {
            uint32_t id = *(uint32_t *)array_get(q, i);
            if (*(unsigned long *)array_get(distance, id) < min_distance) {
                min_distance = *(unsigned long *)array_get(distance, id);
                min_q_id = i;
            }
        }
        min_id = *(uint32_t *)array_get(q, min_q_id);
        if (min_q_id != array_n(q) - 1) {
            *(uint32_t *)array_get(q, min_q_id) = *(uint32_t *)array_get(q, array_n(q) - 1);
        }
        array_pop(q);

        struct array *row = array_get(&g->distance_matrix, min_id);
        for (i = 0; i < array_n(row); ++i) {
            if (*(unsigned long *)array_get(row, i) != ULONG_MAX) {
                unsigned long alt = min_distance + *(unsigned long *)array_get(row, i);
                if (alt < *(unsigned long *)array_get(distance, i)) {
                    *(unsigned long *)array_get(distance, i) = alt;
                }
            }
        }
    }

    array_destroy(q);

    return distance;
err2:
    array_destroy(distance);
err:
    return NULL;
}

struct route_id_distance {
    uint32_t id;
    unsigned long distance;
};

static int route_compare_distance(const void *a, const void *b)
{
    const struct route_id_distance *p1 = a;
    const struct route_id_distance *p2 = b;

    if (p1->distance == p2->distance) {
        return 0;
    } else if (p1->distance < p2->distance) {
        return -1;
    } else {
        return 1;
   }
}

static int route_graph_compile_one(struct route_graph *g, struct array *a,
                                   uint32_t src_id)
{
    struct array *distance, *id_distance;
    uint32_t i;

    if (!a) {
        goto err;
    }
    if (array_init(a, array_n(&g->nodes), sizeof(struct array)) != NC_OK) {
        goto err;
    }

    distance = route_graph_find_distance(g, src_id);
    if (!distance) {
        goto err;
    }

    id_distance = array_create(array_n(&g->nodes),
                               sizeof(struct route_id_distance));
    if (!id_distance) {
        goto err2;
    }

    for (i = 0; i < array_n(distance); ++i) {
        struct route_id_distance *pair = array_push(id_distance);
        pair->id = i;
        pair->distance = *(unsigned long *)array_get(distance, i);
    }

    array_sort(id_distance, route_compare_distance);

    for (i = 0; i < array_n(id_distance); ++i) {
        const struct route_id_distance *pair = array_get(id_distance, i);
        if (i == 0 ||
            pair->distance != ((const struct route_id_distance *)array_get(id_distance, i - 1))->distance) {
            struct array *list = array_push(a);
            if (array_init(list, array_n(&g->nodes), sizeof(uint32_t)) != NC_OK) {
                goto err3;
            }
        }
        struct array *last = array_get(a, array_n(a) - 1);
        *(uint32_t *)array_push(last) = pair->id;
    }

    array_destroy(id_distance);
    array_destroy(distance);

    return 0;
err3:
    array_destroy(id_distance);
err2:
    array_destroy(distance);
err:
    return -1;
}

static int route_graph_compile(struct route_graph *g)
{
    uint32_t i;

    for (i = 0; i < array_n(&g->nodes); ++i) {
        if (route_graph_compile_one(g, array_push(&g->prefer_cube), i)) {
            goto err;
        }
    }

    return 0;
err:
    return -1;
}

struct route_graph *route_graph_load(const char *filename)
{
    struct route_graph *g = route_graph_create();
    FILE *fp;
    char buf[LINE_MAX];

    assert(filename);

    if (!g) {
        log_error("oom");
        goto err;
    }
    fp = fopen(filename, "rb");
    if (!fp) {
        log_error("Failed to open: %s", filename);
        goto err2;
    }
    while (fgets(buf, sizeof(buf), fp)) {
        char *n1, *n2, *metric, *saveptr;
        char *ptr = buf + strspn(buf, " \t\r\n");
        /* Ignore blank lines and comments */
        if (!*ptr || *ptr == '#') {
            continue;
        }
        n1 = strtok_r(ptr, " \t\r\n", &saveptr);
        if (!n1) {
            log_error("No node 1");
            goto err3;
        }
        n2 = strtok_r(NULL, " \t\r\n", &saveptr);
        if (!n2) {
            log_error("No node 2");
            goto err3;
        }
        metric = strtok_r(NULL, " \t\r\n", &saveptr);
        if (!metric) {
            log_error("No metric");
            goto err3;
        }
        if (route_graph_add_rule(g, n1, n2, metric)) {
            goto err3;
        }
    }
    if (!feof(fp) || ferror(fp)) {
        log_error("Failed to read: %s", filename);
        goto err3;
    }
    if (route_graph_compile(g)) {
        log_error("oom");
        goto err3;
    }
    fclose(fp);

    return g;
err3:
    fclose(fp);
err2:
    route_graph_free(g);
err:
    return NULL;
}

static inline bool ip_match(uint32_t ip, uint32_t net, uint32_t mask)
{
    return ((ip ^ net) & mask) == 0;
}

bool route_graph_seek(struct route_graph *g, uint32_t local_ip,
                      struct route_iterator *it)
{
    uint32_t i;
    struct route_node *n;

    for (i = 0; i < array_n(&g->nodes); ++i) {
        n = array_get(&g->nodes, i);
        if (ip_match(local_ip, n->net, n->mask)) {
            it->node_id = i;
            it->metric = 0;
            return true;
        }
    }

    return false;
}

bool route_graph_match(struct route_graph *g, uint32_t peer_ip, struct
                       route_iterator *it)
{
    struct array *metric, *nodes;
    uint32_t i;
    struct route_node *n;

    metric = array_get(&g->prefer_cube, it->node_id);
    nodes = array_get(metric, it->metric);
    for (i = 0; i < array_n(nodes); ++i) {
        uint32_t id = *(uint32_t *)array_get(nodes, i);
        n = array_get(&g->nodes, id);
        if (ip_match(peer_ip, n->net, n->mask)) {
            return true;
        }
    }

    return false;
}

bool route_graph_next(struct route_graph *g, struct route_iterator *it)
{
    struct array *metric;

    metric = array_get(&g->prefer_cube, it->node_id);
    if (it->metric >= array_n(metric) - 1) {
        return false;
    }
    ++it->metric;

    return true;
}

static void route_graph_print_node(struct route_graph *g, uint32_t id)
{
    struct route_node *node;
    char buf[INET_ADDRSTRLEN];
    int prefix_len;
    uint32_t net;

    assert(id <= array_n(&g->nodes));
    node = array_get(&g->nodes, id);
    net = htonl(node->net);
    inet_ntop(AF_INET, &net, buf, sizeof(buf));
    prefix_len = ffs((int)node->mask);
    if (prefix_len != 0) {
        prefix_len = 33 - prefix_len;
    }
    fprintf(stderr, "%s/%d", buf, prefix_len);
}

void route_graph_dump(struct route_graph *g)
{
    struct array *a;
    uint32_t i, j, k;

    for (i = 0; i < array_n(&g->prefer_cube); ++i) {
        a = array_get(&g->prefer_cube, i);
        route_graph_print_node(g, i);
        fprintf(stderr, ": ");
        for (j = 0; j < array_n(a); ++j) {
            struct array *b = array_get(a, j);
            for (k = 0; k < array_n(b); ++k) {
                if (k != 0) {
                    putchar(',');
                }
                route_graph_print_node(g, *(uint32_t *)array_get(b, k));
            }
            fprintf(stderr, "; ");
        }
        putc('\n', stderr);
    }
}

void route_graph_test(void)
{
    struct route_graph *g = route_graph_load("route.conf");
    assert(g);
    struct array *a = route_graph_find_distance(g, 0);
    assert(a);
    uint32_t i, j, k;
    struct route_iterator it;
    uint32_t local_ip = 0x0a010001;
    uint32_t peer_ip = 0x0a040001;

    for (i = 0; i < array_n(a); ++i) {
        printf("%u: %u\n", i, *(uint32_t *)array_get(a, i));
    }
    array_destroy(a);

    printf("---\n");
    for (i = 0; i < array_n(&g->prefer_cube); ++i) {
        a = array_get(&g->prefer_cube, i);
        printf("%u: ", i);
        for (j = 0; j < array_n(a); ++j) {
            struct array *b = array_get(a, j);
            for (k = 0; k < array_n(b); ++k) {
                if (k != 0) {
                    putchar(',');
                }
                printf("%u", *(uint32_t *)array_get(b, k));
            }
            printf("; ");
        }
        putchar('\n');
    }

    printf("---\n");
    route_graph_dump(g);

    assert(route_graph_seek(g, local_ip, &it));
    do {
        if (route_graph_match(g, peer_ip, &it)) {
            printf("hit\n");
            break;
        } else {
            printf("miss\n");
        }
    } while (route_graph_next(g, &it));

    route_graph_free(g);
}