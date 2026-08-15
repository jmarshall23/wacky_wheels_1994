#include "ww_racer.h"

#include "ww_common.h"
#include "ww_perspective.h"

#include <string.h>

enum {
    WW_RACER_PLAYER_FRAME = 4,
    WW_RACER_CRASH_FIRST_FRAME = 4,
    WW_RACER_CRASH_FRAME_COUNT = 8,
    WW_RACER_RECOVERY_FIRST_FRAME = 12,
    WW_RACER_CRASH_TOP_Y = 0x72,
    WW_RACER_CRASH_BOTTOM_Y = 0xc7,
    WW_RACER_SPARK_WIDTH = 10,
    WW_RACER_SPARK_HEIGHT = 9,
    WW_RACER_SPARK_FRAME_BYTES = 0x5a,
    WW_RACER_SPARK_FRAMES = 4
};

/* word_73C98...word_740C0, installed by sub_328D4. */
static const uint8_t ww_racer_recovery_frame_count[WW_CAR_VEHICLES] = {
    4, 4, 4, 4, 2, 1, 2, 1
};

/* word_73C9A...word_740C2, installed beside the recovery counts by
 * sub_328D4. */
static const uint8_t ww_racer_finish_frame_count[WW_CAR_VEHICLES] = {
    2, 4, 8, 8, 2, 7, 2, 4
};

static const uint8_t ww_racer_lower_finish_sequence[8] = {
    8, 9, 8, 4, 10, 11, 10, 4
};

static const uint8_t ww_racer_podium_turn_sequence[5] = {
    4, 3, 2, 1, 0
};

/* dword_7B448, built at loc_1771C, starts at CARS frame four and wraps
 * through all eight directions.  Racer animation state 14 consumes it once
 * for the airborne barrel/twirl. */
static const uint8_t ww_racer_twirl_sequence[8] = {
    4, 5, 6, 7, 0, 1, 2, 3
};

/* sub_2876C rounds the 0..0x77f heading to one of eight 0xf0-wide
 * octants.  sub_287EC then indexes the table built at loc_3657D. */
uint8_t ww_racer_relative_direction_frame(uint16_t racer_heading,
                                          uint16_t camera_heading)
{
    unsigned racer_octant = ((unsigned)racer_heading + 0x78u) / 0xf0u;
    unsigned camera_octant = ((unsigned)camera_heading + 0x78u) / 0xf0u;
    racer_octant &= 7u;
    camera_octant &= 7u;
    return (uint8_t)((racer_octant + 4u - camera_octant) & 7u);
}

void ww_racer_initialize_grid(WwRacerState racers[WW_RACER_COUNT],
                              const uint16_t grid_x[WW_RACER_COUNT],
                              const uint16_t grid_y[WW_RACER_COUNT],
                              uint16_t heading, unsigned player_vehicle)
{
    unsigned i;
    unsigned next_vehicle;
    if (racers == NULL || grid_x == NULL || grid_y == NULL ||
        player_vehicle >= WW_CAR_VEHICLES) {
        return;
    }
    memset(racers, 0, sizeof(*racers) * WW_RACER_COUNT);
    next_vehicle = (player_vehicle + 1u) % WW_CAR_VEHICLES;
    for (i = 0; i < WW_RACER_COUNT; ++i) {
        racers[i].world_x = grid_x[i];
        racers[i].world_y = grid_y[i];
        racers[i].heading = heading;
        racers[i].active = true;
        if (i == WW_RACER_PLAYER_INDEX) {
            racers[i].vehicle = (uint8_t)player_vehicle;
        } else {
            racers[i].vehicle = (uint8_t)next_vehicle;
            next_vehicle = (next_vehicle + 1u) % WW_CAR_VEHICLES;
        }
    }
}

/* Racer-specific wrapper around sub_255D4's common projection core. */
bool ww_racer_project(const WwRenderer *renderer,
                      const WwSpriteScaleSet *scale_set,
                      const WwRacerState *racer, unsigned racer_index,
                      uint16_t camera_x, uint16_t camera_y,
                      uint16_t camera_heading,
                      WwProjectedRacer *projected)
{
    WwPerspectiveProjection projection;
    const WwSpriteScale *scale;

    if (renderer == NULL || scale_set == NULL || racer == NULL ||
        projected == NULL || !racer->active ||
        racer_index >= WW_RACER_COUNT ||
        camera_heading >= WW_TRIG_ENTRY_COUNT ||
        racer->heading >= WW_TRIG_ENTRY_COUNT ||
        racer->vehicle >= WW_CAR_VEHICLES) {
        return false;
    }
    if (!ww_perspective_project(renderer, racer->world_x, racer->world_y,
                                camera_x, camera_y, camera_heading,
                                &projection)) {
        return false;
    }
    scale = &scale_set->level[projection.scale_level];
    if (scale->width == 0 || scale->height == 0) {
        return false;
    }

    memset(projected, 0, sizeof(*projected));
    projected->x =
        (int16_t)(projection.center_x - (int32_t)scale->width / 2);
    projected->y = (int16_t)(
        (int32_t)renderer->projection_right[projection.distance] -
        (int32_t)scale->height);
    projected->width = scale->width;
    projected->height = scale->height;
    projected->distance = projection.distance;
    projected->scale_level = projection.scale_level;
    projected->direction_frame =
        ww_racer_relative_direction_frame(racer->heading, camera_heading);
    projected->racer_index = (uint8_t)racer_index;
    projected->player = false;
    return true;
}

static bool ww_racer_push_projected(WwRenderQueue *queue,
                                    const WwProjectedRacer *projected,
                                    const WwSpriteScaleSet *scale_set,
                                    const WwRacerState *racer,
                                    const uint8_t *sprites,
                                    size_t sprites_size,
                                    size_t frame_offset)
{
    WwRenderQueueItem item;
    if (queue == NULL || projected == NULL || scale_set == NULL ||
        racer == NULL || sprites == NULL ||
        projected->scale_level >= WW_SPRITE_SCALE_LEVELS ||
        frame_offset + WW_CAR_SOURCE_BYTES > sprites_size) {
        return false;
    }
    memset(&item, 0, sizeof(item));
    item.x = projected->x;
    item.y = projected->y;
    item.distance = projected->distance;
    item.source = sprites + frame_offset;
    item.source_size = sprites_size - frame_offset;
    item.scale = &scale_set->level[projected->scale_level];
    return ww_render_queue_push(queue, &item);
}

bool ww_racer_enqueue_opponents(
    WwRenderQueue *queue, const WwRenderer *renderer,
    const WwSpriteScaleSet *scale_set,
    const WwRacerState racers[WW_RACER_COUNT],
    uint16_t camera_x, uint16_t camera_y, uint16_t camera_heading,
    const uint8_t *car_sprites, size_t car_sprites_size)
{
    unsigned i;

    if (queue == NULL || renderer == NULL || scale_set == NULL ||
        racers == NULL || car_sprites == NULL ||
        car_sprites_size != WW_CAR_BYTES ||
        !racers[WW_RACER_PLAYER_INDEX].active ||
        racers[WW_RACER_PLAYER_INDEX].vehicle >= WW_CAR_VEHICLES) {
        return false;
    }

    /* sub_255D4 skips the current racer. */
    for (i = 1; i < WW_RACER_COUNT; ++i) {
        WwProjectedRacer projected;
        if (ww_racer_project(renderer, scale_set, &racers[i], i,
                             camera_x, camera_y, camera_heading,
                             &projected)) {
            uint8_t direction_frame =
                racers[i].hit_effect == 1u
                    ? (uint8_t)(racers[i].hit_direction_frame & 7u)
                    : projected.direction_frame;
            size_t frame_offset =
                ((size_t)racers[i].vehicle * WW_CAR_FRAMES +
                 direction_frame) * WW_CAR_SOURCE_BYTES;
            if (!ww_racer_push_projected(queue, &projected, scale_set,
                                         &racers[i], car_sprites,
                                         car_sprites_size, frame_offset)) {
                return false;
            }
        }
    }
    return true;
}

/* sub_28CAC calls sub_289EC and skips the rest of that racer's AI update
 * while +58h is active.  State one lasts 0x21 frames; the alternate effect
 * states last 0x32.  The sound-controlled 2->3 transition does not alter the
 * duration or sprite path, so the native state advances on its first tick. */
bool ww_racer_hit_effect_step(WwRacerState *racer)
{
    uint16_t duration;
    if (racer == NULL || racer->hit_effect == 0u) return false;
    duration = racer->hit_effect == 1u ? 0x21u : 0x32u;
    if (racer->hit_effect == 2u) racer->hit_effect = 3u;
    ++racer->hit_age;
    if (racer->hit_age >= duration) {
        racer->hit_effect = 0u;
        racer->hit_age = 0u;
    }
    /* The assembly still skips movement on the frame that clears +58h. */
    return true;
}

/* sub_24BCC's +0Ah/+10h vertical jump path.  Physics changes state one to
 * state two halfway through the eight-update arc; rendering performs the
 * visible 8-pixel rise/fall and clears transient state three after drawing. */
void ww_racer_player_jump_render_step(WwRacerState *player)
{
    if (player == NULL || player->jump_state == 0u) return;
    if (player->jump_state == 1u) {
        player->jump_height = (uint16_t)(player->jump_height + 8u);
        if (player->jump_height > 0x28u) player->jump_height = 0x28u;
    } else {
        if (player->jump_height <= 8u) {
            player->jump_height = 0u;
            player->jump_state = 3u;
        } else {
            player->jump_height = (uint16_t)(player->jump_height - 8u);
        }
    }
}

void ww_racer_player_jump_render_finish(WwRacerState *player)
{
    if (player != NULL && player->jump_state == 3u) {
        player->jump_state = 0u;
    }
}

void ww_racer_player_begin_crash(WwRacerState *player, int16_t screen_y)
{
    if (player == NULL || player->crash_state != WW_RACER_CRASH_NONE) return;
    player->crash_state = WW_RACER_CRASH_RISING;
    player->crash_frame = 0u;
    player->crash_y = screen_y;
    player->crash_phase_tick = 0u;
    player->jump_state = 0u;
    player->jump_height = 0u;
    player->jump_countdown = 0u;
    player->twirl_frame = 0u;
    player->twirl_active = false;
    player->spark_position = 0u;
    player->spark_frame = 0u;
}

void ww_racer_player_finish_animation_step(WwRacerState *player,
                                           uint16_t finish_place)
{
    unsigned limit;
    if (player == NULL || finish_place == 0u ||
        player->vehicle >= WW_CAR_VEHICLES) {
        return;
    }
    ++player->finish_frame;
    if (finish_place <= 3u) {
        limit = 5u + ww_racer_finish_frame_count[player->vehicle];
        /* loc_29CB6 plays the five turn-in cells once, then loops only the
         * animal's finish frames beginning at cell five. */
        if (player->finish_frame >= limit) player->finish_frame = 5u;
    } else if (player->finish_frame >= 8u) {
        player->finish_frame = 0u;
    }
}

bool ww_racer_enqueue_player(
    WwRenderQueue *queue, const WwRenderer *renderer,
    const WwSpriteScaleSet *scale_set, WwRacerState *player,
    const uint8_t *car_sprites, size_t car_sprites_size,
    const uint8_t *player_vehicle_sprites,
    size_t player_vehicle_sprites_size, uint8_t player_steering_frame,
    uint16_t finish_place, uint32_t elapsed_136_ticks)
{
    WwProjectedRacer projected;
    const WwSpriteScale *player_scale;
    const uint8_t *source;
    size_t source_size;
    size_t frame_offset;
    if (queue == NULL || renderer == NULL || scale_set == NULL ||
        player == NULL || !player->active ||
        player->vehicle >= WW_CAR_VEHICLES || car_sprites == NULL ||
        car_sprites_size != WW_CAR_BYTES ||
        player_vehicle_sprites == NULL || player_steering_frame > 4u) {
        return false;
    }

    if (player->crash_state == WW_RACER_CRASH_COMPLETE) return true;

    /* Normal sub_26A3C path: the player's car is fixed at distance 0x7c and
     * participates in the same far-to-near queue sort as every other racer. */
    player_scale = &scale_set->level[0];
    memset(&projected, 0, sizeof(projected));
    projected.x = (int16_t)(0xa0 - (int)player_scale->width / 2);
    projected.y = (int16_t)(
        (int)renderer->projection_right[WW_RACER_PLAYER_DISTANCE] -
        (int)player_scale->height - (int)player->jump_height);
    projected.width = player_scale->width;
    projected.height = player_scale->height;
    projected.distance = WW_RACER_PLAYER_DISTANCE;
    projected.scale_level = 0;
    projected.direction_frame = player_steering_frame;
    projected.racer_index = WW_RACER_PLAYER_INDEX;
    projected.player = true;

    if (player->crash_state != WW_RACER_CRASH_NONE) {
        unsigned individual_frame;
        if (player->crash_state == WW_RACER_CRASH_RISING) {
            individual_frame = WW_RACER_CRASH_FIRST_FRAME +
                               player->crash_frame;
            ++player->crash_frame;
            if (player->crash_frame == WW_RACER_CRASH_FRAME_COUNT) {
                player->crash_frame = 0u;
            }
            player->crash_y = (int16_t)(player->crash_y - 6);
            if (player->crash_y <= WW_RACER_CRASH_TOP_Y) {
                player->crash_y = WW_RACER_CRASH_TOP_Y;
                player->crash_state = WW_RACER_CRASH_HOLD;
                player->crash_frame = 0u;
                player->crash_phase_tick = elapsed_136_ticks;
                individual_frame = WW_RACER_RECOVERY_FIRST_FRAME;
            }
        } else if (player->crash_state == WW_RACER_CRASH_HOLD) {
            individual_frame = WW_RACER_RECOVERY_FIRST_FRAME;
            if (elapsed_136_ticks - player->crash_phase_tick >=
                WW_RACER_CRASH_HOLD_TICKS) {
                player->crash_state = WW_RACER_CRASH_RECOVERING;
            }
        } else {
            uint16_t count = ww_racer_recovery_frame_count[player->vehicle];
            ++player->crash_frame;
            if (player->crash_frame == count) player->crash_frame = 0u;
            individual_frame = WW_RACER_RECOVERY_FIRST_FRAME +
                               player->crash_frame;
            player->crash_y = (int16_t)(player->crash_y + 4);
            if (player->crash_y > WW_RACER_CRASH_BOTTOM_Y) {
                player->crash_y = WW_RACER_CRASH_BOTTOM_Y;
                player->crash_state = WW_RACER_CRASH_COMPLETE;
                player->crash_phase_tick = elapsed_136_ticks;
            }
        }
        projected.y = player->crash_y;
        frame_offset = (size_t)individual_frame * WW_CAR_SOURCE_BYTES;
        source = player_vehicle_sprites;
        source_size = player_vehicle_sprites_size;
    } else if (finish_place != 0u) {
        if (finish_place <= 3u) {
            if (player->finish_frame < 5u) {
                frame_offset =
                    ((size_t)player->vehicle * WW_CAR_FRAMES +
                     ww_racer_podium_turn_sequence[player->finish_frame]) *
                    WW_CAR_SOURCE_BYTES;
                source = car_sprites;
                source_size = car_sprites_size;
            } else {
                unsigned individual_frame =
                    12u + ww_racer_recovery_frame_count[player->vehicle] +
                    (unsigned)player->finish_frame - 5u;
                frame_offset =
                    (size_t)individual_frame * WW_CAR_SOURCE_BYTES;
                source = player_vehicle_sprites;
                source_size = player_vehicle_sprites_size;
            }
        } else {
            frame_offset =
                ((size_t)player->vehicle * WW_CAR_FRAMES +
                 ww_racer_lower_finish_sequence[player->finish_frame & 7u]) *
                WW_CAR_SOURCE_BYTES;
            source = car_sprites;
            source_size = car_sprites_size;
        }
    } else if (player->twirl_active) {
        frame_offset =
            ((size_t)player->vehicle * WW_CAR_FRAMES +
             ww_racer_twirl_sequence[player->twirl_frame & 7u]) *
            WW_CAR_SOURCE_BYTES;
        source = car_sprites;
        source_size = car_sprites_size;
    } else if (player_steering_frame != 2u) {
        unsigned vehicle_frame = player_steering_frame < 2u
                                     ? player_steering_frame
                                     : player_steering_frame - 1u;
        frame_offset = (size_t)vehicle_frame * WW_CAR_SOURCE_BYTES;
        source = player_vehicle_sprites;
        source_size = player_vehicle_sprites_size;
    } else {
        frame_offset =
            ((size_t)player->vehicle * WW_CAR_FRAMES +
             WW_RACER_PLAYER_FRAME) * WW_CAR_SOURCE_BYTES;
        source = car_sprites;
        source_size = car_sprites_size;
    }
    return ww_racer_push_projected(queue, &projected, scale_set, player,
                                   source, source_size, frame_offset);
}

/* Racer offsets +46h/+48h/+4Ah in sub_24BCC and sub_253D8.  Position three
 * anchors the effect at the left edge of the fixed player-car image;
 * position four anchors it 0x1e pixels to the right. */
bool ww_racer_draw_player_spark(WwDisplay *display,
                                const WwRenderer *renderer,
                                const WwSpriteScaleSet *scale_set,
                                WwRacerState *player,
                                const uint8_t *spark_sprites,
                                size_t spark_sprites_size)
{
    const WwSpriteScale *player_scale;
    size_t frame_offset;
    int car_x;
    int car_y;
    int spark_x;
    if (display == NULL || renderer == NULL || scale_set == NULL ||
        player == NULL || spark_sprites == NULL ||
        spark_sprites_size !=
            WW_RACER_SPARK_FRAME_BYTES * WW_RACER_SPARK_FRAMES) {
        return false;
    }
    if (player->spark_position == 0u) return true;
    if (player->spark_frame >= WW_RACER_SPARK_FRAMES) return false;

    player_scale = &scale_set->level[0];
    car_x = 0xa0 - (int)player_scale->width / 2;
    car_y = (int)renderer->projection_right[WW_RACER_PLAYER_DISTANCE] -
            (int)player_scale->height - (int)player->jump_height;
    if (player->spark_position == 3u) {
        spark_x = car_x;
    } else if (player->spark_position == 4u) {
        spark_x = car_x + 0x1e;
    } else {
        spark_x = car_x + (int)player_scale->width / 2 - 1;
    }
    frame_offset = (size_t)player->spark_frame *
                   WW_RACER_SPARK_FRAME_BYTES;
    ww_display_blit_column_major(display, spark_x, car_y + 0x10,
                                 WW_RACER_SPARK_WIDTH,
                                 WW_RACER_SPARK_HEIGHT,
                                 spark_sprites + frame_offset,
                                 WW_RACER_SPARK_HEIGHT, 0);

    ++player->spark_frame;
    if (player->spark_frame == WW_RACER_SPARK_FRAMES) {
        player->spark_position = 0u;
        player->spark_frame = 0u;
    }
    return true;
}
