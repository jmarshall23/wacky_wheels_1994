#ifndef WW_DUCK_H
#define WW_DUCK_H

#include "ww_ai.h"
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
    WW_DUCK_TRACK_FIRST = 6,
    WW_DUCK_TRACK_LAST = 11,
    WW_DUCK_DURATION_TENTHS = 0x4b0,
    WW_DUCK_SPRITE_BYTES = 0x2140,
    WW_DUCK_TARGET_FRAMES = 8,
    WW_DUCK_HIT_TICKS = 4,
    WW_DUCK_SCORE_MAXIMUM = 0x63
};

typedef struct WwDuckShoot {
    const uint8_t *digger_sprites;
    size_t digger_sprites_size;
    uint8_t ordinary_duck_vehicle;
    uint8_t special_duck_vehicle;
    uint8_t target_generation[WW_RACER_COUNT];
    uint8_t score;
    bool active;
    bool finished;
    bool score_sound_pending;
} WwDuckShoot;

bool ww_duck_open(WwDuckShoot *duck, const WwArchive *archive,
                  unsigned player_vehicle);
bool ww_duck_target_hit_step(WwDuckShoot *duck, unsigned racer_index,
                             WwRacerState racers[WW_RACER_COUNT],
                             WwAiRacerPathMotion paths[WW_RACER_COUNT],
                             const WwTrack *track);
void ww_duck_update_timer(WwDuckShoot *duck, uint32_t elapsed_tenths);
uint32_t ww_duck_time_remaining(const WwDuckShoot *duck,
                                uint32_t elapsed_tenths);
bool ww_duck_enqueue_targets(
    const WwDuckShoot *duck, WwRenderQueue *queue,
    const WwRenderer *renderer, const WwSpriteScaleSet *scale_set,
    const WwRacerState racers[WW_RACER_COUNT],
    uint16_t camera_x, uint16_t camera_y, uint16_t camera_heading,
    const uint8_t *car_sprites, size_t car_sprites_size);

#endif
