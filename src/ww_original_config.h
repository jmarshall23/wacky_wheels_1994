#ifndef WW_ORIGINAL_CONFIG_H
#define WW_ORIGINAL_CONFIG_H

#include "ww_archive.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WW_ORIGINAL_SETTINGS_BYTES = 0x492,
    WW_ORIGINAL_CONTROLS_BYTES = 0x18,
    WW_ORIGINAL_SAVE_BYTES = 0x630,
    WW_ORIGINAL_CONFIG_BYTES = 0xada,
    WW_ORIGINAL_CONFIG_SENTINEL_OFFSET = 0x308,
    WW_ORIGINAL_CONFIG_SENTINEL = 0xfa
};

typedef struct WwOriginalConfig {
    uint8_t settings[WW_ORIGINAL_SETTINGS_BYTES];
    uint8_t controls[WW_ORIGINAL_CONTROLS_BYTES];
    uint8_t save_data[WW_ORIGINAL_SAVE_BYTES];
    bool loaded_cfg;
} WwOriginalConfig;

bool ww_original_config_defaults(WwOriginalConfig *config,
                                 const WwArchive *archive);
bool ww_original_config_overlay(WwOriginalConfig *config, const char *path);

#endif
