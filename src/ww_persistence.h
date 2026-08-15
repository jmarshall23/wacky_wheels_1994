#ifndef WW_PERSISTENCE_H
#define WW_PERSISTENCE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct WwSettings {
    bool fullscreen;
    bool aspect_correct;
    bool overhead_map;
    bool clock_display;
    bool speedometer;
    uint8_t sound_volume;
    uint8_t music_volume;
    uint8_t engine_volume;
    uint8_t single_screen_detail;
    uint8_t split_screen_detail;
    char data_path[1024];
    char writable_path[1024];
} WwSettings;

void ww_settings_defaults(WwSettings *settings);
bool ww_settings_load(WwSettings *settings);
bool ww_settings_save(const WwSettings *settings);

#endif
