#include "ww_pcx.h"

#include <stdlib.h>
#include <string.h>

enum {
    WW_PCX_HEADER_BYTES = 128,
    WW_PCX_PALETTE_RECORD_BYTES = 769
};

bool ww_pcx_decode(const uint8_t *data, size_t size, WwPcxImage *image)
{
    uint16_t x_min;
    uint16_t y_min;
    uint16_t x_max;
    uint16_t y_max;
    uint16_t bytes_per_line;
    uint8_t planes;
    size_t source_position;
    size_t palette_position;
    uint16_t y;

    if (data == NULL || image == NULL || size < WW_PCX_HEADER_BYTES) {
        return false;
    }
    memset(image, 0, sizeof(*image));
    if (data[0] != 0x0a || data[2] != 1 || data[3] != 8) {
        return false;
    }
    x_min = ww_read_le16(data + 4);
    y_min = ww_read_le16(data + 6);
    x_max = ww_read_le16(data + 8);
    y_max = ww_read_le16(data + 10);
    planes = data[65];
    bytes_per_line = ww_read_le16(data + 66);
    if (x_max < x_min || y_max < y_min || planes != 1 || bytes_per_line == 0) {
        return false;
    }
    image->width = (uint16_t)(x_max - x_min + 1);
    image->height = (uint16_t)(y_max - y_min + 1);
    if (image->width > bytes_per_line || image->width > 4096 || image->height > 4096) {
        return false;
    }
    image->pixels = (uint8_t *)malloc((size_t)image->width * image->height);
    if (image->pixels == NULL) {
        return false;
    }

    source_position = WW_PCX_HEADER_BYTES;
    palette_position = size >= WW_PCX_PALETTE_RECORD_BYTES
                           ? size - WW_PCX_PALETTE_RECORD_BYTES
                           : size;
    for (y = 0; y < image->height; ++y) {
        uint16_t decoded = 0;
        while (decoded < bytes_per_line) {
            uint8_t value;
            unsigned run = 1;
            if (source_position >= palette_position) {
                ww_pcx_free(image);
                return false;
            }
            value = data[source_position++];
            if ((value & 0xc0u) == 0xc0u) {
                run = value & 0x3fu;
                if (run == 0 || source_position >= palette_position) {
                    ww_pcx_free(image);
                    return false;
                }
                value = data[source_position++];
            }
            while (run-- != 0 && decoded < bytes_per_line) {
                if (decoded < image->width) {
                    image->pixels[(size_t)y * image->width + decoded] = value;
                }
                ++decoded;
            }
        }
    }

    if (size < WW_PCX_PALETTE_RECORD_BYTES || data[palette_position] != 0x0c) {
        ww_pcx_free(image);
        return false;
    }
    memcpy(image->palette, data + palette_position + 1, WW_PALETTE_BYTES);
    return true;
}

void ww_pcx_free(WwPcxImage *image)
{
    if (image == NULL) {
        return;
    }
    free(image->pixels);
    memset(image, 0, sizeof(*image));
}

