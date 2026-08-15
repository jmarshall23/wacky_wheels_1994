#ifndef WW_TRACK_H
#define WW_TRACK_H

#include "ww_archive.h"
#include "ww_pcx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_TRACK_IMAGE_NAMES = 4,
    WW_TRACK_PARAMETER_PAIRS = 8,
    WW_TRACK_TILE_COUNT = 64 * 64,
    WW_TRACK_PAR_BYTES = 320 * 80,
    WW_TRACK_POSITION_BYTES = 0x2000,
    WW_TRACK_SIN_RECORDS = 108,
    WW_TRACK_ATLAS_IMAGES = 4,
    WW_TRACK_TILES_PER_IMAGE = 54,
    WW_TRACK_TILES_PER_BANK = 108,
    WW_TRACK_TILE_WIDTH = 32,
    WW_TRACK_TILE_HEIGHT = 32,
    WW_TRACK_TILE_BYTES = WW_TRACK_TILE_WIDTH * WW_TRACK_TILE_HEIGHT,
    WW_TRACK_TILE_BANK_BYTES = WW_TRACK_TILES_PER_BANK * WW_TRACK_TILE_BYTES,
    WW_TRACK_ALL_TILE_BYTES = 2 * WW_TRACK_TILE_BANK_BYTES,
    WW_TRACK_SURFACE_CLASSES = 32,
    WW_TRACK_SDX_RECORD_BYTES = 0x12e
};

/* Text fields read by sub_2717C.  Names whose purpose is not yet proven by
 * their consumers deliberately retain neutral labels. */
typedef struct WwTrackGameDefinition {
    char image_name[WW_TRACK_IMAGE_NAMES][WW_ARCHIVE_NAME_BYTES + 1];
    char alternate_image_name[WW_TRACK_IMAGE_NAMES][WW_ARCHIVE_NAME_BYTES + 1];
    char background_name[WW_ARCHIVE_NAME_BYTES + 1];
    int16_t origin[2];
    char sub_2717c_line_7[32];
    int16_t parameter_pair[WW_TRACK_PARAMETER_PAIRS][2];
    bool has_special_parameters;
    int16_t special_parameter[3];
} WwTrackGameDefinition;

typedef struct WwRoadSegment {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    int16_t sub_279e8_value_4;
    int16_t sub_279e8_value_5;
    int16_t sub_279e8_value_6;
    uint32_t point_offset;
    uint16_t point_count;
} WwRoadSegment;

typedef struct WwTrackPoint {
    int16_t x;
    int16_t y;
} WwTrackPoint;

typedef struct WwSpawnRecord {
    int16_t sprite_type;
    int16_t state;
    int16_t world_x;
    int16_t world_y;
    int16_t sub_21490_value_4;
    int16_t animation_frame;
} WwSpawnRecord;

typedef struct WwSinRecord {
    uint32_t sub_279e8_discarded;
    uint32_t value[2];
} WwSinRecord;

typedef struct WwTrackSurfaceSample {
    uint8_t tile_id;
    uint8_t color;
    uint8_t mask;
    uint32_t sub_37afc_value;
} WwTrackSurfaceSample;

typedef struct WwTrack {
    char base_name[WW_ARCHIVE_NAME_BYTES + 1];
    WwTrackGameDefinition game;
    WwRoadSegment *road_segments;
    uint16_t road_segment_count;
    WwTrackPoint *road_points;
    size_t road_point_count;
    WwSpawnRecord *spawn_records;
    uint16_t spawn_record_count;
    uint8_t tile_index[WW_TRACK_TILE_COUNT];
    uint32_t tile_offset[WW_TRACK_TILE_COUNT];
    uint8_t par_bytes[WW_TRACK_PAR_BYTES];
    uint8_t position_map[WW_TRACK_POSITION_BYTES];
    int16_t position_count;
    int16_t position_quarter_count;
    WwSinRecord sin_records[WW_TRACK_SIN_RECORDS];
    /* sub_28180's WACKY.SDX records.  sub_237E4 copies +120 into racer
     * +36; sub_224EC removes (velocity * coefficient) >> 16 before moving. */
    uint32_t surface_drag[WW_TRACK_SURFACE_CLASSES];
    uint16_t surface_class_count;
    WwPcxImage background;
    /* sub_277B8/sub_376D7 build one contiguous 0x36000-byte allocation.
     * The first 108 tiles are color pixels and the second 108, at the exact
     * original +0x1B000 displacement, are surface/classification pixels. */
    uint8_t *tile_pixels;
    uint8_t tile_palette[WW_PALETTE_BYTES];
} WwTrack;

bool ww_track_load(WwTrack *track, const WwArchive *archive, const char *base_name);
void ww_track_close(WwTrack *track);
size_t ww_track_rasterize_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               WwTrackPoint *points, size_t capacity);
bool ww_track_surface_sample(const WwTrack *track, uint16_t x, uint16_t y,
                             WwTrackSurfaceSample *sample);
bool ww_track_position_sample(const WwTrack *track, uint16_t x, uint16_t y,
                              int16_t *position);

#endif
