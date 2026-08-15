#include "ww_menu.h"

#include "ww_common.h"

#include <stdio.h>
#include <string.h>

/* Exact main-menu labels written by sub_1F760. */
static const char *const ww_main_menu_items[] = {
    " SINGLE PLAYER RACING ",
    " TWO PLAYER RACE      ",
    " TWO PLAYER SHOOT OUT ",
    " WACKY DUCK SHOOT     ",
    " COMM#BAT~ PLAY       ",
    " GAME OPTIONS         ",
    " WACKY WHEELS INFO    ",
    " ORDERING INFO        ",
    " LEAVE GAME           "
};

/* Confirmed submenu text from loc_210BA/sub_16984/sub_163FC/sub_1647C. */
static const char *const ww_options_items[] = {
    " GRAPHIC DETAIL LEVELS ",
    " VOLUME LEVELS         ",
    " RACE SCREEN OPTIONS   ",
    " LEAVE THIS MENU       "
};

static const char *const ww_graphics_items[] = {
    " SINGLE SCREEN DETAIL ",
    " SPLIT SCREEN DETAIL  ",
    " LEAVE THIS MENU      "
};

static const char *const ww_detail_items[] = {
    " HIGH   DETAIL ",
    " MEDIUM DETAIL ",
    " LOW    DETAIL "
};

static const char *const ww_info_items[] = {
    " ORDERING        ",
    " INSTRUCTIONS    ",
    " FAMOUS DRIVERS  ",
    " CREDITS         ",
    " LEAVE THIS MENU "
};

static const char *const ww_volume_items[] = {
    "MUSIC     -",
    "SOUND FX  -",
    "ENGINE FX -"
};

/* Vehicle names/taglines are the exact sub_16B38 switch strings. */
static const char *const ww_vehicle_names[] = {
    "       UNO       ", "      SULTAN     ", "      MORRIS     ",
    "     PEGGLES     ", "      RAZER      ", "      RINGO      ",
    "     BLOMBO      ", "      TIGI       "
};

static const char *const ww_vehicle_taglines[] = {
    "  CUTE AND FAST  ", " ONE HUMP OR TWO ", "   OLD RELIABLE  ",
    "  TURBO SQUAWK!  ", "  KILLER WHEELS  ", "   RAPID RASCAL  ",
    "  NEVER FORGETS  ", "  VROOM! GROWL!  "
};

static const char *const ww_race_class_items[] = {
    " AMATEUR  CLASS ", " PRO      CLASS ", " CHAMPION CLASS ",
    " TIME TRIAL     ", " KID MODE       "
};

static const char *const ww_lap_items[] = {
    " SIX LAP RACE   ", " EIGHT LAP RACE ", " TEN LAP RACE   "
};

static const char *const ww_engine_items[] = {
    " FAST 12 HP ENGINES ", " SLOW 6 HP ENGINES  "
};

static const char *const ww_race_pack_items[] = {
    " REGULAR RACES    ", " BONUS PACK RACES "
};

static const char *const ww_duck_track_items[] = {
    " DUCK COURSE 1 ", " DUCK COURSE 2 ", " DUCK COURSE 3 ",
    " DUCK COURSE 4 ", " DUCK COURSE 5 ", " DUCK COURSE 6 "
};

static void ww_menu_update_title(WwMenu *menu);

static void ww_menu_play_navigation(WwMenu *menu)
{
    if (menu != NULL && menu->audio != NULL &&
        menu->navigation_sound.samples != NULL) {
        (void)ww_audio_play(menu->audio, &menu->navigation_sound,
                            menu->settings->sound_volume);
    }
}

void ww_menu_play_navigation_sound(WwMenu *menu)
{
    ww_menu_play_navigation(menu);
}

static void ww_menu_items(const WwMenu *menu, const char *const **items,
                          unsigned *count)
{
    switch (menu->screen) {
    case WW_MENU_OPTIONS:
        *items = ww_options_items;
        *count = (unsigned)(sizeof(ww_options_items) / sizeof(ww_options_items[0]));
        break;
    case WW_MENU_GRAPHICS:
        *items = ww_graphics_items;
        *count = (unsigned)(sizeof(ww_graphics_items) / sizeof(ww_graphics_items[0]));
        break;
    case WW_MENU_SINGLE_DETAIL:
    case WW_MENU_SPLIT_DETAIL:
        *items = ww_detail_items;
        *count = (unsigned)(sizeof(ww_detail_items) / sizeof(ww_detail_items[0]));
        break;
    case WW_MENU_INFO:
        *items = ww_info_items;
        *count = (unsigned)(sizeof(ww_info_items) / sizeof(ww_info_items[0]));
        break;
    case WW_MENU_VOLUME:
        *items = ww_volume_items;
        *count = (unsigned)(sizeof(ww_volume_items) / sizeof(ww_volume_items[0]));
        break;
    case WW_MENU_VEHICLE:
        *items = ww_vehicle_names;
        *count = (unsigned)(sizeof(ww_vehicle_names) / sizeof(ww_vehicle_names[0]));
        break;
    case WW_MENU_RACE_CLASS:
        *items = ww_race_class_items;
        *count = (unsigned)(sizeof(ww_race_class_items) /
                            sizeof(ww_race_class_items[0]));
        break;
    case WW_MENU_LAPS:
        *items = ww_lap_items;
        *count = (unsigned)(sizeof(ww_lap_items) / sizeof(ww_lap_items[0]));
        break;
    case WW_MENU_ENGINE:
        *items = ww_engine_items;
        *count = (unsigned)(sizeof(ww_engine_items) / sizeof(ww_engine_items[0]));
        break;
    case WW_MENU_RACE_PACK:
        *items = ww_race_pack_items;
        *count = (unsigned)(sizeof(ww_race_pack_items) /
                            sizeof(ww_race_pack_items[0]));
        break;
    case WW_MENU_DUCK_TRACK:
        *items = ww_duck_track_items;
        *count = (unsigned)(sizeof(ww_duck_track_items) /
                            sizeof(ww_duck_track_items[0]));
        break;
    case WW_MENU_DUCK_INSTRUCTIONS:
    case WW_MENU_DUCK_RESULTS:
        *items = NULL;
        *count = 0u;
        break;
    case WW_MENU_RACE_SCREEN:
        *items = NULL;
        *count = 4;
        break;
    case WW_MENU_MAIN:
    default:
        *items = ww_main_menu_items;
        *count = (unsigned)(sizeof(ww_main_menu_items) / sizeof(ww_main_menu_items[0]));
        break;
    }
}

static const char *ww_menu_race_item(const WwMenu *menu, unsigned index)
{
    static const char *const on_items[] = {
        " OVERHEAD MAP  ON  ", " CLOCK DISPLAY ON  ",
        " SPEEDOMETER   ON  "
    };
    static const char *const off_items[] = {
        " OVERHEAD MAP  OFF ", " CLOCK DISPLAY OFF ",
        " SPEEDOMETER   OFF "
    };
    bool enabled;
    if (index == 3) return " LEAVE THIS MENU   ";
    enabled = index == 0 ? menu->settings->overhead_map
              : index == 1 ? menu->settings->clock_display
                           : menu->settings->speedometer;
    return enabled ? on_items[index] : off_items[index];
}

static const char *ww_menu_item(const WwMenu *menu, unsigned index)
{
    const char *const *items;
    unsigned count;
    ww_menu_items(menu, &items, &count);
    if (index >= count) return "";
    return items != NULL ? items[index] : ww_menu_race_item(menu, index);
}

static const char *ww_menu_asset(WwMenu *menu)
{
    if (menu->override_asset[0] != '\0') {
        return menu->override_asset;
    }
    if (menu->screen == WW_MENU_SLIDES) {
        snprintf(menu->generated_asset, sizeof(menu->generated_asset), "%s%u.PCX",
                 menu->slide_prefix, menu->slide_index + 1u);
        return menu->generated_asset;
    }
    if (menu->screen == WW_MENU_TRACK_BOARD) {
        snprintf(menu->generated_asset, sizeof(menu->generated_asset), "%sSB%u.PCX",
                 menu->bonus_pack ? "B" : "", menu->board_page + 1u);
        return menu->generated_asset;
    }
    if (menu->screen == WW_MENU_DUCK_TRACK) return "SBCS.PCX";
    if (menu->screen == WW_MENU_DUCK_INSTRUCTIONS ||
        menu->screen == WW_MENU_DUCK_RESULTS) return "ORDER.PCX";
    /* The normal sub_1F858/sub_155C0 menu path redraws Picbufs[0].  On entry
     * and after a race, loc_1FF72/loc_2007C load CHECK.PCX into that buffer.
     * BEACH.PCX is only the startup palette source in sub_35CA4. */
    return "CHECK.PCX";
}

static bool ww_menu_current_race(const WwMenu *menu,
                                 WwRaceSelection *selection)
{
    return ww_race_selection_init(selection, menu->bonus_pack,
                                  menu->board_page, menu->board_race,
                                  menu->engine_type);
}

static void ww_menu_request_current_race(WwMenu *menu)
{
    if (ww_menu_current_race(menu, &menu->requested_race)) {
        menu->request_start_race = true;
        ww_menu_update_title(menu);
    }
}

static void ww_menu_update_title(WwMenu *menu)
{
    char title[192];
    const char *asset = ww_menu_asset(menu);
    if (menu->screen == WW_MENU_TRACK_BOARD &&
        (menu->board_selecting_race || menu->request_start_race)) {
        WwRaceSelection selection;
        char race_title[64];
        if (menu->request_start_race) {
            selection = menu->requested_race;
        } else if (!ww_menu_current_race(menu, &selection)) {
            snprintf(title, sizeof(title), "Wacky Wheels 95 - %s", asset);
            ww_display_set_title(menu->display, title);
            return;
        }
        if (ww_race_selection_title(&selection, race_title,
                                    sizeof(race_title))) {
            snprintf(title, sizeof(title), "Wacky Wheels 95 - %s", race_title);
        } else {
            snprintf(title, sizeof(title), "Wacky Wheels 95 - %s", asset);
        }
    } else if (menu->show_main_menu && menu->screen != WW_MENU_SLIDES &&
        menu->screen != WW_MENU_TRACK_BOARD) {
        snprintf(title, sizeof(title), "Wacky Wheels 95 - %s",
                 ww_menu_item(menu, menu->selection));
    } else {
        snprintf(title, sizeof(title), "Wacky Wheels 95 - %s", asset);
    }
    ww_display_set_title(menu->display, title);
}

static bool ww_menu_load_screen(WwMenu *menu)
{
    WwArchiveView view;
    const char *asset = ww_menu_asset(menu);
    ww_pcx_free(&menu->image);
    if (!ww_archive_view(menu->archive, asset, &view) ||
        !ww_pcx_decode(view.data, view.size, &menu->image)) {
        ww_error("cannot decode menu screen %s", asset);
        return false;
    }
    ww_menu_update_title(menu);
    menu->dirty = true;
    return true;
}

/* sub_1E2E4 mode 1 constructs OR1.PCX..OR11.PCX; mode 2 constructs
 * INT1.PCX..INT8.PCX.  Its left/right bounds are preserved here. */
static void ww_menu_open_slides(WwMenu *menu, const char *prefix,
                                unsigned count, WwMenuScreen parent)
{
    menu->slide_parent = parent;
    menu->screen = WW_MENU_SLIDES;
    menu->slide_index = 0;
    menu->slide_count = count;
    strncpy(menu->slide_prefix, prefix, sizeof(menu->slide_prefix) - 1);
    if (!ww_menu_load_screen(menu)) {
        menu->screen = parent;
        menu->slide_count = 0;
        (void)ww_menu_load_screen(menu);
    }
}

bool ww_menu_open(WwMenu *menu, const WwArchive *archive, WwDisplay *display,
                  WwAudio *audio, WwSettings *settings,
                  const char *override_asset)
{
    WwArchiveView handles;
    WwArchiveView cars;
    memset(menu, 0, sizeof(*menu));
    menu->archive = archive;
    menu->display = display;
    menu->audio = audio;
    menu->settings = settings;
    menu->screen = WW_MENU_MAIN;
    menu->show_main_menu = override_asset == NULL;
    if (override_asset != NULL) {
        strncpy(menu->override_asset, override_asset, sizeof(menu->override_asset) - 1);
    }
    if (audio != NULL && audio->device != 0) {
        (void)ww_audio_load_voc(archive, "START.VOC", &menu->select_sound);
        (void)ww_audio_load_voc(archive, "PLIP.VOC", &menu->navigation_sound);
    }
    if (!ww_archive_view(archive, "HANDLE.SP", &handles) ||
        handles.size != 8u * 20u * 23u) {
        return false;
    }
    menu->handle_sprites = handles.data;
    if (!ww_archive_view(archive, "CARS.SP", &cars) ||
        cars.size != 8u * 12u * 38u * 28u) {
        return false;
    }
    menu->car_sprites = cars.data;
    return ww_font_load_menu(&menu->font, archive) && ww_menu_load_screen(menu);
}

void ww_menu_close(WwMenu *menu)
{
    if (menu == NULL) {
        return;
    }
    if (menu->audio != NULL) {
        ww_audio_stop_all(menu->audio);
    }
    ww_audio_free_sound(&menu->select_sound);
    ww_audio_free_sound(&menu->navigation_sound);
    ww_pcx_free(&menu->image);
    memset(menu, 0, sizeof(*menu));
}

void ww_menu_update(WwMenu *menu, const WwInput *input)
{
    if (menu->screen == WW_MENU_DUCK_INSTRUCTIONS) {
        if (ww_input_pressed(input, WW_SCAN_ESCAPE)) {
            menu->duck_setup = false;
            menu->screen = WW_MENU_MAIN;
            menu->selection = 3u;
            (void)ww_menu_load_screen(menu);
        } else if (ww_input_pressed(input, WW_SCAN_ENTER) ||
                   ww_input_pressed(input, WW_SCAN_SPACE)) {
            menu->screen = WW_MENU_VEHICLE;
            menu->selection = menu->selected_vehicle;
            (void)ww_menu_load_screen(menu);
        }
        return;
    }
    if (menu->screen == WW_MENU_DUCK_RESULTS) {
        if (ww_input_pressed(input, WW_SCAN_ESCAPE) ||
            ww_input_pressed(input, WW_SCAN_ENTER) ||
            ww_input_pressed(input, WW_SCAN_SPACE)) {
            menu->duck_setup = false;
            menu->screen = WW_MENU_MAIN;
            menu->selection = 3u;
            (void)ww_menu_load_screen(menu);
        }
        return;
    }
    if (menu->screen == WW_MENU_DUCK_TRACK) {
        if (menu->request_start_duck) return;
        if (ww_input_pressed(input, WW_SCAN_ESCAPE)) {
            menu->screen = WW_MENU_VEHICLE;
            menu->selection = menu->selected_vehicle;
            (void)ww_menu_load_screen(menu);
        } else if (ww_input_pressed(input, WW_SCAN_UP) ||
                   ww_input_pressed(input, WW_SCAN_LEFT)) {
            menu->selection = (menu->selection + 5u) % 6u;
            ww_menu_play_navigation(menu);
            menu->dirty = true;
        } else if (ww_input_pressed(input, WW_SCAN_DOWN) ||
                   ww_input_pressed(input, WW_SCAN_RIGHT)) {
            menu->selection = (menu->selection + 1u) % 6u;
            ww_menu_play_navigation(menu);
            menu->dirty = true;
        } else if (ww_input_pressed(input, WW_SCAN_ENTER) ||
                   ww_input_pressed(input, WW_SCAN_SPACE)) {
            unsigned track_number = WW_DUCK_TRACK_FIRST + menu->selection;
            if (ww_race_selection_init(
                    &menu->requested_race, false,
                    (track_number - 1u) / WW_RACES_PER_BOARD,
                    (track_number - 1u) % WW_RACES_PER_BOARD, 1u)) {
                menu->request_start_duck = true;
                menu->requested_race.track_number = (uint8_t)track_number;
                ww_menu_update_title(menu);
            }
        }
        return;
    }
    if (menu->screen == WW_MENU_SLIDES) {
        if (ww_input_pressed(input, WW_SCAN_ESCAPE)) {
            menu->screen = menu->slide_parent;
            menu->selection = 0;
            menu->slide_count = 0;
            (void)ww_menu_load_screen(menu);
            return;
        }
        if ((ww_input_pressed(input, WW_SCAN_RIGHT) ||
             ww_input_pressed(input, WW_SCAN_DOWN)) &&
            menu->slide_index + 1u < menu->slide_count) {
            ++menu->slide_index;
            ww_menu_play_navigation(menu);
            (void)ww_menu_load_screen(menu);
        } else if ((ww_input_pressed(input, WW_SCAN_LEFT) ||
                    ww_input_pressed(input, WW_SCAN_UP)) &&
                   menu->slide_index != 0) {
            --menu->slide_index;
            ww_menu_play_navigation(menu);
            (void)ww_menu_load_screen(menu);
        }
        return;
    }
    if (menu->screen == WW_MENU_TRACK_BOARD) {
        if (ww_input_pressed(input, WW_SCAN_ESCAPE)) {
            menu->screen = WW_MENU_RACE_PACK;
            menu->selection = menu->bonus_pack ? 1u : 0u;
            menu->board_selecting_race = false;
            menu->request_start_race = false;
            (void)ww_menu_load_screen(menu);
        } else if (menu->request_start_race) {
            /* The race owner consumes this typed handoff once the remaining
             * sub_17928 initialization path has been translated. */
            return;
        } else if (!menu->board_selecting_race) {
            if (ww_input_pressed(input, WW_SCAN_RIGHT) ||
                ww_input_pressed(input, WW_SCAN_DOWN)) {
                menu->board_page =
                    (menu->board_page + 1u) % WW_RACE_BOARD_PAGES;
                ww_menu_play_navigation(menu);
                (void)ww_menu_load_screen(menu);
            } else if (ww_input_pressed(input, WW_SCAN_LEFT) ||
                       ww_input_pressed(input, WW_SCAN_UP)) {
                menu->board_page =
                    (menu->board_page + WW_RACE_BOARD_PAGES - 1u) %
                    WW_RACE_BOARD_PAGES;
                ww_menu_play_navigation(menu);
                (void)ww_menu_load_screen(menu);
            } else if (ww_input_pressed(input, WW_SCAN_ENTER) ||
                       ww_input_pressed(input, WW_SCAN_SPACE)) {
                /* sub_1832C accepts the board immediately for championship
                 * classes.  Class 4 enters its 1..5 time-trial selector. */
                if (menu->race_class == 4u) {
                    menu->board_selecting_race = true;
                    menu->board_race = 0;
                    menu->dirty = true;
                    ww_menu_update_title(menu);
                } else {
                    menu->board_race = 0;
                    ww_menu_request_current_race(menu);
                }
            }
        } else if (ww_input_pressed(input, WW_SCAN_RIGHT) ||
                   ww_input_pressed(input, WW_SCAN_DOWN)) {
            menu->board_race =
                (menu->board_race + 1u) % WW_RACES_PER_BOARD;
            ww_menu_play_navigation(menu);
            menu->dirty = true;
            ww_menu_update_title(menu);
        } else if (ww_input_pressed(input, WW_SCAN_LEFT) ||
                   ww_input_pressed(input, WW_SCAN_UP)) {
            menu->board_race =
                (menu->board_race + WW_RACES_PER_BOARD - 1u) %
                WW_RACES_PER_BOARD;
            ww_menu_play_navigation(menu);
            menu->dirty = true;
            ww_menu_update_title(menu);
        } else if (ww_input_pressed(input, WW_SCAN_ENTER) ||
                   ww_input_pressed(input, WW_SCAN_SPACE)) {
            ww_menu_request_current_race(menu);
        }
        return;
    }
    if (ww_input_pressed(input, WW_SCAN_ESCAPE)) {
        if (menu->screen == WW_MENU_MAIN || !menu->show_main_menu) {
            menu->request_quit = true;
        } else if (menu->screen == WW_MENU_SINGLE_DETAIL ||
                   menu->screen == WW_MENU_SPLIT_DETAIL) {
            menu->screen = WW_MENU_GRAPHICS;
            menu->selection = 0;
            menu->dirty = true;
        } else if (menu->screen == WW_MENU_GRAPHICS ||
                   menu->screen == WW_MENU_RACE_SCREEN ||
                   menu->screen == WW_MENU_VOLUME) {
            menu->screen = WW_MENU_OPTIONS;
            menu->selection = 0;
            menu->dirty = true;
        } else if (menu->screen == WW_MENU_VEHICLE) {
            if (menu->duck_setup) {
                menu->screen = WW_MENU_DUCK_INSTRUCTIONS;
                menu->selection = 0u;
                (void)ww_menu_load_screen(menu);
            } else {
                menu->screen = WW_MENU_MAIN;
                menu->selection = 0;
            }
            menu->dirty = true;
        } else if (menu->screen == WW_MENU_RACE_CLASS) {
            menu->screen = WW_MENU_VEHICLE;
            menu->selection = menu->selected_vehicle;
            menu->dirty = true;
        } else if (menu->screen == WW_MENU_LAPS) {
            menu->screen = WW_MENU_RACE_CLASS;
            menu->selection = menu->race_class - 1u;
            menu->dirty = true;
        } else if (menu->screen == WW_MENU_ENGINE) {
            if (menu->race_class == 4 || menu->race_class == 5) {
                menu->screen = WW_MENU_RACE_CLASS;
                menu->selection = menu->race_class - 1u;
            } else {
                menu->screen = WW_MENU_LAPS;
                menu->selection = menu->lap_count / 2u - 3u;
            }
            menu->dirty = true;
        } else if (menu->screen == WW_MENU_RACE_PACK) {
            menu->screen = WW_MENU_ENGINE;
            menu->selection = menu->engine_type - 1u;
            menu->dirty = true;
        } else {
            menu->screen = WW_MENU_MAIN;
            menu->selection = 0;
            menu->dirty = true;
        }
        ww_menu_update_title(menu);
        return;
    }
    if (!menu->show_main_menu) {
        return;
    }
    if (menu->screen == WW_MENU_VEHICLE) {
        unsigned old_selection = menu->selection;
        if (ww_input_pressed(input, WW_SCAN_LEFT)) {
            menu->selection = (menu->selection & 4u) |
                              ((menu->selection + 3u) & 3u);
        } else if (ww_input_pressed(input, WW_SCAN_RIGHT)) {
            menu->selection = (menu->selection & 4u) |
                              ((menu->selection + 1u) & 3u);
        } else if (ww_input_pressed(input, WW_SCAN_UP) ||
                   ww_input_pressed(input, WW_SCAN_DOWN)) {
            menu->selection ^= 4u;
        }
        if (old_selection != menu->selection) {
            ww_menu_play_navigation(menu);
            ww_menu_update_title(menu);
        }
        menu->dirty = true;
    } else {
    {
        const char *const *unused_items;
        unsigned item_count;
        ww_menu_items(menu, &unused_items, &item_count);
    if (ww_input_pressed(input, WW_SCAN_UP)) {
        menu->selection = menu->selection == 0
                              ? item_count - 1u
                              : menu->selection - 1;
        ww_menu_play_navigation(menu);
        menu->dirty = true;
        ww_menu_update_title(menu);
    } else if (ww_input_pressed(input, WW_SCAN_DOWN)) {
        menu->selection = (menu->selection + 1) % item_count;
        ww_menu_play_navigation(menu);
        menu->dirty = true;
        ww_menu_update_title(menu);
    }
    }
    }

    if (menu->screen == WW_MENU_VOLUME &&
        (ww_input_pressed(input, WW_SCAN_LEFT) ||
         ww_input_pressed(input, WW_SCAN_RIGHT))) {
        uint8_t *volume = menu->selection == 0 ? &menu->settings->music_volume
                          : menu->selection == 1 ? &menu->settings->sound_volume
                                                 : &menu->settings->engine_volume;
        int adjusted = *volume + (ww_input_pressed(input, WW_SCAN_LEFT) ? -5 : 5);
        if (adjusted < 5) adjusted = 5;
        if (adjusted > 255) adjusted = 255;
        *volume = (uint8_t)adjusted;
        if (menu->selection == 1 && menu->audio != NULL) {
            menu->audio->master_volume = *volume;
        } else if (menu->selection == 0 && menu->audio != NULL) {
            ww_audio_set_music_volume(menu->audio, *volume);
        }
        menu->dirty = true;
    }
    if (ww_input_pressed(input, WW_SCAN_ENTER) ||
        ww_input_pressed(input, WW_SCAN_SPACE)) {
        if (menu->select_sound.samples != NULL) {
            (void)ww_audio_play(menu->audio, &menu->select_sound, 192);
        }
        switch (menu->screen) {
        case WW_MENU_MAIN:
            if (menu->selection == 0) {
                menu->duck_setup = false;
                menu->screen = WW_MENU_VEHICLE;
                menu->selection = menu->selected_vehicle;
            } else if (menu->selection == 3) {
                menu->duck_setup = true;
                menu->screen = WW_MENU_DUCK_INSTRUCTIONS;
                menu->selection = 0u;
                (void)ww_menu_load_screen(menu);
            } else if (menu->selection == 5) {
                menu->screen = WW_MENU_OPTIONS;
                menu->selection = 0;
            } else if (menu->selection == 6) {
                menu->screen = WW_MENU_INFO;
                menu->selection = 0;
            } else if (menu->selection == 7) {
                ww_menu_open_slides(menu, "OR", 11, WW_MENU_MAIN);
            } else if (menu->selection == 8) {
                menu->request_quit = true;
            }
            break;
        case WW_MENU_VEHICLE:
            menu->selected_vehicle = (uint8_t)menu->selection;
            menu->screen = menu->duck_setup ? WW_MENU_DUCK_TRACK
                                            : WW_MENU_RACE_CLASS;
            menu->selection = 0;
            if (menu->duck_setup) (void)ww_menu_load_screen(menu);
            break;
        case WW_MENU_RACE_CLASS:
            menu->race_class = (uint8_t)(menu->selection + 1u);
            if (menu->race_class == 4) {
                menu->lap_count = 6;
                menu->screen = WW_MENU_ENGINE;
                menu->selection = 0;
            } else if (menu->race_class == 5) {
                menu->lap_count = 3;
                menu->screen = WW_MENU_ENGINE;
                menu->selection = 0;
            } else {
                menu->screen = WW_MENU_LAPS;
                menu->selection = 0;
            }
            break;
        case WW_MENU_LAPS:
            menu->lap_count = (uint8_t)(6u + menu->selection * 2u);
            menu->screen = WW_MENU_ENGINE;
            menu->selection = 0;
            break;
        case WW_MENU_ENGINE:
            menu->engine_type = (uint8_t)(menu->selection + 1u);
            menu->screen = WW_MENU_RACE_PACK;
            menu->selection = 0;
            break;
        case WW_MENU_RACE_PACK:
            menu->bonus_pack = menu->selection == 1;
            menu->screen = WW_MENU_TRACK_BOARD;
            menu->board_page = 0;
            menu->board_race = 0;
            menu->board_selecting_race = false;
            menu->request_start_race = false;
            (void)ww_menu_load_screen(menu);
            break;
        case WW_MENU_OPTIONS:
            if (menu->selection == 0) menu->screen = WW_MENU_GRAPHICS;
            else if (menu->selection == 1) menu->screen = WW_MENU_VOLUME;
            else if (menu->selection == 2) menu->screen = WW_MENU_RACE_SCREEN;
            else menu->screen = WW_MENU_MAIN;
            menu->selection = 0;
            break;
        case WW_MENU_GRAPHICS:
            if (menu->selection == 0) {
                menu->screen = WW_MENU_SINGLE_DETAIL;
                menu->selection = menu->settings->single_screen_detail - 1u;
            } else if (menu->selection == 1) {
                menu->screen = WW_MENU_SPLIT_DETAIL;
                menu->selection = menu->settings->split_screen_detail - 1u;
            } else {
                menu->screen = WW_MENU_OPTIONS;
                menu->selection = 0;
            }
            break;
        case WW_MENU_SINGLE_DETAIL:
            menu->settings->single_screen_detail = (uint8_t)(menu->selection + 1u);
            menu->screen = WW_MENU_GRAPHICS;
            menu->selection = 0;
            break;
        case WW_MENU_SPLIT_DETAIL:
            menu->settings->split_screen_detail = (uint8_t)(menu->selection + 1u);
            menu->screen = WW_MENU_GRAPHICS;
            menu->selection = 0;
            break;
        case WW_MENU_RACE_SCREEN:
            if (menu->selection == 0) menu->settings->overhead_map = !menu->settings->overhead_map;
            else if (menu->selection == 1) menu->settings->clock_display = !menu->settings->clock_display;
            else if (menu->selection == 2) menu->settings->speedometer = !menu->settings->speedometer;
            else {
                menu->screen = WW_MENU_OPTIONS;
                menu->selection = 0;
            }
            break;
        case WW_MENU_INFO:
            if (menu->selection == 0) {
                ww_menu_open_slides(menu, "OR", 11, WW_MENU_INFO);
            } else if (menu->selection == 1) {
                ww_menu_open_slides(menu, "INT", 8, WW_MENU_INFO);
            } else if (menu->selection == 4) {
                menu->screen = WW_MENU_MAIN;
                menu->selection = 0;
            }
            break;
        case WW_MENU_VOLUME:
            break;
        }
        if (menu->screen != WW_MENU_SLIDES &&
            menu->screen != WW_MENU_TRACK_BOARD) {
            menu->dirty = true;
            ww_menu_update_title(menu);
        }
    }
}

bool ww_menu_render(WwMenu *menu)
{
    unsigned animation = (SDL_GetTicks() / 59u) & 3u;
    if (menu->show_main_menu && menu->screen != WW_MENU_SLIDES &&
        menu->screen != WW_MENU_TRACK_BOARD &&
        animation != menu->animation_frame) {
        menu->animation_frame = animation;
        menu->dirty = true;
    }
    if (menu->dirty) {
        ww_display_set_draw_page(menu->display, 0);
        if (!ww_display_blit_pcx(menu->display, &menu->image)) {
            return false;
        }
        if (menu->show_main_menu &&
            (menu->screen == WW_MENU_DUCK_INSTRUCTIONS ||
             menu->screen == WW_MENU_DUCK_RESULTS)) {
            const char *heading = menu->screen == WW_MENU_DUCK_RESULTS
                                      ? "YOUR DUCK SCORE"
                                      : "THE WACKY DUCK SHOOT";
            char score[16];
            int width = ww_font_menu_text_width(heading);
            ww_font_draw_menu_text(&menu->font, menu->display,
                                   (WW_SCREEN_WIDTH - width) / 2, 14,
                                   heading);
            if (menu->screen == WW_MENU_DUCK_RESULTS) {
                unsigned i;
                snprintf(score, sizeof(score), "%02u",
                         (unsigned)menu->duck_score);
                width = ww_font_menu_text_width(score);
                ww_font_draw_menu_text(&menu->font, menu->display,
                                       (WW_SCREEN_WIDTH - width) / 2, 60,
                                       score);
                width = ww_font_menu_text_width("PRESS ENTER");
                ww_font_draw_menu_text(&menu->font, menu->display,
                                       (WW_SCREEN_WIDTH - width) / 2, 164,
                                       "PRESS ENTER");
                width = ww_font_menu_text_width("TOP SHOOTERS");
                ww_font_draw_menu_text(&menu->font, menu->display,
                                       (WW_SCREEN_WIDTH - width) / 2, 82,
                                       "TOP SHOOTERS");
                for (i = 0u; i < WW_PROFILE_COUNT; ++i) {
                    char line[32];
                    snprintf(line, sizeof(line), "%-16.16s %02u",
                             menu->duck_top[i].name,
                             (unsigned)(menu->duck_top[i].value > 99u
                                            ? 99u
                                            : menu->duck_top[i].value));
                    width = ww_font_menu_text_width(line);
                    ww_font_draw_menu_text(
                        &menu->font, menu->display,
                        (WW_SCREEN_WIDTH - width) / 2,
                        98 + (int)i * 13, line);
                }
            } else {
                const char *line1 = "YOU HAVE TWO MINUTES TO";
                const char *line2 = "BLAST AS MANY DUCKS AS YOU CAN!";
                width = ww_font_menu_text_width(line1);
                ww_font_draw_menu_text(&menu->font, menu->display,
                                       (WW_SCREEN_WIDTH - width) / 2, 48,
                                       line1);
                width = ww_font_menu_text_width(line2);
                ww_font_draw_menu_text(&menu->font, menu->display,
                                       (WW_SCREEN_WIDTH - width) / 2, 62,
                                       line2);
                width = ww_font_menu_text_width("PRESS ENTER");
                ww_font_draw_menu_text(&menu->font, menu->display,
                                       (WW_SCREEN_WIDTH - width) / 2, 164,
                                       "PRESS ENTER");
            }
        } else if (menu->show_main_menu && menu->screen == WW_MENU_VEHICLE) {
            unsigned i;
            int heading_width = ww_font_menu_text_width("CHOOSE VEHICLE");
            ww_font_draw_menu_text(&menu->font, menu->display,
                                   (WW_SCREEN_WIDTH - heading_width) / 2, 96,
                                   "CHOOSE VEHICLE");
            for (i = 0; i < 8; ++i) {
                unsigned frame = i == menu->selection
                                     ? (SDL_GetTicks() / 59u) % 8u : 0u;
                int x = 46 + (int)(i % 4u) * 64;
                int y = i < 4 ? 58 : 114;
                const uint8_t *sprite = menu->car_sprites +
                    ((size_t)i * 12u + frame) * 38u * 28u;
                ww_display_blit_column_major(menu->display, x, y, 38, 28,
                                              sprite, 28, 0);
            }
            {
                const char *name = ww_vehicle_names[menu->selection];
                const char *tagline = ww_vehicle_taglines[menu->selection];
                ww_font_draw_menu_text(&menu->font, menu->display, 46, 175, name);
                ww_font_draw_menu_text(&menu->font, menu->display, 46, 187,
                                       tagline);
            }
        } else if (menu->show_main_menu && menu->screen != WW_MENU_SLIDES &&
                   menu->screen != WW_MENU_TRACK_BOARD) {
            const char *const *unused_items;
            unsigned item_count;
            unsigned i;
            ww_menu_items(menu, &unused_items, &item_count);
            int y = (WW_SCREEN_HEIGHT -
                     (int)item_count * 12) / 2 +
                    (menu->screen == WW_MENU_MAIN ? 30 : 12);
            if (menu->screen == WW_MENU_VOLUME) y = 70;
            for (i = 0; i < item_count; ++i) {
                const char *item = ww_menu_item(menu, i);
                int width = ww_font_menu_text_width(item);
                int x = (WW_SCREEN_WIDTH - width) / 2;
                ww_font_draw_menu_text(&menu->font, menu->display, x, y, item);
                if (menu->screen == WW_MENU_VOLUME) {
                    unsigned volume = i == 0 ? menu->settings->music_volume
                                      : i == 1 ? menu->settings->sound_volume
                                               : menu->settings->engine_volume;
                    unsigned bar = volume / 5u;
                    if (bar < 1u) bar = 1u;
                    if (bar > 51u) bar = 51u;
                    ww_display_draw_box(menu->display, x + width + 4, y + 4,
                                        bar * 4u, 5, 0x2c, 0x26);
                }
                if (i == menu->selection) {
                    size_t handle_offset =
                        (size_t)menu->animation_frame * 20u * 23u;
                    /* dword_6A518/dword_6A528 are the four left/right
                     * HANDLE.SP frame pointers advanced at the eight-tick
                     * loc_1604C cadence in sub_155C0. */
                    ww_display_blit_column_major(
                        menu->display, x - 18, y - 7, 20, 23,
                        menu->handle_sprites + handle_offset, 23, 0);
                    ww_display_blit_column_major(
                        menu->display, x + width + 3, y - 7, 20, 23,
                        menu->handle_sprites + 4u * 20u * 23u + handle_offset,
                        23, 0);
                }
                y += 12;
            }
        }
        ww_display_set_visible_page(menu->display, 0);
        menu->dirty = false;
    }
    return ww_display_present(menu->display);
}

void ww_menu_show_duck_results(WwMenu *menu, unsigned score,
                               const WwProfiles *profiles)
{
    if (menu == NULL) return;
    menu->duck_score = (uint8_t)(score > 99u ? 99u : score);
    menu->request_start_duck = false;
    menu->screen = WW_MENU_DUCK_RESULTS;
    menu->selection = 0u;
    if (profiles != NULL) {
        memcpy(menu->duck_top, profiles->profile,
               sizeof(menu->duck_top));
    }
    (void)ww_menu_load_screen(menu);
}

bool ww_menu_render_race_pause(WwMenu *menu, unsigned selection)
{
    static const char *const items[] = {
        " BACK TO RACE  ", " LEAVE RACE    ", " RESTART RACE  "
    };
    unsigned i;
    unsigned animation;
    int widest = 0;
    int y = 82;
    if (menu == NULL || menu->display == NULL || selection >= 3u) return false;
    animation = (SDL_GetTicks() / 59u) & 3u;
    for (i = 0; i < 3u; ++i) {
        int width = ww_font_menu_text_width(items[i]);
        if (width > widest) widest = width;
    }
    ww_display_set_draw_page(menu->display, 0);
    ww_display_draw_box(menu->display, (WW_SCREEN_WIDTH - widest) / 2 - 25,
                        y - 13, (unsigned)widest + 50u, 62u, 0x2cu, 0x26u);
    for (i = 0; i < 3u; ++i) {
        int width = ww_font_menu_text_width(items[i]);
        int x = (WW_SCREEN_WIDTH - width) / 2;
        ww_font_draw_menu_text(&menu->font, menu->display, x, y, items[i]);
        if (i == selection) {
            size_t offset = (size_t)animation * 20u * 23u;
            ww_display_blit_column_major(menu->display, x - 18, y - 7,
                                          20, 23,
                                          menu->handle_sprites + offset,
                                          23, 0);
            ww_display_blit_column_major(
                menu->display, x + width + 3, y - 7, 20, 23,
                menu->handle_sprites + 4u * 20u * 23u + offset, 23, 0);
        }
        y += 12;
    }
    ww_display_set_visible_page(menu->display, 0);
    return ww_display_present(menu->display);
}
