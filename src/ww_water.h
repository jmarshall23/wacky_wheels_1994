#ifndef WW_WATER_H
#define WW_WATER_H

#include "ww_archive.h"
#include "ww_display.h"
#include "ww_render_queue.h"
#include "ww_renderer.h"
#include "ww_sprite.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_WATER_SHALLOW_CLASS = 5,
    WW_WATER_DEEP_CLASS_FIRST = 6,
    WW_WATER_DEEP_CLASS_SECOND = 0x0f,
    WW_WATER_SDX_RECORD_BYTES = 0x12e
};

typedef struct WwWaterEffectDescriptor {
    uint32_t splash_offset;
    uint16_t splash_stride;
    uint16_t splash_width;
    uint16_t splash_height;
    uint16_t splash_frames;
    uint32_t periscope_offset;
    uint16_t periscope_stride;
    uint16_t periscope_width;
    uint16_t periscope_height;
    uint16_t periscope_frames;
} WwWaterEffectDescriptor;

typedef struct WwWaterAssets {
    const uint8_t *effects;
    size_t effects_size;
    WwWaterEffectDescriptor shallow;
    WwWaterEffectDescriptor deep[2];
    bool loaded;
} WwWaterAssets;

typedef struct WwWaterState {
    uint8_t surface_class;
    uint8_t shallow_frame;
    uint8_t splash_frame;
    uint8_t periscope_frame;
    bool shallow_active;
    bool submerged;
    bool splash_active;
    bool splash_sound_pending;
} WwWaterState;

bool ww_water_assets_load(WwWaterAssets *assets, const WwArchive *archive);
void ww_water_state_reset(WwWaterState *state);
void ww_water_update(WwWaterState *state, uint32_t surface_class,
                     bool allow_entry);
bool ww_water_enqueue_periscope(const WwWaterAssets *assets,
                                const WwWaterState *state,
                                const WwRenderer *renderer,
                                const WwSpriteScaleSet *scale_set,
                                WwRenderQueue *queue);
bool ww_water_draw_splash(const WwWaterAssets *assets,
                          const WwWaterState *state, WwDisplay *display);
bool ww_water_draw_shallow_spray(const WwWaterAssets *assets,
                                 const WwWaterState *state,
                                 const WwRenderer *renderer,
                                 const WwSpriteScaleSet *scale_set,
                                 WwDisplay *display);
void ww_water_render_step(WwWaterState *state,
                          const WwWaterAssets *assets);

#endif
