#ifndef WW_GAME_H
#define WW_GAME_H

#include "ww_archive.h"
#include "ww_audio.h"
#include "ww_display.h"
#include "ww_input.h"
#include "ww_intro.h"
#include "ww_menu.h"
#include "ww_original_config.h"
#include "ww_persistence.h"
#include "ww_profiles.h"
#include "ww_race.h"
#include "ww_renderer.h"
#include "ww_timing.h"
#include "ww_victory.h"

#include <stdbool.h>

typedef struct WwGameOptions {
    const char *data_path;
    const char *asset_name;
    bool self_test;
    unsigned smoke_frames;
    unsigned race_smoke_track;
    unsigned duck_smoke_track;
} WwGameOptions;

typedef struct WwGame {
    WwArchive archive;
    WwDisplay display;
    WwInput input;
    WwTiming timing;
    WwAudio audio;
    WwSound engine_sound;
    WwSound crash_sound;
    WwSound start_sound;
    WwSound splash_sound;
    WwSound plip_sound;
    WwSound horn_sound;
    WwSound bell_sound;
    int engine_voice;
    int shallow_water_voice;
    int water_horn_voice;
    WwSettings settings;
    WwOriginalConfig original_config;
    WwProfiles profiles;
    WwRenderer renderer;
    WwIntro intro;
    WwMenu menu;
    WwRace race;
    WwVictory victory;
    bool sdl_initialized;
    bool menu_initialized;
    bool intro_initialized;
    bool intro_music_handoff;
    bool race_sounds_loaded;
    bool race_paused;
    unsigned race_pause_selection;
    bool running;
    unsigned smoke_frames;
} WwGame;

bool ww_game_self_test(const char *data_path);
bool ww_game_open(WwGame *game, const WwGameOptions *options);
int ww_game_run(WwGame *game);
void ww_game_close(WwGame *game);

#endif
