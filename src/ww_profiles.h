#ifndef WW_PROFILES_H
#define WW_PROFILES_H

#include <stdbool.h>
#include <stdint.h>

enum {
    WW_PROFILE_COUNT = 4,
    WW_PROFILE_NAME_BYTES = 20,
    WW_PROFILE_RECORD_BYTES = 24,
    WW_PROFILE_FILE_BYTES = 96
};

typedef struct WwProfile {
    char name[WW_PROFILE_NAME_BYTES];
    uint32_t value;
} WwProfile;

typedef struct WwProfiles {
    WwProfile profile[WW_PROFILE_COUNT];
    bool loaded_file;
} WwProfiles;

void ww_profiles_defaults(WwProfiles *profiles);
bool ww_profiles_load(WwProfiles *profiles, const char *path);
bool ww_profiles_load_user(WwProfiles *profiles);
bool ww_profiles_record_score(WwProfiles *profiles, const char *name,
                              uint32_t score);
bool ww_profiles_save_user(const WwProfiles *profiles);

#endif
