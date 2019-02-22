#ifndef _NC_BLACKLIST_H_
#define _NC_BLACKLIST_H_

#include <stdbool.h>
#include <stdint.h>

#include <sys/types.h>

bool blacklist_init(const char *filename);
bool blacklist_has(uint8_t *key, size_t key_len);

#endif
