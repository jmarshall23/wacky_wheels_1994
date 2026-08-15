#ifndef WW_RACE_H
#define WW_RACE_H

#include "ww_archive.h"
#include "ww_ai.h"
#include "ww_display.h"
#include "ww_duck.h"
#include "ww_dynamic_object.h"
#include "ww_pcx.h"
#include "ww_physics.h"
#include "ww_racer.h"
#include "ww_renderer.h"
#include "ww_sprite.h"
#include "ww_track.h"
#include "ww_world_object.h"
#include "ww_weapon.h"
#include "ww_water.h"
#include "ww_input.h"
#include "ww_hud.h"
#include "ww_lap.h"
#include "ww_finish.h"
#include "ww_minimap.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_RACE_BOARD_PAGES = 3,
    WW_RACES_PER_BOARD = 5,
    WW_RACE_FIRST_TRACK = 1,
    WW_RACE_LAST_TRACK = 30,
    WW_RACE_FIRST_ASSET_TRACK = 1,
    WW_RACE_LAST_ASSET_TRACK = 36,
    WW_RACE_GRID_ENTRIES = 8,
    WW_RACE_SCALE_SETS = 6,
    WW_RACE_INITIAL_HEADING = 0x1e0,
    WW_RACE_MARKER_BYTES = 19500
};

/* sub_1832C keeps the board and race indices one-based.  The native menu
 * uses zero-based indices, so this structure stores both the menu choices
 * and the exact asset number selected by sub_17928. */
typedef struct WwRaceSelection {
    uint8_t board_page;
    uint8_t race_index;
    uint8_t engine_type;
    uint8_t track_number;
    bool bonus_pack;
} WwRaceSelection;

typedef struct WwRaceInitialState {
    uint16_t grid_x[WW_RACE_GRID_ENTRIES];
    uint16_t grid_y[WW_RACE_GRID_ENTRIES];
    uint16_t camera_x;
    uint16_t camera_y;
    uint16_t heading;
} WwRaceInitialState;

typedef struct WwRace {
    WwRaceSelection selection;
    WwTrack track;
    WwRaceInitialState initial;
    WwRacerState racers[WW_RACER_COUNT];
    WwAiRacerPathMotion opponent_path[WW_RACER_COUNT];
    WwPlayerMotion player;
    const uint8_t *marker_sprites;
    size_t marker_size;
    const uint8_t *velocity_table;
    size_t velocity_table_size;
    const uint8_t *car_sprites;
    size_t car_sprites_size;
    const uint8_t *vehicle_sprites;
    size_t vehicle_sprites_size;
    WwSpriteScaleSet perspective_scale[WW_RACE_SCALE_SETS];
    WwWorldSpriteCatalog world_sprites;
    WwDynamicSpriteAssets dynamic_sprites;
    WwDynamicObjectPool dynamic_objects;
    WwWeaponState player_weapon;
    WwWaterAssets water_assets;
    WwWaterState water;
    WwHudAssets hud;
    WwHudLapAlert lap_alert;
    WwLapState racer_lap[WW_RACER_COUNT];
    WwFinishState finish;
    WwMinimap minimap;
    WwDuckShoot duck;
    uint8_t racer_rank[WW_RACER_COUNT];
    int16_t player_collision_spawn;
    int16_t player_collision_racer;
    int16_t player_collision_ignore_racer;
    uint8_t player_collision_ignore_ticks;
    uint16_t horizon_source_offset;
    uint16_t horizon_left_step;
    uint16_t horizon_right_step;
    uint8_t steering_frame;
    uint8_t selected_vehicle;
    uint8_t lap_count;
    uint8_t race_class;
    uint16_t lives;
    uint16_t next_finish_place;
    uint32_t update_tick;
    uint32_t elapsed_136_ticks;
    uint32_t race_time_tenths;
    uint16_t race_time_136_remainder;
    int16_t displayed_speed;
    uint16_t start_sequence_frame;
    uint8_t start_light_frame;
    bool clock_display;
    bool speedometer;
    bool wrong_way_blink;
    bool start_released;
    bool start_sound_pending;
    bool last_lap_sound_pending;
    bool open;
} WwRace;

bool ww_race_selection_init(WwRaceSelection *selection, bool bonus_pack,
                            unsigned board_page, unsigned race_index,
                            unsigned engine_type);
bool ww_race_selection_title(const WwRaceSelection *selection,
                             char *title, size_t title_size);
bool ww_race_selection_base_name(const WwRaceSelection *selection,
                                 char *base_name, size_t base_name_size);
const char *ww_race_marker_asset(const WwRaceSelection *selection);
bool ww_race_initial_state(const WwTrack *track, WwRaceInitialState *state);
bool ww_race_open(WwRace *race, const WwArchive *archive,
                  const WwRaceSelection *selection,
                  unsigned selected_vehicle, unsigned lap_count,
                  unsigned race_class,
                  bool clock_display, bool speedometer,
                  bool overhead_map);
bool ww_race_open_duck(WwRace *race, const WwArchive *archive,
                       const WwRaceSelection *selection,
                       unsigned selected_vehicle,
                       bool speedometer, bool overhead_map);
void ww_race_close(WwRace *race);
bool ww_race_update(WwRace *race, const WwRenderer *renderer,
                    const WwInput *input, WwRoadDetail detail,
                    unsigned elapsed_136_ticks);
bool ww_race_render_bringup(WwRace *race, const WwRenderer *renderer,
                            WwDisplay *display, WwRoadDetail detail);

#endif
