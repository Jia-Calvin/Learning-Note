#include <stdio.h>
#include <ctype.h>

#include <nc_blacklist.h>
#include <nc_core.h>

struct blacklist_entry {
    SLIST_ENTRY(blacklist_entry) link;
    uint8_t *key;
    size_t key_len;
};

SLIST_HEAD( , blacklist_entry) blacklist_head =
        SLIST_HEAD_INITIALIZER(blacklist_head);

static bool blacklist_insert(uint8_t *key, size_t key_len)
{
    struct blacklist_entry *entry;

    entry = malloc(sizeof(*entry));
    if (!entry)
        goto err;
    entry->key = nc_strndup(key, key_len);
    if (!entry->key)
        goto err2;
    entry->key_len = nc_strlen(entry->key);
    SLIST_INSERT_HEAD(&blacklist_head, entry, link);

    return true;
err2:
    free(entry);
err:
    return false;
}

bool blacklist_init(const char *filename)
{
    uint8_t buf[LINE_MAX];
    FILE *fp = fopen(filename, "rb");
    size_t len;

    if (!fp)
        goto err;
    while (fgets((char *)buf, sizeof(buf), fp)) {
        len = nc_strlen(buf);
        /* Remove the trailing spaces. */
        while (len > 0 && isspace(buf[len - 1]))
            --len;
        if (len == 0)
            continue;
        if (!blacklist_insert(buf, len))
            goto err2;
    }
    if (ferror(fp) || !feof(fp)) {
        goto err2;
    }
    fclose(fp);

    return true;
err2:
    fclose(fp);
err:
    return false;
}

bool blacklist_has(uint8_t *key, size_t key_len)
{
    struct blacklist_entry *entry;

    SLIST_FOREACH(entry, &blacklist_head, link) {
        if (entry->key_len == key_len &&
            memcmp(entry->key, key, key_len) == 0) {
            return true;
        }
    }

    return false;
}
