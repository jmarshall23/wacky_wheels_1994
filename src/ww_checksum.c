#include "ww_checksum.h"

/* Exact C translation of sub_10260, sub_10298 and sub_102CC. */

static uint16_t ww_crc16_table_entry(uint16_t polynomial, uint8_t index)
{
    uint16_t value = 0;
    uint16_t high = (uint16_t)index << 8;
    unsigned bit;
    for (bit = 0; bit < 8; ++bit) {
        uint16_t mixed = (uint16_t)(value ^ high);
        value = (uint16_t)(value << 1);
        if ((mixed & 0x8000u) != 0) {
            value ^= polynomial;
        }
        high = (uint16_t)(high << 1);
    }
    return value;
}

void ww_crc16_init(WwCrc16 *crc, uint16_t polynomial)
{
    unsigned i;
    crc->polynomial = polynomial;
    for (i = 0; i < 256; ++i) {
        crc->table[i] = ww_crc16_table_entry(polynomial, (uint8_t)i);
    }
}

uint16_t ww_crc16_update(const WwCrc16 *crc, uint16_t value,
                         const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    size_t i;
    for (i = 0; i < size; ++i) {
        uint8_t table_index = (uint8_t)((value >> 8) ^ bytes[i]);
        value = (uint16_t)((value << 8) ^ crc->table[table_index]);
    }
    return value;
}

