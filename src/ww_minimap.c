#include "ww_minimap.h"

#include <string.h>

static unsigned ww_minimap_axis_index(uint16_t coordinate)
{
    int index = (int)coordinate - 0x400;
    if (index < 0) return 0u;
    if (index >= WW_MINIMAP_AXIS_ENTRIES) {
        return WW_MINIMAP_AXIS_ENTRIES - 1u;
    }
    return (unsigned)index;
}

bool ww_minimap_load(WwMinimap *minimap, const WwArchive *archive,
                     const char *marker_asset, unsigned race_index,
                     bool enabled)
{
    WwArchiveView view;
    size_t map_offset;
    if (minimap == NULL || archive == NULL || marker_asset == NULL ||
        race_index >= 5u) {
        return false;
    }
    memset(minimap, 0, sizeof(*minimap));
    minimap->enabled = enabled;
    if (!enabled) return true;

    /* sub_279E8 reads one 0xf3c-byte cell from the board's marker sheet.
     * BRONZEM.SP, SILVERM.SP, and the other four sheets each concatenate
     * five 78x50 column-major little maps. SHRINK<n>.PCX is a separate
     * full-screen preview and is never used by sub_12F80/sub_2A4AC. */
    map_offset = (size_t)race_index * WW_MINIMAP_WIDTH * WW_MINIMAP_HEIGHT;
    if (!ww_archive_view(archive, marker_asset, &view) ||
        map_offset + WW_MINIMAP_WIDTH * WW_MINIMAP_HEIGHT > view.size) {
        return false;
    }
    minimap->background = view.data + map_offset;
    minimap->background_size = WW_MINIMAP_WIDTH * WW_MINIMAP_HEIGHT;
    if (!ww_archive_view(archive, "MAP.XY", &view) ||
        view.size != WW_MINIMAP_MAP_BYTES) {
        ww_minimap_close(minimap);
        return false;
    }
    /* sub_28180 stores the first half as the X table and the second half as
     * the Y table. Both are byte lookups, not little-endian words. */
    minimap->x_lookup = view.data;
    minimap->y_lookup = view.data + WW_MINIMAP_AXIS_ENTRIES;
    minimap->map_size = view.size;
    minimap->loaded = true;
    return true;
}

void ww_minimap_close(WwMinimap *minimap)
{
    if (minimap == NULL) return;
    memset(minimap, 0, sizeof(*minimap));
}

bool ww_minimap_draw(const WwMinimap *minimap, WwDisplay *display,
                     const WwRacerState racers[WW_RACER_COUNT])
{
    unsigned racer_index;
    if (minimap == NULL || display == NULL || racers == NULL) return false;
    if (!minimap->enabled) return true;
    if (!minimap->loaded || minimap->background == NULL ||
        minimap->background_size != WW_MINIMAP_WIDTH * WW_MINIMAP_HEIGHT ||
        minimap->x_lookup == NULL || minimap->y_lookup == NULL ||
        minimap->map_size != WW_MINIMAP_MAP_BYTES) {
        return false;
    }

    if (ww_display_draw_pixels(display) == NULL) return false;
    ww_display_blit_column_major(display, WW_MINIMAP_SCREEN_X,
                                 WW_MINIMAP_SCREEN_Y,
                                 WW_MINIMAP_WIDTH, WW_MINIMAP_HEIGHT,
                                 minimap->background,
                                 WW_MINIMAP_HEIGHT, -1);
    for (racer_index = 0; racer_index < WW_RACER_COUNT; ++racer_index) {
        unsigned x_index;
        unsigned y_index;
        int x;
        int y;
        if (!racers[racer_index].active) continue;
        x_index = ww_minimap_axis_index(racers[racer_index].world_x);
        y_index = ww_minimap_axis_index(racers[racer_index].world_y);
        x = WW_MINIMAP_SCREEN_X + minimap->x_lookup[x_index];
        y = WW_MINIMAP_SCREEN_Y + minimap->y_lookup[y_index];
        ww_display_put_pixel(display, x, y,
                             racer_index == WW_RACER_PLAYER_INDEX
                                 ? 0xffu : 0x5au);
    }
    return true;
}
