#ifndef WW_VICTORY_H
#define WW_VICTORY_H

#include "ww_archive.h"
#include "ww_display.h"
#include "ww_font.h"
#include "ww_input.h"
#include "ww_pcx.h"
#include "ww_race.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum WwVictoryStage {
    WW_VICTORY_ORDER = 0,
    WW_VICTORY_ROSTRUM = 1,
    WW_VICTORY_COMPLETE = 2
} WwVictoryStage;

typedef struct WwVictory {
    WwDisplay *display;
    WwPcxImage order_image;
    WwPcxImage win_image;
    WwMenuFont font;
    const uint8_t *car_sprites;
    size_t car_sprites_size;
    const uint8_t *vehicle_sprites[WW_RACER_COUNT];
    size_t vehicle_sprites_size[WW_RACER_COUNT];
    const uint8_t *puff_sprites;
    size_t puff_sprites_size;
    uint8_t racer_vehicle[WW_RACER_COUNT];
    uint8_t finish_order[WW_RACER_COUNT];
    uint16_t finish_place[WW_RACER_COUNT];
    uint8_t player_vehicle;
    uint8_t player_place;
    WwRaceSelection race_selection;
    uint8_t selected_vehicle;
    uint8_t lap_count;
    uint8_t race_class;
    bool clock_display;
    bool speedometer;
    bool overhead_map;
    uint8_t animation_frame;
    unsigned animation_tick_accumulator;
    unsigned stage_ticks;
    WwVictoryStage stage;
    bool final_race;
    bool wait_for_release;
    bool open;
} WwVictory;

bool ww_victory_open(WwVictory *victory, const WwArchive *archive,
                     WwDisplay *display, const WwRace *race);
void ww_victory_close(WwVictory *victory);
void ww_victory_update(WwVictory *victory, const WwInput *input,
                       unsigned elapsed_136_ticks);
bool ww_victory_render(WwVictory *victory);
bool ww_victory_complete(const WwVictory *victory);
bool ww_victory_has_next_race(const WwVictory *victory);

#endif
