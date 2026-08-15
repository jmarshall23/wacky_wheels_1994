#include "ww_duck.h"

#include "ww_common.h"

#include <string.h>

static bool ww_duck_is_digger(const WwDuckShoot *duck, uint8_t vehicle)
{
    return vehicle == duck->ordinary_duck_vehicle ||
           vehicle == duck->special_duck_vehicle;
}

bool ww_duck_open(WwDuckShoot *duck, const WwArchive *archive,
                  unsigned player_vehicle)
{
    WwDuckShoot loaded;
    WwArchiveView view;
    unsigned vehicle;
    if (duck == NULL || archive == NULL || player_vehicle >= 8u ||
        !ww_archive_view(archive, "DIGGER.SP", &view) ||
        view.size < WW_DUCK_SPRITE_BYTES) {
        return false;
    }
    memset(&loaded, 0, sizeof(loaded));
    loaded.digger_sprites = view.data;
    /* sub_327E0 requests exactly 0x2140 bytes from the larger entry and
     * copies that same eight-frame bank into both selected vehicle slots. */
    loaded.digger_sprites_size = WW_DUCK_SPRITE_BYTES;
    loaded.ordinary_duck_vehicle = 0xffu;
    loaded.special_duck_vehicle = 0xffu;
    /* sub_327E0 selects the first two vehicle banks which are neither the
     * player's bank nor each other, then replaces both eight-way CARS.SP
     * sets with the eight DIGGER.SP frames. */
    for (vehicle = 0u; vehicle < 8u; ++vehicle) {
        if (vehicle == player_vehicle) continue;
        if (loaded.ordinary_duck_vehicle == 0xffu) {
            loaded.ordinary_duck_vehicle = (uint8_t)vehicle;
        } else {
            loaded.special_duck_vehicle = (uint8_t)vehicle;
            break;
        }
    }
    loaded.active = true;
    *duck = loaded;
    return true;
}

static bool ww_duck_recycle_target(
    WwDuckShoot *duck, unsigned racer_index,
    WwRacerState racers[WW_RACER_COUNT],
    WwAiRacerPathMotion paths[WW_RACER_COUNT], const WwTrack *track)
{
    WwRacerState *target = &racers[racer_index];
    unsigned advance;
    unsigned i;
    /* The native sub_16F90 allocates 24 path records and sub_28CAC advances
     * past a four-tick retired record.  The port has eight live racer slots,
     * so advance the recycled slot into the corresponding later path window
     * before making it hittable again. */
    ++duck->target_generation[racer_index];
    advance = 24u + ((unsigned)duck->target_generation[racer_index] * 31u +
                     racer_index * 13u) % 96u;
    for (i = 0u; i < advance; ++i) {
        if (!ww_ai_racer_path_step(&paths[racer_index], track,
                                   &target->world_x, &target->world_y,
                                   &target->heading)) {
            return false;
        }
    }
    target->active = true;
    target->hit_effect = 0u;
    target->hit_age = 0u;
    target->collision_state = 0u;
    return true;
}

bool ww_duck_target_hit_step(WwDuckShoot *duck, unsigned racer_index,
                             WwRacerState racers[WW_RACER_COUNT],
                             WwAiRacerPathMotion paths[WW_RACER_COUNT],
                             const WwTrack *track)
{
    WwRacerState *target;
    unsigned points;
    if (duck == NULL || !duck->active || racers == NULL || paths == NULL ||
        track == NULL || racer_index == 0u ||
        racer_index >= WW_RACER_COUNT) {
        return false;
    }
    target = &racers[racer_index];
    if (target->hit_effect == 0u) return true;
    ++target->hit_age;
    if (target->hit_age < WW_DUCK_HIT_TICKS) return true;

    /* sub_289EC: the second substituted DIGGER bank is worth two, every
     * other target one, and dword_7EDB4 saturates at 0x63. */
    points = target->vehicle == duck->special_duck_vehicle ? 2u : 1u;
    if ((unsigned)duck->score + points > WW_DUCK_SCORE_MAXIMUM) {
        duck->score = WW_DUCK_SCORE_MAXIMUM;
    } else {
        duck->score = (uint8_t)(duck->score + points);
    }
    duck->score_sound_pending = true;
    return ww_duck_recycle_target(duck, racer_index, racers, paths, track);
}

void ww_duck_update_timer(WwDuckShoot *duck, uint32_t elapsed_tenths)
{
    if (duck != NULL && duck->active &&
        elapsed_tenths >= WW_DUCK_DURATION_TENTHS) {
        duck->finished = true;
    }
}

uint32_t ww_duck_time_remaining(const WwDuckShoot *duck,
                                uint32_t elapsed_tenths)
{
    if (duck == NULL || !duck->active ||
        elapsed_tenths >= WW_DUCK_DURATION_TENTHS) {
        return 0u;
    }
    return WW_DUCK_DURATION_TENTHS - elapsed_tenths;
}

bool ww_duck_enqueue_targets(
    const WwDuckShoot *duck, WwRenderQueue *queue,
    const WwRenderer *renderer, const WwSpriteScaleSet *scale_set,
    const WwRacerState racers[WW_RACER_COUNT],
    uint16_t camera_x, uint16_t camera_y, uint16_t camera_heading,
    const uint8_t *car_sprites, size_t car_sprites_size)
{
    unsigned i;
    if (duck == NULL || !duck->active || queue == NULL || renderer == NULL ||
        scale_set == NULL || racers == NULL || car_sprites == NULL ||
        car_sprites_size != WW_CAR_BYTES || duck->digger_sprites == NULL ||
        duck->digger_sprites_size != WW_DUCK_SPRITE_BYTES) {
        return false;
    }
    for (i = 1u; i < WW_RACER_COUNT; ++i) {
        WwProjectedRacer projected;
        WwRenderQueueItem item;
        const WwSpriteScale *scale;
        const uint8_t *source;
        size_t source_size;
        size_t frame_offset;
        uint8_t frame;
        if (!ww_racer_project(renderer, scale_set, &racers[i], i,
                              camera_x, camera_y, camera_heading,
                              &projected)) {
            continue;
        }
        frame = racers[i].hit_effect != 0u
                    ? (uint8_t)(racers[i].hit_direction_frame & 7u)
                    : projected.direction_frame;
        if (ww_duck_is_digger(duck, racers[i].vehicle)) {
            source = duck->digger_sprites;
            source_size = duck->digger_sprites_size;
            frame_offset = (size_t)frame * WW_CAR_SOURCE_BYTES;
        } else {
            source = car_sprites;
            source_size = car_sprites_size;
            frame_offset = ((size_t)racers[i].vehicle * WW_CAR_FRAMES +
                            frame) * WW_CAR_SOURCE_BYTES;
        }
        if (frame_offset + WW_CAR_SOURCE_BYTES > source_size) return false;
        scale = &scale_set->level[projected.scale_level];
        memset(&item, 0, sizeof(item));
        item.x = projected.x;
        item.y = projected.y;
        item.distance = projected.distance;
        item.source = source + frame_offset;
        item.source_size = source_size - frame_offset;
        item.scale = scale;
        if (!ww_render_queue_push(queue, &item)) return false;
    }
    return true;
}
