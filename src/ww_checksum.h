#ifndef WW_CHECKSUM_H
#define WW_CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

typedef struct WwCrc16 {
    uint16_t polynomial;
    uint16_t table[256];
} WwCrc16;

void ww_crc16_init(WwCrc16 *crc, uint16_t polynomial);
uint16_t ww_crc16_update(const WwCrc16 *crc, uint16_t value,
                         const void *data, size_t size);

#endif

