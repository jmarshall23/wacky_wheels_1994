#ifndef WW_MINIMAP_H
#define WW_MINIMAP_H

#include "ww_archive.h"
#include "ww_display.h"
#include "ww_racer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_MINIMAP_WIDTH = 78,
    WW_MINIMAP_HEIGHT = 50,
    WW_MINIMAP_SCREEN_X = 0,
    WW_MINIMAP_SCREEN_Y = 0x3c,
    WW_MINIMAP_AXIS_ENTRIES = 0x800,
    WW_MINIMAP_MAP_BYTES = WW_MINIMAP_AXIS_ENTRIES * 2
};

typedef struct WwMinimap {
    const uint8_t *background;
    size_t background_size;
    const uint8_t *x_lookup;
    const uint8_t *y_lookup;
    size_t map_size;
    bool enabled;
    bool loaded;
} WwMinimap;

bool ww_minimap_load(WwMinimap *minimap, const WwArchive *archive,
                     const char *marker_asset, unsigned race_index,
                     bool enabled);
void ww_minimap_close(WwMinimap *minimap);
bool ww_minimap_draw(const WwMinimap *minimap, WwDisplay *display,
                     const WwRacerState racers[WW_RACER_COUNT]);

#endif
