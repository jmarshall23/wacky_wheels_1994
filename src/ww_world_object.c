#include "ww_world_object.h"

#include "ww_common.h"
#include "ww_perspective.h"

#include <string.h>

bool ww_world_sprite_catalog_load(WwWorldSpriteCatalog *catalog,
                                  const WwArchive *archive)
{
    WwArchiveView attributes;
    WwWorldSpriteCatalog loaded;
    uint16_t count;
    unsigned i;
    if (catalog == NULL || archive == NULL ||
        !ww_archive_view(archive, "SPRITE.ATR", &attributes) ||
        attributes.size < 2u) {
        return false;
    }
    count = ww_read_le16(attributes.data);
    if (count != WW_WORLD_SPRITE_DESCRIPTORS ||
        attributes.size != 2u + (size_t)count * WW_WORLD_SPRITE_RECORD_BYTES) {
        return false;
    }
    memset(&loaded, 0, sizeof(loaded));
    for (i = 0; i < count; ++i) {
        const uint8_t *record = attributes.data + 2u +
                                (size_t)i * WW_WORLD_SPRITE_RECORD_BYTES;
        WwWorldSpriteDescriptor *descriptor = &loaded.descriptor[i];
        WwArchiveView sprites;
        size_t name_length = 0;
        size_t expected_size;
        descriptor->classification = (int16_t)ww_read_le16(record);
        descriptor->scale_levels = ww_read_le16(record + 2u);
        descriptor->source_width = ww_read_le16(record + 4u);
        descriptor->source_height = ww_read_le16(record + 6u);
        descriptor->collision_width = ww_read_le16(record + 8u);
        descriptor->collision_height = ww_read_le16(record + 10u);
        descriptor->scale_bank = ww_read_le16(record + 12u);
        descriptor->frame_count = ww_read_le16(record + 14u);
        while (name_length < 24u && record[16u + name_length] != 0) {
            if (name_length >= WW_ARCHIVE_NAME_BYTES) {
                return false;
            }
            descriptor->asset_name[name_length] =
                (char)record[16u + name_length];
            ++name_length;
        }
        descriptor->asset_name[name_length] = '\0';
        if (name_length == 0 || name_length == 24u ||
            descriptor->scale_levels != WW_SPRITE_SCALE_LEVELS ||
            descriptor->source_width == 0 ||
            descriptor->source_height == 0 || descriptor->frame_count == 0 ||
            descriptor->scale_bank >= 6u ||
            !ww_archive_view(archive, descriptor->asset_name, &sprites)) {
            return false;
        }
        expected_size = (size_t)descriptor->source_width *
                        descriptor->source_height * descriptor->frame_count;
        if (sprites.size != expected_size) {
            return false;
        }
        descriptor->sprites = sprites.data;
        descriptor->sprites_size = sprites.size;
    }
    loaded.loaded = true;
    *catalog = loaded;
    return true;
}

/* Static .SPW producer translated from sub_25A78. */
bool ww_world_object_enqueue(
    const WwTrack *track, const WwWorldSpriteCatalog *catalog,
    const WwSpriteScaleSet scale_set[], size_t scale_set_count,
    const WwRenderer *renderer, uint16_t camera_x, uint16_t camera_y,
    uint16_t camera_heading, WwRenderQueue *queue)
{
    size_t i;
    if (track == NULL || catalog == NULL || !catalog->loaded ||
        scale_set == NULL || renderer == NULL || queue == NULL) {
        return false;
    }
    for (i = 0; i < track->spawn_record_count; ++i) {
        const WwSpawnRecord *spawn = &track->spawn_records[i];
        const WwWorldSpriteDescriptor *descriptor;
        WwPerspectiveProjection projection;
        const WwSpriteScale *scale;
        WwRenderQueueItem item;
        size_t source_frame_bytes;
        size_t source_offset;
        int type = spawn->sprite_type;
        int frame = spawn->animation_frame;
        if (spawn->state == -1) {
            continue;
        }
        if (type < 0 || type >= WW_WORLD_SPRITE_DESCRIPTORS) {
            return false;
        }
        descriptor = &catalog->descriptor[type];
        if (descriptor->scale_bank >= scale_set_count || frame < 0 ||
            frame >= descriptor->frame_count) {
            return false;
        }
        if (!ww_perspective_project(
                renderer, (uint16_t)spawn->world_x,
                (uint16_t)spawn->world_y, camera_x, camera_y,
                camera_heading, &projection)) {
            continue;
        }
        scale = &scale_set[descriptor->scale_bank]
                     .level[projection.scale_level];
        source_frame_bytes = (size_t)descriptor->source_width *
                             descriptor->source_height;
        source_offset = (size_t)frame * source_frame_bytes;
        if (source_offset + source_frame_bytes > descriptor->sprites_size) {
            return false;
        }
        memset(&item, 0, sizeof(item));
        item.x = (int16_t)(projection.center_x - (int)scale->width / 2);
        item.y = (int16_t)(
            (int)renderer->projection_right[projection.distance] -
            (int)scale->height);
        item.distance = projection.distance;
        item.source = descriptor->sprites + source_offset;
        item.source_size = descriptor->sprites_size - source_offset;
        item.scale = scale;
        if (!ww_render_queue_push(queue, &item)) {
            return false;
        }
    }
    return true;
}

/* First loop of sub_261FC: reset transient state and advance each static
 * sprite's animation frame once per game update. */
bool ww_world_object_update(WwTrack *track,
                            const WwWorldSpriteCatalog *catalog)
{
    size_t i;
    if (track == NULL || catalog == NULL || !catalog->loaded) {
        return false;
    }
    for (i = 0; i < track->spawn_record_count; ++i) {
        WwSpawnRecord *spawn = &track->spawn_records[i];
        int type = spawn->sprite_type;
        if (spawn->state == -1) {
            continue;
        }
        if (type < 0 || type >= WW_WORLD_SPRITE_DESCRIPTORS) {
            return false;
        }
        spawn->state = 0;
        ++spawn->animation_frame;
        if (spawn->animation_frame ==
            (int16_t)catalog->descriptor[type].frame_count) {
            spawn->animation_frame = 0;
        }
    }
    return true;
}
