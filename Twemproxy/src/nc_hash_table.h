#ifndef _NC_HASH_TABLE_H_
#define _NC_HASH_TABLE_H_

#include <nc_core.h>

struct hash_node;

struct hash_table {
    struct hash_node *buckets;
    uint32_t bucket_len;
    uint32_t size;
};

rstatus_t hash_table_init(struct hash_table *htb, uint32_t len);
void hash_table_deinit(struct hash_table *htb);
rstatus_t hash_table_set(struct hash_table *htb, const struct string k, void* v);
void* hash_table_get(const struct hash_table *htb, const struct string k);
void* hash_table_pop(struct hash_table *htb, const struct string k);
uint32_t hash_table_size(const struct hash_table *htb);

#endif
