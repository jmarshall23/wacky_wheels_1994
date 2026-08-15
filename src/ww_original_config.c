#include "ww_original_config.h"

#include "ww_common.h"

#include <stdio.h>
#include <string.h>

bool ww_original_config_defaults(WwOriginalConfig *config,
                                 const WwArchive *archive)
{
    WwArchiveView view;
    if (config == NULL || !ww_archive_view(archive, "WACKY.ING", &view) ||
        view.size != WW_ORIGINAL_SETTINGS_BYTES) {
        return false;
    }
    memset(config, 0, sizeof(*config));
    memcpy(config->settings, view.data, WW_ORIGINAL_SETTINGS_BYTES);
    return true;
}

/* The three reads and 0x00fa validity word match sub_35CA4 exactly. */
bool ww_original_config_overlay(WwOriginalConfig *config, const char *path)
{
    uint8_t bytes[WW_ORIGINAL_CONFIG_BYTES];
    FILE *file;
    bool valid;
    if (config == NULL || path == NULL) {
        return false;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    valid = fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes) &&
            fgetc(file) == EOF;
    if (fclose(file) != 0) {
        valid = false;
    }
    if (!valid || ww_read_le16(bytes + WW_ORIGINAL_SETTINGS_BYTES +
                               WW_ORIGINAL_CONTROLS_BYTES +
                               WW_ORIGINAL_CONFIG_SENTINEL_OFFSET) !=
                      WW_ORIGINAL_CONFIG_SENTINEL) {
        return false;
    }
    memcpy(config->settings, bytes, WW_ORIGINAL_SETTINGS_BYTES);
    memcpy(config->controls, bytes + WW_ORIGINAL_SETTINGS_BYTES,
           WW_ORIGINAL_CONTROLS_BYTES);
    memcpy(config->save_data,
           bytes + WW_ORIGINAL_SETTINGS_BYTES + WW_ORIGINAL_CONTROLS_BYTES,
           WW_ORIGINAL_SAVE_BYTES);
    config->loaded_cfg = true;
    return true;
}
