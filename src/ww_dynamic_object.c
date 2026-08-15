#include "ww_dynamic_object.h"

#include "ww_common.h"
#include "ww_perspective.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum {
    WW_DYNAMIC_STATE_EXPLOSION = 0xff,
    WW_DYNAMIC_MAXIMUM_TRAVEL = 0x3e8,
    WW_DYNAMIC_BOUNCE_STEP = 0x64,
    WW_DYNAMIC_STATIC_COLLISION_DISTANCE = 0x0e,
    WW_DYNAMIC_RACER_COLLISION_DISTANCE = 0x12
};

static int16_t ww_dynamic_scaled_direction(const WwRenderer *renderer,
                                           uint16_t heading,
                                           unsigned component,
                                           int32_t scale)
{
    const uint8_t *entry = renderer->trig_data +
                           (size_t)heading * WW_TRIG_ENTRY_BYTES;
    int32_t direction = (int32_t)ww_read_le32(entry + component * 4u);
    int64_t product = (int64_t)direction * scale + 0x8000;
    return (int16_t)(product >> 16);
}

static unsigned ww_dynamic_manhattan(uint16_t ax, uint16_t ay,
                                     uint16_t bx, uint16_t by)
{
    int dx = (int)(int16_t)ax - (int)(int16_t)bx;
    int dy = (int)(int16_t)ay - (int)(int16_t)by;
    return (unsigned)(abs(dx) + abs(dy));
}

/* sub_25E6C interprets +1E as a three-way firing mode for object types zero
 * and five.  Other types store an absolute 0..0x77f heading there. */
static uint16_t ww_dynamic_effective_heading(const WwDynamicObject *object,
                                             uint16_t owner_heading)
{
    int heading;
    if (object->type != 0u && object->type != 5u) {
        return (uint16_t)(object->heading_or_mode % WW_TRIG_ENTRY_COUNT);
    }
    heading = owner_heading;
    if (object->heading_or_mode == 1u) {
        heading -= 0x1e;
    } else if (object->heading_or_mode == 2u) {
        heading += 0x1e;
    } else if (object->heading_or_mode != 0u) {
        heading = object->heading_or_mode;
    }
    while (heading < 0) heading += WW_TRIG_ENTRY_COUNT;
    while (heading >= WW_TRIG_ENTRY_COUNT) heading -= WW_TRIG_ENTRY_COUNT;
    return (uint16_t)heading;
}

bool ww_dynamic_sprite_assets_load(WwDynamicSpriteAssets *assets,
                                   const WwArchive *archive)
{
    WwDynamicSpriteAssets loaded;
    WwArchiveView view;
    if (assets == NULL || archive == NULL) return false;
    memset(&loaded, 0, sizeof(loaded));
    if (!ww_archive_view(archive, "HOGMIS.SP", &view) ||
        view.size != WW_DYNAMIC_HOGMIS_BYTES) {
        return false;
    }
    loaded.hogmis = view.data;
    loaded.hogmis_size = view.size;
    if (!ww_archive_view(archive, "SPARK.SP", &view) ||
        view.size != WW_DYNAMIC_SPARK_BYTES) {
        return false;
    }
    loaded.spark = view.data;
    loaded.spark_size = view.size;
    loaded.loaded = true;
    *assets = loaded;
    return true;
}

void ww_dynamic_object_clear(WwDynamicObjectPool *pool)
{
    if (pool != NULL) memset(pool, 0, sizeof(*pool));
}

/* sub_2D548 allocation and initialization.  The caller supplies the exact
 * HOGMIS.SP slice and frame count chosen by sub_24364. */
bool ww_dynamic_object_spawn(WwDynamicObjectPool *pool,
                             unsigned owner_racer, unsigned type,
                             uint16_t world_x, uint16_t world_y,
                             uint16_t owner_heading,
                             uint16_t heading_or_mode,
                             size_t source_offset,
                             unsigned animation_frames,
                             const WwRenderer *renderer)
{
    size_t i;
    WwDynamicObject *object = NULL;
    if (pool == NULL || renderer == NULL || renderer->trig_data == NULL ||
        owner_racer >= WW_RACER_COUNT || type >= WW_DYNAMIC_OBJECT_TYPES ||
        owner_heading >= WW_TRIG_ENTRY_COUNT ||
        heading_or_mode >= WW_TRIG_ENTRY_COUNT || animation_frames == 0u ||
        animation_frames > UINT16_MAX ||
        source_offset + (size_t)animation_frames * WW_DYNAMIC_FRAME_BYTES >
            WW_DYNAMIC_HOGMIS_BYTES) {
        return false;
    }
    for (i = 0; i < WW_DYNAMIC_OBJECT_CAPACITY; ++i) {
        if (!pool->object[i].active) {
            object = &pool->object[i];
            break;
        }
    }
    if (object == NULL) return false;
    memset(object, 0, sizeof(*object));
    object->owner_racer = (uint16_t)owner_racer;
    object->type = (uint16_t)type;
    object->active = true;
    object->owner_contact_armed = 1u;
    object->world_x = world_x;
    object->world_y = world_y;
    object->animation_frames = (uint16_t)animation_frames;
    object->frame_stride = WW_DYNAMIC_FRAME_BYTES;
    object->maximum_travel = WW_DYNAMIC_MAXIMUM_TRAVEL;
    object->movement_step = (type == 1u || type == 6u) ? 0x10u : 0x0au;
    object->source_offset = source_offset;
    object->heading_or_mode = heading_or_mode;
    /* sub_2D548 calls sub_25E6C with collision checking disabled, placing
     * the new object one movement quantum beyond its owner's anchor. */
    {
        uint16_t effective_heading =
            ww_dynamic_effective_heading(object, owner_heading);
        object->world_x = (uint16_t)(object->world_x +
            ww_dynamic_scaled_direction(renderer, effective_heading, 0,
                                        object->movement_step));
        object->world_y = (uint16_t)(object->world_y +
            ww_dynamic_scaled_direction(renderer, effective_heading, 1,
                                        object->movement_step));
    }
    ++pool->active_by_owner[owner_racer];
    return true;
}

static unsigned ww_dynamic_collision_at(
    const WwDynamicObject *object, const WwTrack *track,
    uint16_t x, uint16_t y,
    const WwRacerState racers[WW_RACER_COUNT],
    uint16_t *collision_x, uint16_t *collision_y,
    uint16_t *collision_racer)
{
    WwTrackSurfaceSample surface;
    size_t i;
    if (!ww_track_surface_sample(track, x, y, &surface)) return UINT_MAX;
    if (surface.sub_37afc_value == 3u) return 1u;
    for (i = 0; i < track->spawn_record_count; ++i) {
        const WwSpawnRecord *spawn = &track->spawn_records[i];
        if (spawn->state != -1 &&
            ww_dynamic_manhattan(x, y, (uint16_t)spawn->world_x,
                                 (uint16_t)spawn->world_y) <
                WW_DYNAMIC_STATIC_COLLISION_DISTANCE) {
            return 2u;
        }
    }
    for (i = 0; i < WW_RACER_COUNT; ++i) {
        if (i != object->owner_racer && racers[i].active &&
            racers[i].hit_effect == 0u &&
            ww_dynamic_manhattan(x, y, racers[i].world_x,
                                 racers[i].world_y) <
                WW_DYNAMIC_RACER_COLLISION_DISTANCE) {
            *collision_x = racers[i].world_x;
            *collision_y = racers[i].world_y;
            *collision_racer = (uint16_t)i;
            return 3u;
        }
    }
    return 0u;
}

/* sub_25E6C's strict line walk.  On a collision, sub_218A4 leaves the line
 * cursor at the first hit point; racer hits additionally snap the object to
 * that racer's anchor. */
static bool ww_dynamic_move(WwDynamicObject *object, const WwTrack *track,
                            const WwRenderer *renderer,
                            const WwRacerState racers[WW_RACER_COUNT],
                            unsigned *collision,
                            uint16_t *collision_racer)
{
    WwTrackPoint points[WW_DYNAMIC_BOUNCE_STEP + 2u];
    int16_t end_x;
    int16_t end_y;
    size_t count;
    size_t i;
    uint16_t effective_heading = ww_dynamic_effective_heading(
        object, racers[object->owner_racer].heading);
    end_x = (int16_t)(object->world_x + ww_dynamic_scaled_direction(
        renderer, effective_heading, 0, object->movement_step));
    end_y = (int16_t)(object->world_y + ww_dynamic_scaled_direction(
        renderer, effective_heading, 1, object->movement_step));
    count = ww_track_rasterize_line((int16_t)object->world_x,
                                    (int16_t)object->world_y,
                                    end_x, end_y, points,
                                    sizeof(points) / sizeof(points[0]));
    if (count == 0u) return false;
    *collision = 0u;
    for (i = 0; i < count; ++i) {
        uint16_t hit_x = (uint16_t)points[i].x;
        uint16_t hit_y = (uint16_t)points[i].y;
        unsigned result = ww_dynamic_collision_at(
            object, track, hit_x, hit_y, racers, &hit_x, &hit_y,
            collision_racer);
        if (result == UINT_MAX) return false;
        object->world_x = hit_x;
        object->world_y = hit_y;
        if (result != 0u) {
            *collision = result;
            return true;
        }
    }
    return true;
}

static void ww_dynamic_begin_explosion(WwDynamicObjectPool *pool,
                                       WwDynamicObject *object)
{
    if (object->owner_racer < WW_RACER_COUNT &&
        pool->active_by_owner[object->owner_racer] != 0u) {
        --pool->active_by_owner[object->owner_racer];
    }
    object->state = WW_DYNAMIC_STATE_EXPLOSION;
    object->animation_frame = 0u;
    object->animation_frames = WW_DYNAMIC_EXPLOSION_FRAMES;
    object->frame_stride = WW_DYNAMIC_FRAME_BYTES;
    object->source_offset = 0u;
}

/* sub_2602C animation, movement, range expiry, type-1 bounce, and conversion
 * to the three-frame HOGMIS.SP explosion at offset zero. */
bool ww_dynamic_object_update(WwDynamicObjectPool *pool,
                              const WwDynamicSpriteAssets *assets,
                              const WwTrack *track,
                              const WwRenderer *renderer,
                              WwRacerState racers[WW_RACER_COUNT])
{
    size_t i;
    if (pool == NULL || assets == NULL || !assets->loaded || track == NULL ||
        renderer == NULL || renderer->trig_data == NULL || racers == NULL) {
        return false;
    }
    for (i = 0; i < WW_DYNAMIC_OBJECT_CAPACITY; ++i) {
        WwDynamicObject *object = &pool->object[i];
        unsigned collision;
        uint16_t collision_racer = UINT16_MAX;
        if (!object->active) continue;
        ++object->animation_frame;
        if (object->animation_frame >= object->animation_frames) {
            object->animation_frame = 0u;
            if (object->state == WW_DYNAMIC_STATE_EXPLOSION) {
                object->active = false;
                continue;
            }
        }
        if (object->state == WW_DYNAMIC_STATE_EXPLOSION) continue;
        if (!ww_dynamic_move(object, track, renderer, racers, &collision,
                             &collision_racer)) {
            return false;
        }
        if (object->type == 0u || object->type == 5u) {
            object->distance_traveled = (uint16_t)(
                object->distance_traveled + object->movement_step);
            object->movement_step = WW_DYNAMIC_BOUNCE_STEP;
            if (object->distance_traveled >= object->maximum_travel) {
                collision = 1u;
            }
        }
        if (object->type == 1u && (collision == 1u || collision == 2u)) {
            object->heading_or_mode = (uint16_t)(
                object->heading_or_mode + WW_TRIG_ENTRY_COUNT / 2u);
            if (object->heading_or_mode >= WW_TRIG_ENTRY_COUNT) {
                object->heading_or_mode = (uint16_t)(
                    object->heading_or_mode - WW_TRIG_ENTRY_COUNT);
            }
            object->movement_step = WW_DYNAMIC_BOUNCE_STEP;
            object->world_x = (uint16_t)(object->world_x +
                ww_dynamic_scaled_direction(renderer, object->heading_or_mode,
                                            0, object->movement_step));
            object->world_y = (uint16_t)(object->world_y +
                ww_dynamic_scaled_direction(renderer, object->heading_or_mode,
                                            1, object->movement_step));
            collision = 0u;
        }
        if (collision == 3u && collision_racer < WW_RACER_COUNT) {
            WwRacerState *target = &racers[collision_racer];
            target->hit_effect = object->type == 5u ? 2u : 1u;
            target->hit_direction_frame = ww_racer_relative_direction_frame(
                target->heading, racers[object->owner_racer].heading);
            target->hit_age = 0u;
        }
        if (collision != 0u) ww_dynamic_begin_explosion(pool, object);
    }
    return true;
}

/* sub_2632C uses the sixth sub_28038 bank: dword_6E470 + 5*0x50 is
 * dword_6E600, which is the ten records loaded from 18X13.INF. */
bool ww_dynamic_object_enqueue(const WwDynamicObjectPool *pool,
                               const WwDynamicSpriteAssets *assets,
                               const WwSpriteScaleSet *scale_set,
                               const WwRenderer *renderer,
                               uint16_t camera_x, uint16_t camera_y,
                               uint16_t camera_heading,
                               WwRenderQueue *queue)
{
    size_t i;
    if (pool == NULL || assets == NULL || !assets->loaded ||
        scale_set == NULL || renderer == NULL || queue == NULL) {
        return false;
    }
    for (i = 0; i < WW_DYNAMIC_OBJECT_CAPACITY; ++i) {
        const WwDynamicObject *object = &pool->object[i];
        WwPerspectiveProjection projection;
        WwRenderQueueItem item;
        const WwSpriteScale *scale;
        size_t frame_offset;
        if (!object->active) continue;
        if (!ww_perspective_project(renderer, object->world_x,
                                    object->world_y, camera_x, camera_y,
                                    camera_heading, &projection)) {
            continue;
        }
        scale = &scale_set->level[projection.scale_level];
        frame_offset = object->source_offset +
                       (size_t)object->animation_frame * object->frame_stride;
        if (frame_offset + object->frame_stride > assets->hogmis_size) {
            return false;
        }
        memset(&item, 0, sizeof(item));
        item.x = (int16_t)(projection.center_x - (int)scale->width / 2);
        item.y = (int16_t)((int)renderer->projection_right[
                               projection.distance] - (int)scale->height);
        item.distance = projection.distance;
        item.source = assets->hogmis + frame_offset;
        item.source_size = assets->hogmis_size - frame_offset;
        item.scale = scale;
        if (!ww_render_queue_push(queue, &item)) return false;
    }
    return true;
}
