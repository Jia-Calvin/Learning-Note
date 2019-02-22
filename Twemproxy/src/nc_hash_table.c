#include <hashkit/nc_hashkit.h>
#include <nc_hash_table.h>

struct hash_node {
    struct string key;
    void *value;
    struct hash_node *prev;
    struct hash_node *next;
};

static void hash_node_init(struct hash_node *node)
{
    string_init(&(node->key));
    node->value = NULL;
    node->prev = NULL;
    node->next = NULL;
}

static struct hash_node* hash_node_alloc(void)
{
    struct hash_node* node = nc_alloc(sizeof(struct hash_node));
    if (!node) {
        return NULL;
    }
    hash_node_init(node);
    return node;
}

static void hash_node_free(struct hash_node *node)
{
    nc_free(node);
}

uint32_t hash_table_size(const struct hash_table *htb)
{
    return htb->size;
}

rstatus_t hash_table_init(struct hash_table *htb, uint32_t len)
{
    uint32_t i;
    htb->buckets = nc_alloc(sizeof(struct hash_node) * len);
    htb->bucket_len = len;
    htb->size = 0;
    if (!htb->buckets) {
        htb->bucket_len = 0;
        return NC_ENOMEM;
    }
    for (i = 0; i < len; ++i) {
        hash_node_init(htb->buckets + i);
    }
    return NC_OK;
}

void hash_table_deinit(struct hash_table *htb)
{
    if (htb->buckets) {
        nc_free(htb->buckets);
        htb->buckets = NULL;
        htb->bucket_len = 0;
        htb->size = 0;
    }
}

rstatus_t hash_table_set(struct hash_table *htb, const struct string k, void* v)
{
    uint32_t i = hash_crc32((const char*)k.data, (size_t)k.len);
    struct hash_node *head = htb->buckets + (i % htb->bucket_len);
    struct hash_node *node = head->next;
    while (node) {
        if (string_compare(&(node->key), &k) == 0) {
            node->value = v;
            return NC_OK;
        }
        node = node->next;
    }
    node = hash_node_alloc();
    if (!node) {
        return NC_ENOMEM;
    }
    node->key = k;
    node->value = v;
    node->prev = head;
    node->next = head->next;
    head->next = node;
    htb->size += 1;
    return NC_OK;
}

static struct hash_node*
hash_table_find(const struct hash_table *htb, const struct string k)
{
    if (htb->buckets) {
        uint32_t i = hash_crc32((const char*)k.data, (size_t)k.len);
        struct hash_node *node = htb->buckets[i % htb->bucket_len].next;
        while (node) {
            if (string_compare(&(node->key), &k) == 0) {
                return node;
            }
            node = node->next;
        }
    }
    return NULL;
}

void* hash_table_get(const struct hash_table *htb, const struct string k)
{
    struct hash_node* node = hash_table_find(htb, k);
    return node ? node->value : NULL;
}

void* hash_table_pop(struct hash_table *htb, const struct string k)
{
    struct hash_node* node = hash_table_find(htb, k);
    if (node) {
        void* value = node->value;
        if (node->prev) {
            node->prev->next = node->next;
        }
        if (node->next) {
            node->next->prev = node->prev;
        }
        hash_node_free(node);
        htb->size -= 1;
        return value;
    }
    return NULL;
}

