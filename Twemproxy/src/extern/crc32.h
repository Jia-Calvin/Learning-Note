#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>


void crc32_init();
uint32_t crc32_checksum(const char *buf, int len);

#endif

