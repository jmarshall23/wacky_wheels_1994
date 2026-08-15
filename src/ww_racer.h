#ifndef WW_RACER_H
#define WW_RACER_H

#include "ww_display.h"
#include "ww_renderer.h"
#include "ww_render_queue.h"
#include "ww_sprite.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_RACER_COUNT = 8,
    WW_RACER_PLAYER_INDEX = 0,
    WW_RACER_PLAYER_DISTANCE = 0x7c
};

typedef enum WwRacerCrashState {
    WW_RACER_CRASH_NONE = 0,
    WW_RACER_CRASH_RISING = 1,
    WW_RACER_CRASH_HOLD = 2,
    WW_RACER_CRASH_RECOVERING = 3,
    WW_RACER_CRASH_COMPLETE = 4
} WwRacerCrashState;

enum {
    WW_RACER_CRASH_HOLD_TICKS = 0x0b,
    WW_RACER_CRASH_RESTART_TICKS = 0x0d
};

typedef struct WwRacerState {
    uint16_t world_x;
    uint16_t world_y;
    uint16_t heading;
    uint8_t vehicle;
    bool active;
    uint16_t spark_position;
    uint16_t spark_frame;
    uint16_t hit_effect;
    uint16_t hit_direction_frame;
    uint16_t hit_age;
    uint16_t jump_state;
    uint16_t jump_height;
    uint16_t jump_countdown;
    uint8_t twirl_frame;
    bool twirl_active;
    uint16_t collision_state;
    uint16_t crash_state;
    uint16_t crash_frame;
    uint16_t finish_frame;
    int16_t crash_y;
    uint32_t crash_phase_tick;
} WwRacerState;

typedef struct WwProjectedRacer {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t distance;
    uint8_t scale_level;
    uint8_t direction_frame;
    uint8_t racer_index;
    bool player;
} WwProjectedRacer;

void ww_racer_initialize_grid(WwRacerState racers[WW_RACER_COUNT],
                              const uint16_t grid_x[WW_RACER_COUNT],
                              const uint16_t grid_y[WW_RACER_COUNT],
                              uint16_t heading, unsigned player_vehicle);
bool ww_racer_project(const WwRenderer *renderer,
                      const WwSpriteScaleSet *scale_set,
                      const WwRacerState *racer, unsigned racer_index,
                      uint16_t camera_x, uint16_t camera_y,
                      uint16_t camera_heading,
                      WwProjectedRacer *projected);
uint8_t ww_racer_relative_direction_frame(uint16_t racer_heading,
                                          uint16_t camera_heading);
bool ww_racer_hit_effect_step(WwRacerState *racer);
void ww_racer_player_jump_render_step(WwRacerState *player);
void ww_racer_player_jump_render_finish(WwRacerState *player);
void ww_racer_player_begin_crash(WwRacerState *player, int16_t screen_y);
void ww_racer_player_finish_animation_step(WwRacerState *player,
                                           uint16_t finish_place);
bool ww_racer_enqueue_opponents(
    WwRenderQueue *queue, const WwRenderer *renderer,
    const WwSpriteScaleSet *scale_set,
    const WwRacerState racers[WW_RACER_COUNT],
    uint16_t camera_x, uint16_t camera_y, uint16_t camera_heading,
    const uint8_t *car_sprites, size_t car_sprites_size);
bool ww_racer_enqueue_player(
    WwRenderQueue *queue, const WwRenderer *renderer,
    const WwSpriteScaleSet *scale_set, WwRacerState *player,
    const uint8_t *car_sprites, size_t car_sprites_size,
    const uint8_t *player_vehicle_sprites,
    size_t player_vehicle_sprites_size, uint8_t player_steering_frame,
    uint16_t finish_place, uint32_t elapsed_136_ticks);
bool ww_racer_draw_player_spark(WwDisplay *display,
                                const WwRenderer *renderer,
                                const WwSpriteScaleSet *scale_set,
                                WwRacerState *player,
                                const uint8_t *spark_sprites,
                                size_t spark_sprites_size);

#endif
