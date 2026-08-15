#ifndef WW_MENU_H
#define WW_MENU_H

#include "ww_archive.h"
#include "ww_audio.h"
#include "ww_display.h"
#include "ww_font.h"
#include "ww_input.h"
#include "ww_pcx.h"
#include "ww_persistence.h"
#include "ww_profiles.h"
#include "ww_race.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum WwMenuScreen {
    WW_MENU_MAIN,
    WW_MENU_OPTIONS,
    WW_MENU_GRAPHICS,
    WW_MENU_SINGLE_DETAIL,
    WW_MENU_SPLIT_DETAIL,
    WW_MENU_RACE_SCREEN,
    WW_MENU_VOLUME,
    WW_MENU_INFO,
    WW_MENU_SLIDES,
    WW_MENU_VEHICLE,
    WW_MENU_RACE_CLASS,
    WW_MENU_LAPS,
    WW_MENU_ENGINE,
    WW_MENU_RACE_PACK,
    WW_MENU_TRACK_BOARD,
    WW_MENU_DUCK_INSTRUCTIONS,
    WW_MENU_DUCK_TRACK,
    WW_MENU_DUCK_RESULTS
} WwMenuScreen;

typedef struct WwMenu {
    const WwArchive *archive;
    WwDisplay *display;
    WwAudio *audio;
    WwSettings *settings;
    WwPcxImage image;
    WwMenuFont font;
    const uint8_t *handle_sprites;
    const uint8_t *car_sprites;
    WwSound select_sound;
    WwSound navigation_sound;
    WwMenuScreen screen;
    WwMenuScreen slide_parent;
    unsigned selection;
    unsigned slide_index;
    unsigned slide_count;
    unsigned board_page;
    unsigned board_race;
    uint8_t selected_vehicle;
    uint8_t race_class;
    uint8_t lap_count;
    uint8_t engine_type;
    bool bonus_pack;
    bool board_selecting_race;
    bool request_start_race;
    bool request_start_duck;
    bool duck_setup;
    uint8_t duck_score;
    WwProfile duck_top[WW_PROFILE_COUNT];
    WwRaceSelection requested_race;
    char slide_prefix[4];
    char generated_asset[WW_ARCHIVE_NAME_BYTES + 1];
    bool show_main_menu;
    bool request_quit;
    bool dirty;
    unsigned animation_frame;
    char override_asset[WW_ARCHIVE_NAME_BYTES + 1];
} WwMenu;

bool ww_menu_open(WwMenu *menu, const WwArchive *archive, WwDisplay *display,
                  WwAudio *audio, WwSettings *settings,
                  const char *override_asset);
void ww_menu_close(WwMenu *menu);
void ww_menu_update(WwMenu *menu, const WwInput *input);
bool ww_menu_render(WwMenu *menu);
bool ww_menu_render_race_pause(WwMenu *menu, unsigned selection);
void ww_menu_play_navigation_sound(WwMenu *menu);
void ww_menu_show_duck_results(WwMenu *menu, unsigned score,
                               const WwProfiles *profiles);

#endif
