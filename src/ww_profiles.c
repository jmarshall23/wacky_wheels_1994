#include "ww_profiles.h"

#include "ww_common.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ww_profile_default(WwProfile *profile, const char *name, uint32_t value)
{
    size_t length = strlen(name);
    memset(profile, 0, sizeof(*profile));
    memset(profile->name, ' ', 16);
    if (length > 16) length = 16;
    memcpy(profile->name, name, length);
    profile->value = value;
}

/* Defaults constructed by sub_35534 when WACKY.DTT is absent. */
void ww_profiles_defaults(WwProfiles *profiles)
{
    memset(profiles, 0, sizeof(*profiles));
    ww_profile_default(&profiles->profile[0], "SHAUN", 16);
    ww_profile_default(&profiles->profile[1], "ANDY", 12);
    ww_profile_default(&profiles->profile[2], "NUGGET", 8);
    ww_profile_default(&profiles->profile[3], "RUEBEN", 6);
}

bool ww_profiles_load(WwProfiles *profiles, const char *path)
{
    uint8_t bytes[WW_PROFILE_FILE_BYTES];
    FILE *file;
    unsigned i;
    bool valid;
    if (profiles == NULL || path == NULL) {
        return false;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    valid = fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes) &&
            fgetc(file) == EOF;
    if (fclose(file) != 0) valid = false;
    if (!valid) return false;

    memset(profiles, 0, sizeof(*profiles));
    for (i = 0; i < WW_PROFILE_COUNT; ++i) {
        const uint8_t *record = bytes + i * WW_PROFILE_RECORD_BYTES;
        memcpy(profiles->profile[i].name, record, WW_PROFILE_NAME_BYTES);
        profiles->profile[i].name[WW_PROFILE_NAME_BYTES - 1] = '\0';
        profiles->profile[i].value = ww_read_le32(record + WW_PROFILE_NAME_BYTES);
    }
    profiles->loaded_file = true;
    return true;
}

static bool ww_profiles_user_path(char path[1024])
{
    char *preference_path = SDL_GetPrefPath("Wacky Wheels", "WW95");
    int written;
    if (preference_path == NULL) return false;
    written = snprintf(path, 1024, "%sWACKY.DTT", preference_path);
    SDL_free(preference_path);
    return written > 0 && written < 1024;
}

bool ww_profiles_load_user(WwProfiles *profiles)
{
    char path[1024];
    return profiles != NULL && ww_profiles_user_path(path) &&
           ww_profiles_load(profiles, path);
}

bool ww_profiles_record_score(WwProfiles *profiles, const char *name,
                              uint32_t score)
{
    unsigned position;
    size_t length;
    if (profiles == NULL || name == NULL) return false;
    for (position = 0u; position < WW_PROFILE_COUNT; ++position) {
        if (score >= profiles->profile[position].value) break;
    }
    if (position == WW_PROFILE_COUNT) return false;
    if (position + 1u < WW_PROFILE_COUNT) {
        memmove(&profiles->profile[position + 1u],
                &profiles->profile[position],
                (WW_PROFILE_COUNT - position - 1u) *
                    sizeof(profiles->profile[0]));
    }
    memset(&profiles->profile[position], 0, sizeof(profiles->profile[0]));
    memset(profiles->profile[position].name, ' ', 16u);
    length = strlen(name);
    if (length > 16u) length = 16u;
    memcpy(profiles->profile[position].name, name, length);
    profiles->profile[position].value = score;
    return true;
}

bool ww_profiles_save_user(const WwProfiles *profiles)
{
    char path[1024];
    uint8_t bytes[WW_PROFILE_FILE_BYTES];
    FILE *file;
    unsigned i;
    if (profiles == NULL || !ww_profiles_user_path(path)) return false;
    memset(bytes, 0, sizeof(bytes));
    for (i = 0u; i < WW_PROFILE_COUNT; ++i) {
        uint8_t *record = bytes + i * WW_PROFILE_RECORD_BYTES;
        uint32_t value = profiles->profile[i].value;
        memcpy(record, profiles->profile[i].name, WW_PROFILE_NAME_BYTES);
        record[WW_PROFILE_NAME_BYTES + 0u] = (uint8_t)value;
        record[WW_PROFILE_NAME_BYTES + 1u] = (uint8_t)(value >> 8);
        record[WW_PROFILE_NAME_BYTES + 2u] = (uint8_t)(value >> 16);
        record[WW_PROFILE_NAME_BYTES + 3u] = (uint8_t)(value >> 24);
    }
    file = fopen(path, "wb");
    if (file == NULL) return false;
    if (fwrite(bytes, 1u, sizeof(bytes), file) != sizeof(bytes)) {
        fclose(file);
        return false;
    }
    return fclose(file) == 0;
}
