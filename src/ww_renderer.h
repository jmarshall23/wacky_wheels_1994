#ifndef WW_RENDERER_H
#define WW_RENDERER_H

#include "ww_archive.h"
#include "ww_common.h"
#include "ww_track.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_PROJECTION_TABLE_COUNT = 0x1000,
    WW_TRIG_ENTRY_COUNT = 0x780,
    WW_TRIG_ENTRY_BYTES = 8,
    WW_TRIG_BYTES = WW_TRIG_ENTRY_COUNT * WW_TRIG_ENTRY_BYTES,
    WW_VIEW_ENTRY_COUNT = 320,
    WW_VIEW_ENTRY_BYTES = 4,
    WW_VIEW_BYTES = WW_VIEW_ENTRY_COUNT * WW_VIEW_ENTRY_BYTES,
    WW_ROAD_COLUMNS = 320,
    WW_ROAD_ROWS = 70,
    WW_NDIST_BYTES = WW_ROAD_COLUMNS * WW_ROAD_ROWS * 4,
    WW_PADDED_TILE_MAP_SIDE = 128,
    WW_TRACK_TILE_MAP_ORIGIN = 32
};

typedef struct WwRenderer {
    uint16_t projection_left[WW_PROJECTION_TABLE_COUNT];
    uint16_t projection_right[WW_PROJECTION_TABLE_COUNT];
    const uint8_t *trig_data;
    const uint8_t *ndist_data;
    const uint8_t *view_data;
} WwRenderer;

typedef enum WwRoadDetail {
    WW_ROAD_DETAIL_HIGH = 1,
    WW_ROAD_DETAIL_MEDIUM = 2,
    WW_ROAD_DETAIL_LOW = 3
} WwRoadDetail;

void ww_renderer_init(WwRenderer *renderer);
bool ww_renderer_load_assets(WwRenderer *renderer, const WwArchive *archive);
bool ww_renderer_render_road_split(const WwRenderer *renderer,
                                   const WwTrack *track,
                                   uint8_t *pixels, size_t pitch,
                                   bool lower_view, uint16_t heading,
                                   uint16_t origin_x, uint16_t origin_y,
                                   WwRoadDetail detail);
bool ww_renderer_render_horizon_split(const WwTrack *track,
                                      uint8_t *pixels, size_t pitch,
                                      bool lower_view,
                                      unsigned source_address_offset);

#endif
