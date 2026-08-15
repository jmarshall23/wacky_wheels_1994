#ifndef WW_PCX_H
#define WW_PCX_H

#include "ww_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct WwPcxImage {
    uint16_t width;
    uint16_t height;
    uint8_t *pixels;
    uint8_t palette[WW_PALETTE_BYTES];
} WwPcxImage;

bool ww_pcx_decode(const uint8_t *data, size_t size, WwPcxImage *image);
void ww_pcx_free(WwPcxImage *image);

#endif

