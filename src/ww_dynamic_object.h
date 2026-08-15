#ifndef WW_DYNAMIC_OBJECT_H
#define WW_DYNAMIC_OBJECT_H

#include "ww_archive.h"
#include "ww_racer.h"
#include "ww_render_queue.h"
#include "ww_renderer.h"
#include "ww_sprite.h"
#include "ww_track.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_DYNAMIC_OBJECT_CAPACITY = 0x3d,
    WW_DYNAMIC_OBJECT_TYPES = 9,
    WW_DYNAMIC_HOGMIS_BYTES = 0x17c4,
    WW_DYNAMIC_SPARK_BYTES = 0x168,
    WW_DYNAMIC_FRAME_BYTES = 0xea,
    WW_DYNAMIC_EXPLOSION_FRAMES = 3
};

typedef struct WwDynamicSpriteAssets {
    const uint8_t *hogmis;
    size_t hogmis_size;
    const uint8_t *spark;
    size_t spark_size;
    bool loaded;
} WwDynamicSpriteAssets;

/* Named layout of the 0x22-byte records at unk_7F2B0. */
typedef struct WwDynamicObject {
    uint16_t owner_racer;
    uint16_t type;
    bool active;
    uint16_t state;
    uint16_t owner_contact_armed;
    uint16_t world_x;
    uint16_t world_y;
    uint16_t animation_frame;
    uint16_t animation_frames;
    uint16_t frame_stride;
    uint16_t maximum_travel;
    uint16_t movement_step;
    uint16_t distance_traveled;
    size_t source_offset;
    uint16_t heading_or_mode;
} WwDynamicObject;

typedef struct WwDynamicObjectPool {
    WwDynamicObject object[WW_DYNAMIC_OBJECT_CAPACITY];
    uint16_t active_by_owner[WW_RACER_COUNT];
} WwDynamicObjectPool;

bool ww_dynamic_sprite_assets_load(WwDynamicSpriteAssets *assets,
                                   const WwArchive *archive);
void ww_dynamic_object_clear(WwDynamicObjectPool *pool);
bool ww_dynamic_object_spawn(WwDynamicObjectPool *pool,
                             unsigned owner_racer, unsigned type,
                             uint16_t world_x, uint16_t world_y,
                             uint16_t owner_heading,
                             uint16_t heading_or_mode,
                             size_t source_offset,
                             unsigned animation_frames,
                             const WwRenderer *renderer);
bool ww_dynamic_object_update(WwDynamicObjectPool *pool,
                              const WwDynamicSpriteAssets *assets,
                              const WwTrack *track,
                              const WwRenderer *renderer,
                              WwRacerState racers[WW_RACER_COUNT]);
bool ww_dynamic_object_enqueue(const WwDynamicObjectPool *pool,
                               const WwDynamicSpriteAssets *assets,
                               const WwSpriteScaleSet *scale_set,
                               const WwRenderer *renderer,
                               uint16_t camera_x, uint16_t camera_y,
                               uint16_t camera_heading,
                               WwRenderQueue *queue);

#endif
