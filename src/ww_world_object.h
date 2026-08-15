#ifndef WW_WORLD_OBJECT_H
#define WW_WORLD_OBJECT_H

#include "ww_archive.h"
#include "ww_render_queue.h"
#include "ww_renderer.h"
#include "ww_sprite.h"
#include "ww_track.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_WORLD_SPRITE_DESCRIPTORS = 36,
    WW_WORLD_SPRITE_RECORD_BYTES = 40
};

typedef struct WwWorldSpriteDescriptor {
    int16_t classification;
    uint16_t scale_levels;
    uint16_t source_width;
    uint16_t source_height;
    uint16_t collision_width;
    uint16_t collision_height;
    uint16_t scale_bank;
    uint16_t frame_count;
    char asset_name[WW_ARCHIVE_NAME_BYTES + 1];
    const uint8_t *sprites;
    size_t sprites_size;
} WwWorldSpriteDescriptor;

typedef struct WwWorldSpriteCatalog {
    WwWorldSpriteDescriptor descriptor[WW_WORLD_SPRITE_DESCRIPTORS];
    bool loaded;
} WwWorldSpriteCatalog;

bool ww_world_sprite_catalog_load(WwWorldSpriteCatalog *catalog,
                                  const WwArchive *archive);
bool ww_world_object_enqueue(
    const WwTrack *track, const WwWorldSpriteCatalog *catalog,
    const WwSpriteScaleSet scale_set[], size_t scale_set_count,
    const WwRenderer *renderer, uint16_t camera_x, uint16_t camera_y,
    uint16_t camera_heading, WwRenderQueue *queue);
bool ww_world_object_update(WwTrack *track,
                            const WwWorldSpriteCatalog *catalog);

#endif
