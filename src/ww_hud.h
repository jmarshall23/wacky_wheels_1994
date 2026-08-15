#ifndef WW_HUD_H
#define WW_HUD_H

#include "ww_archive.h"
#include "ww_display.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_HUD_LAP_ASSET_BYTES = 0x4c56,
    WW_HUD_OUTPUT_FONT_BYTES = 0x0f40,
    WW_HUD_GENERAL_EFFECT_BYTES = 0x3964,
    WW_HUD_START_LIGHT_OFFSET = 0x1161,
    WW_HUD_START_LIGHT_FRAMES = 3,
    WW_HUD_TIMER_GLYPH_OFFSET = 0x35a5,
    WW_HUD_TIMER_GLYPH_BYTES = 0x3f,
    WW_HUD_ICON_ASSET_BYTES = 0x15c6,
    WW_HUD_SPEEDOMETER_OFFSET = 0x0ccc,
    WW_HUD_SPEED_DIGIT_OFFSET = 0x0fea,
    WW_HUD_SPEED_DIGIT_BYTES = 0x96,
    WW_HUD_AMMO_DIGIT_OFFSET = 0x46ec,
    WW_HUD_AMMO_DIGIT_BYTES = 0x5a,
    WW_HUD_GIGGLES_BYTES = 0xb6e0,
    WW_HUD_GIG_MOV_BYTES = 326,
    WW_HUD_PUFF_BYTES = 0x10a0
};

typedef enum WwHudLapAlertPhase {
    WW_HUD_LAP_ALERT_DEVIL = 0,
    WW_HUD_LAP_ALERT_PUFF = 1
} WwHudLapAlertPhase;

typedef struct WwHudLapAlert {
    uint16_t motion_frame;
    uint8_t sprite_frame;
    WwHudLapAlertPhase phase;
    bool active;
} WwHudLapAlert;

typedef struct WwHudAssets {
    const uint8_t *lap_sprites;
    size_t lap_sprites_size;
    const uint8_t *output_font;
    size_t output_font_size;
    const uint8_t *general_effects;
    size_t general_effects_size;
    const uint8_t *icons;
    size_t icons_size;
    const uint8_t *giggles;
    size_t giggles_size;
    const uint8_t *gig_movement;
    size_t gig_movement_size;
    const uint8_t *puff;
    size_t puff_size;
    bool loaded;
} WwHudAssets;

bool ww_hud_assets_load(WwHudAssets *assets, const WwArchive *archive);
bool ww_hud_draw_race_frame(const WwHudAssets *assets, WwDisplay *display);
bool ww_hud_draw_rank(const WwHudAssets *assets, WwDisplay *display,
                      unsigned rank);
bool ww_hud_draw_wrong_way(const WwHudAssets *assets, WwDisplay *display,
                           bool active, bool *blink_frame);
bool ww_hud_format_race_time(uint32_t tenths, char text[8]);
bool ww_hud_draw_race_time(const WwHudAssets *assets, WwDisplay *display,
                           uint32_t tenths, bool visible);
bool ww_hud_draw_speedometer(const WwHudAssets *assets, WwDisplay *display,
                             int16_t velocity, int16_t *display_speed,
                             bool visible);
bool ww_hud_draw_ammunition(const WwHudAssets *assets, WwDisplay *display,
                            uint16_t ammunition);
bool ww_hud_draw_duck_score(const WwHudAssets *assets, WwDisplay *display,
                            unsigned score);
bool ww_hud_draw_lives(const WwHudAssets *assets, WwDisplay *display,
                       uint16_t lives);
bool ww_hud_draw_lap_status(const WwHudAssets *assets, WwDisplay *display,
                            unsigned current_lap, unsigned lap_count,
                            bool show_status, bool finished);
bool ww_hud_draw_finish_place(const WwHudAssets *assets, WwDisplay *display,
                              const uint8_t *car_sprites,
                              size_t car_sprites_size,
                              unsigned vehicle, unsigned place);
void ww_hud_lap_alert_reset(WwHudLapAlert *alert);
void ww_hud_lap_alert_begin(WwHudLapAlert *alert);
bool ww_hud_draw_lap_alert(const WwHudAssets *assets, WwDisplay *display,
                           const WwHudLapAlert *alert);
void ww_hud_lap_alert_step(WwHudLapAlert *alert,
                           const WwHudAssets *assets);

#endif
