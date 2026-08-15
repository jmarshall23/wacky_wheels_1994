#include "ww_victory.h"

#include "ww_common.h"
#include "ww_sprite.h"

#include <stdio.h>
#include <string.h>

enum {
    WW_VICTORY_PUFF_FRAMES = 4,
    WW_VICTORY_PUFF_FRAME_BYTES = WW_CAR_SOURCE_BYTES,
    WW_VICTORY_ANIMATION_STEP_TICKS = 6,
    WW_VICTORY_PUFF_DURATION_TICKS = 36
};

static const char *const ww_victory_vehicle_assets[WW_RACER_COUNT] = {
    "PANDA.SP", "CAMEL.SP", "MOOSE.SP", "PELICAN.SP",
    "SHARK.SP", "RINGO.SP", "ELE.SP", "TIGER.SP"
};

static const char *const ww_victory_racer_names[WW_RACER_COUNT] = {
    "UNO", "SULTAN", "MORRIS", "PEGGLES",
    "RAZER", "RINGO", "BLOMBO", "TIGI"
};

static const uint8_t ww_victory_recovery_count[WW_RACER_COUNT] = {
    4, 4, 4, 4, 2, 1, 2, 1
};

static const uint8_t ww_victory_finish_count[WW_RACER_COUNT] = {
    2, 4, 8, 8, 2, 7, 2, 4
};

static bool ww_victory_accept_down(const WwInput *input)
{
    return input != NULL &&
           (ww_input_down(input, WW_SCAN_ENTER) ||
            ww_input_down(input, WW_SCAN_SPACE) || input->fire);
}

static unsigned ww_victory_sort_key(const WwRace *race, unsigned racer)
{
    uint16_t place = race->racer_lap[racer].finish_place;
    if (place != 0u) return place;
    return WW_RACER_COUNT + race->racer_rank[racer];
}

bool ww_victory_open(WwVictory *victory, const WwArchive *archive,
                     WwDisplay *display, const WwRace *race)
{
    WwVictory loaded;
    WwArchiveView view;
    unsigned i;
    if (victory == NULL || archive == NULL || display == NULL ||
        race == NULL || !race->open) {
        return false;
    }
    memset(&loaded, 0, sizeof(loaded));
    loaded.display = display;
    if (!ww_archive_view(archive, "ORDER.PCX", &view) ||
        !ww_pcx_decode(view.data, view.size, &loaded.order_image) ||
        !ww_archive_view(archive, "WIN.PCX", &view) ||
        !ww_pcx_decode(view.data, view.size, &loaded.win_image) ||
        !ww_font_load_menu(&loaded.font, archive) ||
        !ww_archive_view(archive, "CARS.SP", &view) ||
        view.size != WW_CAR_BYTES) {
        ww_victory_close(&loaded);
        return false;
    }
    loaded.car_sprites = view.data;
    loaded.car_sprites_size = view.size;
    if (!ww_archive_view(archive, "PUF.SP", &view) ||
        view.size != WW_VICTORY_PUFF_FRAMES *
                         WW_VICTORY_PUFF_FRAME_BYTES) {
        ww_victory_close(&loaded);
        return false;
    }
    loaded.puff_sprites = view.data;
    loaded.puff_sprites_size = view.size;
    for (i = 0; i < WW_RACER_COUNT; ++i) {
        unsigned j;
        loaded.racer_vehicle[i] = race->racers[i].vehicle;
        loaded.finish_place[i] = race->racer_lap[i].finish_place;
        loaded.finish_order[i] = (uint8_t)i;
        if (!ww_archive_view(archive,
                             ww_victory_vehicle_assets[
                                 loaded.racer_vehicle[i]], &view)) {
            ww_victory_close(&loaded);
            return false;
        }
        loaded.vehicle_sprites[i] = view.data;
        loaded.vehicle_sprites_size[i] = view.size;
        for (j = i; j > 0u; --j) {
            unsigned left = loaded.finish_order[j - 1u];
            unsigned right = loaded.finish_order[j];
            if (ww_victory_sort_key(race, left) <=
                ww_victory_sort_key(race, right)) {
                break;
            }
            loaded.finish_order[j - 1u] = (uint8_t)right;
            loaded.finish_order[j] = (uint8_t)left;
        }
    }
    loaded.player_vehicle = race->racers[0].vehicle;
    loaded.player_place = (uint8_t)race->racer_lap[0].finish_place;
    loaded.race_selection = race->selection;
    loaded.selected_vehicle = race->selected_vehicle;
    loaded.lap_count = race->lap_count;
    loaded.race_class = race->race_class;
    loaded.clock_display = race->clock_display;
    loaded.speedometer = race->speedometer;
    loaded.overhead_map = race->minimap.enabled;
    loaded.final_race = race->selection.race_index ==
                        WW_RACES_PER_BOARD - 1u;
    loaded.stage = WW_VICTORY_ORDER;
    loaded.wait_for_release = true;
    loaded.open = true;
    *victory = loaded;
    return true;
}

void ww_victory_close(WwVictory *victory)
{
    if (victory == NULL) return;
    ww_pcx_free(&victory->order_image);
    ww_pcx_free(&victory->win_image);
    memset(victory, 0, sizeof(*victory));
}

void ww_victory_update(WwVictory *victory, const WwInput *input,
                       unsigned elapsed_136_ticks)
{
    if (victory == NULL || !victory->open ||
        victory->stage == WW_VICTORY_COMPLETE) {
        return;
    }
    victory->stage_ticks += elapsed_136_ticks;
    victory->animation_tick_accumulator += elapsed_136_ticks;
    while (victory->animation_tick_accumulator >=
           WW_VICTORY_ANIMATION_STEP_TICKS) {
        victory->animation_tick_accumulator -=
            WW_VICTORY_ANIMATION_STEP_TICKS;
        ++victory->animation_frame;
    }
    if (victory->wait_for_release) {
        if (!ww_victory_accept_down(input)) {
            victory->wait_for_release = false;
        }
        return;
    }
    if (!ww_victory_accept_down(input) ||
        (victory->stage == WW_VICTORY_ROSTRUM &&
         victory->stage_ticks < WW_VICTORY_PUFF_DURATION_TICKS)) {
        return;
    }
    if (victory->stage == WW_VICTORY_ORDER && victory->final_race) {
        victory->stage = WW_VICTORY_ROSTRUM;
        victory->stage_ticks = 0u;
        victory->animation_tick_accumulator = 0u;
        victory->animation_frame = 0u;
        victory->wait_for_release = true;
    } else {
        victory->stage = WW_VICTORY_COMPLETE;
    }
}

static void ww_victory_draw_car(const WwVictory *victory, unsigned racer,
                                unsigned frame, int x, int y)
{
    size_t offset;
    if (victory == NULL || racer >= WW_RACER_COUNT ||
        victory->racer_vehicle[racer] >= WW_CAR_VEHICLES) {
        return;
    }
    offset = ((size_t)victory->racer_vehicle[racer] * WW_CAR_FRAMES +
              frame) * WW_CAR_SOURCE_BYTES;
    if (offset + WW_CAR_SOURCE_BYTES > victory->car_sprites_size) return;
    ww_display_blit_column_major(victory->display, x, y, 38u, 28u,
                                 victory->car_sprites + offset, 28u, 0);
}

static void ww_victory_draw_order(WwVictory *victory)
{
    unsigned row;
    const char *title = "ORDER OF FINISH";
    int title_x = (WW_SCREEN_WIDTH - ww_font_menu_text_width(title)) / 2;
    ww_font_draw_menu_text(&victory->font, victory->display,
                           title_x, 7, title);
    for (row = 0; row < WW_RACER_COUNT; ++row) {
        char line[24];
        unsigned racer = victory->finish_order[row];
        int x = row < 4u ? 20 : 177;
        int y = 32 + (int)(row & 3u) * 27;
        (void)snprintf(line, sizeof(line), "%u. %s",
                       row + 1u, ww_victory_racer_names[
                           victory->racer_vehicle[racer]]);
        ww_font_draw_menu_text(&victory->font, victory->display, x, y, line);
    }
    /* sub_2BFF0 animates the current racer's eight CARS.SP directions over
     * the ORDER.PCX table while it waits for acknowledgment. */
    ww_victory_draw_car(victory, WW_RACER_PLAYER_INDEX,
                        victory->animation_frame & 7u, 141, 160);
}

static void ww_victory_draw_rostrum(WwVictory *victory)
{
    static const int x[3] = {141, 91, 192};
    static const int y[3] = {78, 94, 97};
    unsigned place;
    if (victory->stage_ticks < WW_VICTORY_PUFF_DURATION_TICKS) {
        unsigned puff_frame =
            (victory->animation_frame % WW_VICTORY_PUFF_FRAMES);
        const uint8_t *puff = victory->puff_sprites +
            (size_t)puff_frame * WW_VICTORY_PUFF_FRAME_BYTES;
        for (place = 0; place < 3u; ++place) {
            ww_display_blit_column_major(victory->display,
                                         x[place], y[place], 38u, 28u,
                                         puff, 28u, 0);
        }
        return;
    }
    for (place = 0; place < 3u; ++place) {
        unsigned racer = victory->finish_order[place];
        if (racer == WW_RACER_PLAYER_INDEX &&
            victory->player_place >= 1u &&
            victory->player_place <= 3u) {
            unsigned vehicle = victory->racer_vehicle[racer];
            unsigned count = ww_victory_finish_count[vehicle];
            unsigned frame = 12u + ww_victory_recovery_count[vehicle] +
                (victory->animation_frame % count);
            size_t offset = (size_t)frame * WW_CAR_SOURCE_BYTES;
            if (offset + WW_CAR_SOURCE_BYTES <=
                victory->vehicle_sprites_size[racer]) {
                ww_display_blit_column_major(
                    victory->display, x[place], y[place], 38u, 28u,
                    victory->vehicle_sprites[racer] + offset, 28u, 0);
            }
        } else {
            ww_victory_draw_car(victory, racer,
                                victory->animation_frame & 7u,
                                x[place], y[place]);
        }
    }
}

bool ww_victory_render(WwVictory *victory)
{
    if (victory == NULL || !victory->open) return false;
    if (victory->stage == WW_VICTORY_COMPLETE) return true;
    ww_display_set_draw_page(victory->display, 0u);
    if (!ww_display_blit_pcx(
            victory->display,
            victory->stage == WW_VICTORY_ORDER
                ? &victory->order_image : &victory->win_image)) {
        return false;
    }
    if (victory->stage == WW_VICTORY_ORDER) {
        ww_victory_draw_order(victory);
    } else {
        ww_victory_draw_rostrum(victory);
    }
    ww_display_set_visible_page(victory->display, 0u);
    return ww_display_present(victory->display);
}

bool ww_victory_complete(const WwVictory *victory)
{
    return victory == NULL || !victory->open ||
           victory->stage == WW_VICTORY_COMPLETE;
}

bool ww_victory_has_next_race(const WwVictory *victory)
{
    return victory != NULL && victory->open && !victory->final_race &&
           victory->race_selection.race_index + 1u < WW_RACES_PER_BOARD;
}
