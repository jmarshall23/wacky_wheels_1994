#include "ww_persistence.h"

#include "ww_common.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool ww_settings_path(char path[1024])
{
    char *preference_path = SDL_GetPrefPath("Wacky Wheels", "WW95");
    int written;
    if (preference_path == NULL) {
        return false;
    }
    written = snprintf(path, 1024, "%sww95.cfg", preference_path);
    SDL_free(preference_path);
    return written > 0 && written < 1024;
}

void ww_settings_defaults(WwSettings *settings)
{
    memset(settings, 0, sizeof(*settings));
    settings->aspect_correct = true;
    settings->overhead_map = true;
    settings->clock_display = true;
    settings->speedometer = true;
    settings->sound_volume = 255;
    settings->music_volume = 192;
    settings->engine_volume = 255;
    settings->single_screen_detail = 1;
    settings->split_screen_detail = 1;
}

bool ww_settings_load(WwSettings *settings)
{
    char path[1024];
    char line[1200];
    FILE *file;
    ww_settings_defaults(settings);
    if (!ww_settings_path(path)) {
        return false;
    }
    file = fopen(path, "rt");
    if (file == NULL) {
        return false;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char *equals = strchr(line, '=');
        char *value;
        if (equals == NULL) continue;
        *equals = '\0';
        value = equals + 1;
        value[strcspn(value, "\r\n")] = '\0';
        if (ww_ascii_iequals(line, "fullscreen")) settings->fullscreen = atoi(value) != 0;
        else if (ww_ascii_iequals(line, "aspect_correct")) settings->aspect_correct = atoi(value) != 0;
        else if (ww_ascii_iequals(line, "overhead_map")) settings->overhead_map = atoi(value) != 0;
        else if (ww_ascii_iequals(line, "clock_display")) settings->clock_display = atoi(value) != 0;
        else if (ww_ascii_iequals(line, "speedometer")) settings->speedometer = atoi(value) != 0;
        else if (ww_ascii_iequals(line, "sound_volume")) settings->sound_volume = (uint8_t)atoi(value);
        else if (ww_ascii_iequals(line, "music_volume")) settings->music_volume = (uint8_t)atoi(value);
        else if (ww_ascii_iequals(line, "engine_volume")) settings->engine_volume = (uint8_t)atoi(value);
        else if (ww_ascii_iequals(line, "single_screen_detail")) {
            int parsed = atoi(value);
            if (parsed >= 1 && parsed <= 3) settings->single_screen_detail = (uint8_t)parsed;
        } else if (ww_ascii_iequals(line, "split_screen_detail")) {
            int parsed = atoi(value);
            if (parsed >= 1 && parsed <= 3) settings->split_screen_detail = (uint8_t)parsed;
        }
        else if (ww_ascii_iequals(line, "data_path")) {
            strncpy(settings->data_path, value, sizeof(settings->data_path) - 1);
        }
    }
    fclose(file);
    strncpy(settings->writable_path, path, sizeof(settings->writable_path) - 1);
    return true;
}

bool ww_settings_save(const WwSettings *settings)
{
    char path[1024];
    FILE *file;
    if (!ww_settings_path(path)) {
        return false;
    }
    file = fopen(path, "wt");
    if (file == NULL) {
        return false;
    }
    fprintf(file, "fullscreen=%u\n", settings->fullscreen ? 1u : 0u);
    fprintf(file, "aspect_correct=%u\n", settings->aspect_correct ? 1u : 0u);
    fprintf(file, "overhead_map=%u\n", settings->overhead_map ? 1u : 0u);
    fprintf(file, "clock_display=%u\n", settings->clock_display ? 1u : 0u);
    fprintf(file, "speedometer=%u\n", settings->speedometer ? 1u : 0u);
    fprintf(file, "sound_volume=%u\n", (unsigned)settings->sound_volume);
    fprintf(file, "music_volume=%u\n", (unsigned)settings->music_volume);
    fprintf(file, "engine_volume=%u\n", (unsigned)settings->engine_volume);
    fprintf(file, "single_screen_detail=%u\n",
            (unsigned)settings->single_screen_detail);
    fprintf(file, "split_screen_detail=%u\n",
            (unsigned)settings->split_screen_detail);
    fprintf(file, "data_path=%s\n", settings->data_path);
    if (fclose(file) != 0) {
        return false;
    }
    return true;
}
