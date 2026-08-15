#ifndef WW_SPRITE_H
#define WW_SPRITE_H

#include "ww_archive.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_SPRITE_SCALE_LEVELS = 10,
    WW_CAR_SOURCE_WIDTH = 38,
    WW_CAR_SOURCE_HEIGHT = 28,
    WW_CAR_SOURCE_BYTES = WW_CAR_SOURCE_WIDTH * WW_CAR_SOURCE_HEIGHT,
    WW_CAR_VEHICLES = 8,
    WW_CAR_FRAMES = 12,
    WW_CAR_BYTES = WW_CAR_VEHICLES * WW_CAR_FRAMES * WW_CAR_SOURCE_BYTES
};

typedef struct WwSpriteScale {
    uint16_t width;
    uint16_t height;
    const uint8_t *offset_data;
    size_t offset_count;
} WwSpriteScale;

typedef struct WwSpriteScaleSet {
    WwSpriteScale level[WW_SPRITE_SCALE_LEVELS];
} WwSpriteScaleSet;

bool ww_sprite_scale_set_load(WwSpriteScaleSet *set,
                              const WwArchive *archive,
                              const char *asset_name);
bool ww_sprite_draw_scaled_column_major(uint8_t *pixels, size_t pitch,
                                        unsigned screen_width,
                                        unsigned screen_height,
                                        int x, int y,
                                        const uint8_t *source,
                                        size_t source_size,
                                        const WwSpriteScale *scale,
                                        uint8_t transparent_color);

#endif
