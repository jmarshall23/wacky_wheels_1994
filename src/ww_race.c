#include "ww_race.h"

#include "ww_perspective.h"

#include <stdio.h>
#include <string.h>

/* Exact strings selected by the six-way switch in sub_2E4B8. */
static const char *const ww_race_group_titles[] = {
    "BRONZE WHEELS. TRACK ",
    "SILVER WHEELS. TRACK ",
    "GOLD WHEELS. TRACK ",
    "BONUS BRONZE . TRACK ",
    "BONUS SILVER. TRACK ",
    "BONUS GOLD. TRACK "
};

/* sub_17928 selects one 19,500-byte marker sheet for each board group before
 * entering sub_32E60 and the track loaders. */
static const char *const ww_race_marker_assets[] = {
    "BRONZEM.SP", "SILVERM.SP", "GOLDM.SP",
    "BONUSB.SP", "BONUSS.SP", "BONUSG.SP"
};

/* Exact selected-vehicle sprite filenames installed by sub_328D4. */
static const char *const ww_race_vehicle_assets[WW_CAR_VEHICLES] = {
    "PANDA.SP", "CAMEL.SP", "MOOSE.SP", "PELICAN.SP",
    "SHARK.SP", "RINGO.SP", "ELE.SP", "TIGER.SP"
};

/* sub_328D4 lays out 12 fixed frames followed by the per-vehicle recovery
 * and finish counts in word_73C98/word_73C9A. */
static const uint8_t ww_race_vehicle_frame_count[WW_CAR_VEHICLES] = {
    18, 20, 24, 24, 16, 20, 16, 17
};

/* sub_33842 installs these descriptors and the normal sub_348F4 path calls
 * sub_28038(0) after the intro, replacing the temporary 251X6.INF bank that
 * sub_28180 used during global startup. */
static const char *const ww_race_scale_assets[WW_RACE_SCALE_SETS] = {
    "38X28.INF", "28X28.INF", "14X28.INF",
    "32X24.INF", "15X13.INF", "18X13.INF"
};

typedef struct WwRaceCollisionContext {
    WwRace *race;
} WwRaceCollisionContext;

enum {
    /* sub_266A8 changes the start-light source at 0x11 and releases the
     * race at 0x22 iterations of dword_7EC64. */
    WW_RACE_START_SECOND_FRAME = 0x11,
    WW_RACE_START_RELEASE_FRAME = 0x22
};

static unsigned ww_race_manhattan(uint16_t ax, uint16_t ay,
                                  uint16_t bx, uint16_t by)
{
    int dx = (int)(int16_t)ax - (int)(int16_t)bx;
    int dy = (int)(int16_t)ay - (int)(int16_t)by;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (unsigned)(dx + dy);
}

/* Static and racer portions of sub_216A0.  Terrain is sampled in physics
 * immediately before this callback, preserving the assembly's priority. */
static uint16_t ww_race_player_collision_probe(void *opaque,
                                               uint16_t world_x,
                                               uint16_t world_y)
{
    WwRaceCollisionContext *context = (WwRaceCollisionContext *)opaque;
    WwRace *race;
    size_t i;
    if (context == NULL || context->race == NULL) return 0u;
    race = context->race;
    for (i = 0; i < race->track.spawn_record_count; ++i) {
        WwSpawnRecord *spawn = &race->track.spawn_records[i];
        if (spawn->state != -1 &&
            ww_race_manhattan(world_x, world_y,
                              (uint16_t)spawn->world_x,
                              (uint16_t)spawn->world_y) < 0x0eu) {
            spawn->state = 1;
            race->player_collision_spawn = (int16_t)i;
            return 2u;
        }
    }
    for (i = 1; i < WW_RACER_COUNT; ++i) {
        if (race->player_collision_ignore_ticks != 0u &&
            race->player_collision_ignore_racer == (int16_t)i) {
            continue;
        }
        if (race->racers[i].active &&
            ww_race_manhattan(world_x, world_y,
                              race->racers[i].world_x,
                              race->racers[i].world_y) < 0x0eu) {
            race->player_collision_racer = (int16_t)i;
            /* sub_216A0 writes one to the collided racer's +2 field before
             * returning collision class three. */
            race->racers[i].collision_state = 1u;
            return 3u;
        }
    }
    return 0u;
}

/* sub_22BF4 compares the horizontal centers of a collided queue item and
 * the fixed player item.  The inclusive tolerance is exactly +/-4 pixels. */
static bool ww_race_projected_center_overlap(int16_t x, uint16_t width)
{
    int center = (int)x + (int)width / 2 - 1;
    return center >= 0x9b && center <= 0xa3;
}

static bool ww_race_project_spawn_collision(const WwRace *race,
                                             const WwRenderer *renderer,
                                             size_t spawn_index,
                                             int16_t *screen_y)
{
    const WwSpawnRecord *spawn;
    const WwWorldSpriteDescriptor *descriptor;
    WwPerspectiveProjection projection;
    const WwSpriteScale *scale;
    int type;
    int16_t x;
    if (race == NULL || renderer == NULL || screen_y == NULL ||
        spawn_index >= race->track.spawn_record_count) {
        return false;
    }
    spawn = &race->track.spawn_records[spawn_index];
    type = spawn->sprite_type;
    if (type < 0 || type >= WW_WORLD_SPRITE_DESCRIPTORS) return false;
    descriptor = &race->world_sprites.descriptor[type];
    if (descriptor->scale_bank >= WW_RACE_SCALE_SETS ||
        !ww_perspective_project(renderer, (uint16_t)spawn->world_x,
                                (uint16_t)spawn->world_y,
                                race->player.camera_x, race->player.camera_y,
                                race->player.heading, &projection)) {
        return false;
    }
    scale = &race->perspective_scale[descriptor->scale_bank]
                 .level[projection.scale_level];
    x = (int16_t)(projection.center_x - (int)scale->width / 2);
    *screen_y = (int16_t)(
        (int)renderer->projection_right[projection.distance] -
        (int)scale->height);
    return ww_race_projected_center_overlap(x, scale->width);
}

static bool ww_race_project_racer_collision(const WwRace *race,
                                             const WwRenderer *renderer,
                                             unsigned racer_index,
                                             int16_t *screen_y)
{
    WwProjectedRacer projected;
    if (race == NULL || renderer == NULL || screen_y == NULL ||
        racer_index == 0u || racer_index >= WW_RACER_COUNT ||
        !ww_racer_project(renderer, &race->perspective_scale[0],
                          &race->racers[racer_index], racer_index,
                          race->player.camera_x, race->player.camera_y,
                          race->player.heading, &projected) ||
        !ww_race_projected_center_overlap(projected.x, projected.width)) {
        return false;
    }
    /* loc_234D4 admits only the 0/1/7 relative-direction entries from the
     * table populated by sub_287EC.  Same-direction rear contacts are frame
     * four and must not kill the player. */
    if (projected.direction_frame != 0u &&
        projected.direction_frame != 1u &&
        projected.direction_frame != 7u) {
        return false;
    }
    *screen_y = projected.y;
    return true;
}

static void ww_race_begin_player_crash(WwRace *race, int16_t screen_y)
{
    ww_racer_player_begin_crash(&race->racers[0], screen_y);
    race->player.speed_index = 0u;
    race->player.velocity = (int16_t)ww_read_le16(race->velocity_table);
}

/* The class-three branch of sub_237E4 samples 0x7c units along headings
 * +0x12 and -0x12 after its 0x20-unit reverse.  The right sample is handled
 * first, so it owns the spark when both sides touch; the heading corrections
 * then cancel when both samples are blocked. */
static bool ww_race_apply_terrain_scrape(WwRace *race,
                                         const WwRenderer *renderer)
{
    WwTrackSurfaceSample sample;
    uint16_t sample_x;
    uint16_t sample_y;
    uint16_t right_heading;
    uint16_t left_heading;
    bool right_blocked;
    bool left_blocked;
    if (race == NULL || renderer == NULL) return false;
    if (!race->player.terrain_collision) return true;

    right_heading = (uint16_t)(race->player.heading + 0x12u);
    if (right_heading >= WW_PHYSICS_ANGLE_COUNT) {
        right_heading = (uint16_t)(right_heading - WW_PHYSICS_ANGLE_COUNT);
    }
    left_heading = race->player.heading < 0x12u
                       ? (uint16_t)(race->player.heading +
                                    WW_PHYSICS_ANGLE_COUNT - 0x12u)
                       : (uint16_t)(race->player.heading - 0x12u);

    if (!ww_physics_offset_point(renderer->trig_data, WW_TRIG_BYTES,
                                 race->player.camera_x,
                                 race->player.camera_y, right_heading, 0x7c,
                                 &sample_x, &sample_y) ||
        !ww_track_surface_sample(&race->track, sample_x, sample_y, &sample)) {
        return false;
    }
    right_blocked = sample.sub_37afc_value == 3u;
    if (!ww_physics_offset_point(renderer->trig_data, WW_TRIG_BYTES,
                                 race->player.camera_x,
                                 race->player.camera_y, left_heading, 0x7c,
                                 &sample_x, &sample_y) ||
        !ww_track_surface_sample(&race->track, sample_x, sample_y, &sample)) {
        return false;
    }
    left_blocked = sample.sub_37afc_value == 3u;

    if (right_blocked) {
        if (race->racers[0].spark_position == 0u) {
            race->racers[0].spark_position = 4u;
            race->racers[0].spark_frame = 0u;
        }
        race->player.heading = race->player.heading < 0x12u
                                   ? (uint16_t)(race->player.heading +
                                                WW_PHYSICS_ANGLE_COUNT -
                                                0x12u)
                                   : (uint16_t)(race->player.heading - 0x12u);
    }
    if (left_blocked) {
        if (race->racers[0].spark_position == 0u) {
            race->racers[0].spark_position = 3u;
            race->racers[0].spark_frame = 0u;
        }
        race->player.heading = (uint16_t)(race->player.heading + 0x12u);
        if (race->player.heading >= WW_PHYSICS_ANGLE_COUNT) {
            race->player.heading = (uint16_t)(race->player.heading -
                                               WW_PHYSICS_ANGLE_COUNT);
        }
    }
    return true;
}

static uint16_t ww_race_clamp_crash_coordinate(uint16_t value)
{
    if (value < 0x400u) return 0x400u;
    if (value > 0xbfeu) return 0xbfeu;
    return value;
}

/* word_88C42 state one in sub_290A0.  The camera retreats 0x40 along the
 * inverse heading while the player record remains anchored 0x7c ahead. */
static bool ww_race_update_crash_motion(WwRace *race,
                                        const WwRenderer *renderer)
{
    uint16_t reverse_heading;
    uint16_t camera_x;
    uint16_t camera_y;
    if (race == NULL || renderer == NULL) return false;
    race->player.terrain_collision = false;
    race->player.collision_status = 0u;
    race->player.surface_class = 0u;
    race->player.jump_active = false;
    if (race->racers[0].crash_state != WW_RACER_CRASH_RISING) return true;

    reverse_heading = (uint16_t)(race->player.heading +
                                 WW_PHYSICS_ANGLE_COUNT / 2u);
    if (reverse_heading >= WW_PHYSICS_ANGLE_COUNT) {
        reverse_heading = (uint16_t)(reverse_heading -
                                     WW_PHYSICS_ANGLE_COUNT);
    }
    if (!ww_physics_offset_point(renderer->trig_data, WW_TRIG_BYTES,
                                 race->player.camera_x,
                                 race->player.camera_y, reverse_heading,
                                 0x40u, &camera_x, &camera_y)) {
        return false;
    }
    race->player.camera_x = ww_race_clamp_crash_coordinate(camera_x);
    race->player.camera_y = ww_race_clamp_crash_coordinate(camera_y);
    if (!ww_physics_player_anchor(
            &race->player, renderer->trig_data, WW_TRIG_BYTES,
            &race->racers[0].world_x, &race->racers[0].world_y)) {
        return false;
    }
    race->racers[0].heading = race->player.heading;
    return true;
}

/* loc_3006A is re-entered after a nonfatal crash.  It preserves +6Ch
 * (lives) and the race clock, while clearing the racer/transient records,
 * re-running sub_16F90's grid setup, and restarting every .RD path. */
static bool ww_race_restart_after_crash(WwRace *race)
{
    unsigned i;
    if (race == NULL || race->velocity_table == NULL) return false;

    memset(&race->player, 0, sizeof(race->player));
    race->player.camera_x = race->initial.camera_x;
    race->player.camera_y = race->initial.camera_y;
    race->player.heading = race->initial.heading;
    race->player.velocity = (int16_t)ww_read_le16(race->velocity_table);
    ww_racer_initialize_grid(race->racers, race->initial.grid_x,
                             race->initial.grid_y, race->initial.heading,
                             race->selected_vehicle);
    memset(race->opponent_path, 0, sizeof(race->opponent_path));
    for (i = 1; i < WW_RACER_COUNT; ++i) {
        if (!ww_ai_racer_path_begin(
                &race->opponent_path[i], &race->track, i,
                race->racers[i].world_x, race->racers[i].world_y, 0x54u,
                race->velocity_table, race->velocity_table_size)) {
            return false;
        }
    }
    for (i = 0; i < WW_RACER_COUNT; ++i) {
        ww_lap_state_reset(&race->racer_lap[i]);
        race->racer_rank[i] = 8u;
    }
    ww_dynamic_object_clear(&race->dynamic_objects);
    ww_weapon_reset(&race->player_weapon);
    if (race->duck.active) race->player_weapon.ammunition = 0x63u;
    ww_water_state_reset(&race->water);
    ww_hud_lap_alert_reset(&race->lap_alert);
    ww_finish_reset(&race->finish);
    race->player_collision_spawn = -1;
    race->player_collision_racer = -1;
    race->player_collision_ignore_racer = -1;
    race->player_collision_ignore_ticks = 0u;
    race->next_finish_place = 0u;
    race->horizon_source_offset = 0x50u;
    race->horizon_left_step = 0u;
    race->horizon_right_step = 0u;
    race->steering_frame = 2u;
    race->update_tick = 0u;
    race->displayed_speed = 0;
    race->start_sequence_frame = race->duck.active
                                     ? WW_RACE_START_RELEASE_FRAME : 0u;
    race->start_light_frame = race->duck.active ? 2u : 0u;
    race->start_released = race->duck.active;
    race->start_sound_pending = false;
    race->last_lap_sound_pending = false;
    race->wrong_way_blink = false;
    return true;
}

bool ww_race_selection_init(WwRaceSelection *selection, bool bonus_pack,
                            unsigned board_page, unsigned race_index,
                            unsigned engine_type)
{
    unsigned group;
    if (selection == NULL || board_page >= WW_RACE_BOARD_PAGES ||
        race_index >= WW_RACES_PER_BOARD ||
        (engine_type != 1u && engine_type != 2u)) {
        return false;
    }

    group = board_page + (bonus_pack ? 3u : 0u);
    memset(selection, 0, sizeof(*selection));
    selection->board_page = (uint8_t)board_page;
    selection->race_index = (uint8_t)race_index;
    selection->engine_type = (uint8_t)engine_type;
    /* sub_17928 maps the registered bonus boards to assets 22..36.  Assets
     * 16..21 belong to other game modes and are not the bonus campaign. */
    selection->track_number = (uint8_t)(
        (bonus_pack ? 21u : 0u) + board_page * WW_RACES_PER_BOARD +
        race_index + 1u);
    selection->bonus_pack = bonus_pack;
    return true;
}

bool ww_race_selection_title(const WwRaceSelection *selection,
                             char *title, size_t title_size)
{
    unsigned group;
    int written;
    if (selection == NULL || title == NULL || title_size == 0 ||
        selection->board_page >= WW_RACE_BOARD_PAGES ||
        selection->race_index >= WW_RACES_PER_BOARD ||
        (selection->engine_type != 1u && selection->engine_type != 2u)) {
        return false;
    }
    group = selection->board_page + (selection->bonus_pack ? 3u : 0u);
    written = snprintf(title, title_size, "%s%u. %s",
                       ww_race_group_titles[group],
                       (unsigned)selection->race_index + 1u,
                       selection->engine_type == 1u ? "12HP" : "6HP");
    return written >= 0 && (size_t)written < title_size;
}

bool ww_race_selection_base_name(const WwRaceSelection *selection,
                                 char *base_name, size_t base_name_size)
{
    int written;
    if (selection == NULL || base_name == NULL || base_name_size == 0 ||
        selection->track_number < WW_RACE_FIRST_ASSET_TRACK ||
        selection->track_number > WW_RACE_LAST_ASSET_TRACK ||
        (selection->track_number >= 16u &&
         selection->track_number <= 21u)) {
        return false;
    }
    written = snprintf(base_name, base_name_size, "%u",
                       (unsigned)selection->track_number);
    return written >= 0 && (size_t)written < base_name_size;
}

const char *ww_race_marker_asset(const WwRaceSelection *selection)
{
    unsigned group;
    if (selection == NULL || selection->board_page >= WW_RACE_BOARD_PAGES) {
        return NULL;
    }
    group = selection->board_page + (selection->bonus_pack ? 3u : 0u);
    return ww_race_marker_assets[group];
}

/* Single-player start-grid branch of sub_16F90 followed by the camera values
 * established in sub_2FD8C after its introductory pan completes. */
bool ww_race_initial_state(const WwTrack *track, WwRaceInitialState *state)
{
    static const unsigned grid_order[WW_RACE_GRID_ENTRIES] = {
        4, 3, 2, 1, 0, 7, 6, 5
    };
    uint16_t base_x;
    uint16_t base_y;
    unsigned i;
    if (track == NULL || state == NULL) {
        return false;
    }
    memset(state, 0, sizeof(*state));
    base_x = (uint16_t)track->game.parameter_pair[0][0];
    base_y = (uint16_t)track->game.parameter_pair[0][1];
    for (i = 0; i < WW_RACE_GRID_ENTRIES; ++i) {
        unsigned index = grid_order[i];
        state->grid_x[index] = (uint16_t)(base_x + 80u - i * 20u);
        state->grid_y[index] = base_y;
    }
    /* sub_2FD8C stores grid X directly in word_88B0A.  Its following +0x28
     * writes word_88C4A, not the live camera coordinate. */
    state->camera_x = state->grid_x[0];
    state->camera_y = (uint16_t)(state->grid_y[0] - 0x7cu);
    state->heading = WW_RACE_INITIAL_HEADING;
    return true;
}

bool ww_race_open(WwRace *race, const WwArchive *archive,
                  const WwRaceSelection *selection,
                  unsigned selected_vehicle, unsigned lap_count,
                  unsigned race_class,
                  bool clock_display, bool speedometer,
                  bool overhead_map)
{
    WwRace loaded;
    WwArchiveView view;
    char base_name[8];
    unsigned scale_index;
    if (race == NULL || archive == NULL || selection == NULL ||
        selected_vehicle >= 8u || lap_count == 0 || lap_count > UINT8_MAX ||
        race_class < 1u || race_class > 5u ||
        !ww_race_selection_base_name(selection, base_name, sizeof(base_name))) {
        return false;
    }
    memset(&loaded, 0, sizeof(loaded));
    loaded.selection = *selection;
    loaded.selected_vehicle = (uint8_t)selected_vehicle;
    loaded.lap_count = (uint8_t)lap_count;
    /* sub_19EAC installs amateur behavior for kid mode before entering the
     * race; the remaining class values are retained verbatim. */
    loaded.race_class = (uint8_t)(race_class == 5u ? 1u : race_class);
    /* sub_2FD8C initializes racer +6Ch to three before the first entry at
     * loc_3006A.  Crash restarts deliberately do not overwrite it. */
    loaded.lives = 3u;
    loaded.clock_display = clock_display;
    loaded.speedometer = speedometer;
    if (!ww_track_load(&loaded.track, archive, base_name) ||
        !ww_race_initial_state(&loaded.track, &loaded.initial)) {
        goto failed;
    }

    if (!ww_minimap_load(&loaded.minimap, archive,
                         ww_race_marker_asset(selection),
                         selection->race_index, overhead_map)) {
        goto failed;
    }
    if (!ww_archive_view(archive, ww_race_marker_asset(selection), &view) ||
        view.size != WW_RACE_MARKER_BYTES) {
        goto failed;
    }
    loaded.marker_sprites = view.data;
    loaded.marker_size = view.size;
    if (!ww_archive_view(archive,
                         selection->engine_type == 2u ? "VEL2.TAB"
                                                      : "VEL.TAB",
                         &view) ||
        view.size != WW_PHYSICS_VELOCITY_BYTES) {
        goto failed;
    }
    loaded.velocity_table = view.data;
    loaded.velocity_table_size = view.size;
    if (!ww_archive_view(archive, "CARS.SP", &view) ||
        view.size != WW_CAR_BYTES) {
        goto failed;
    }
    loaded.car_sprites = view.data;
    loaded.car_sprites_size = view.size;
    for (scale_index = 0; scale_index < WW_RACE_SCALE_SETS; ++scale_index) {
        if (!ww_sprite_scale_set_load(
                &loaded.perspective_scale[scale_index], archive,
                ww_race_scale_assets[scale_index])) {
            goto failed;
        }
    }
    if (!ww_world_sprite_catalog_load(&loaded.world_sprites, archive) ||
        !ww_dynamic_sprite_assets_load(&loaded.dynamic_sprites, archive) ||
        !ww_water_assets_load(&loaded.water_assets, archive) ||
        !ww_hud_assets_load(&loaded.hud, archive)) {
        goto failed;
    }
    ww_dynamic_object_clear(&loaded.dynamic_objects);
    ww_weapon_reset(&loaded.player_weapon);
    ww_water_state_reset(&loaded.water);
    ww_hud_lap_alert_reset(&loaded.lap_alert);
    ww_finish_reset(&loaded.finish);
    loaded.player_collision_spawn = -1;
    loaded.player_collision_racer = -1;
    loaded.player_collision_ignore_racer = -1;
    if (!ww_archive_view(archive, ww_race_vehicle_assets[selected_vehicle],
                         &view) ||
        view.size != (size_t)ww_race_vehicle_frame_count[selected_vehicle] *
                         WW_CAR_SOURCE_BYTES) {
        goto failed;
    }
    loaded.vehicle_sprites = view.data;
    loaded.vehicle_sprites_size = view.size;
    loaded.player.camera_x = loaded.initial.camera_x;
    loaded.player.camera_y = loaded.initial.camera_y;
    loaded.player.heading = loaded.initial.heading;
    loaded.player.speed_index = 0;
    loaded.player.velocity = (int16_t)ww_read_le16(loaded.velocity_table);
    /* main installs A7D00 as the panorama base and A7D50 as the initial
     * dword_7D860 window. */
    loaded.horizon_source_offset = 0x50u;
    loaded.steering_frame = 2;
    ww_racer_initialize_grid(loaded.racers, loaded.initial.grid_x,
                             loaded.initial.grid_y, loaded.initial.heading,
                             selected_vehicle);
    for (scale_index = 0; scale_index < WW_RACER_COUNT; ++scale_index) {
        loaded.racer_rank[scale_index] = 8u;
    }
    for (scale_index = 1; scale_index < WW_RACER_COUNT; ++scale_index) {
        if (!ww_ai_racer_path_begin(
                &loaded.opponent_path[scale_index], &loaded.track, scale_index,
                loaded.racers[scale_index].world_x,
                loaded.racers[scale_index].world_y, 0x54u,
                loaded.velocity_table, loaded.velocity_table_size)) {
            ww_error("AI path setup failed for track %s racer %u",
                     base_name, scale_index);
            goto failed;
        }
    }
    loaded.open = true;
    *race = loaded;
    return true;

failed:
    ww_race_close(&loaded);
    return false;
}

bool ww_race_open_duck(WwRace *race, const WwArchive *archive,
                       const WwRaceSelection *selection,
                       unsigned selected_vehicle,
                       bool speedometer, bool overhead_map)
{
    if (selection == NULL ||
        selection->track_number < WW_DUCK_TRACK_FIRST ||
        selection->track_number > WW_DUCK_TRACK_LAST ||
        !ww_race_open(race, archive, selection, selected_vehicle, 6u, 1u,
                      true, speedometer, overhead_map)) {
        return false;
    }
    if (!ww_duck_open(&race->duck, archive, selected_vehicle)) {
        ww_race_close(race);
        return false;
    }
    /* dword_7EC74 enters loc_30169 with word_88C44 already at two: Duck
     * Shoot starts live and never displays the race countdown object. */
    race->start_released = true;
    race->start_sequence_frame = WW_RACE_START_RELEASE_FRAME;
    race->start_light_frame = 2u;
    race->player_weapon.selected_type = 0u;
    race->player_weapon.ammunition = 0x63u;
    return true;
}

bool ww_race_update(WwRace *race, const WwRenderer *renderer,
                    const WwInput *input, WwRoadDetail detail,
                    unsigned elapsed_136_ticks)
{
    WwPlayerControls controls;
    WwRaceCollisionContext collision_context;
    WwFinishPhase previous_finish_phase;
    if (race == NULL || !race->open || renderer == NULL || input == NULL) {
        return false;
    }
    memset(&controls, 0, sizeof(controls));
    controls.steer_left = input->steering < -8192;
    controls.steer_right = input->steering > 8192;
    controls.accelerate = input->throttle > 8192;
    controls.brake = input->throttle < -8192;
    /* word_88C44 gates sub_224EC and sub_28CAC until sub_266A8 has shown
     * the third start-light frame. dword_7EC64 advances once per 12/136-Hz
     * race iteration, so these thresholds are race-frame counts. The DOS
     * race clock and lap update are held by the same gate. */
    if (!race->start_released) {
        ++race->start_sequence_frame;
        if (race->start_sequence_frame >= WW_RACE_START_RELEASE_FRAME) {
            race->start_light_frame = 2u;
            race->start_released = true;
            race->start_sound_pending = true;
        } else if (race->start_sequence_frame >=
                   WW_RACE_START_SECOND_FRAME) {
            race->start_light_frame = 1u;
            return true;
        } else {
            return true;
        }
    }
    if (race->finish.phase != WW_FINISH_RACING ||
        race->racers[0].crash_state != WW_RACER_CRASH_NONE) {
        /* loc_3169D clears the player's control words when the finish state
         * starts.  Coasting physics continues, but fresh input is ignored. */
        memset(&controls, 0, sizeof(controls));
    }
    race->player_collision_spawn = -1;
    race->player_collision_racer = -1;
    if (race->player_collision_ignore_ticks != 0u) {
        --race->player_collision_ignore_ticks;
        if (race->player_collision_ignore_ticks == 0u) {
            race->player_collision_ignore_racer = -1;
        }
    }
    if (race->racers[0].crash_state == WW_RACER_CRASH_NONE) {
        race->player.jump_active = race->racers[0].jump_state != 0u;
        if (race->racers[0].jump_state != 0u) {
            if (race->racers[0].twirl_active) {
                ++race->racers[0].twirl_frame;
                if (race->racers[0].twirl_frame >= 8u) {
                    race->racers[0].twirl_frame = 0u;
                    race->racers[0].twirl_active = false;
                }
            }
            if (race->racers[0].jump_countdown != 0u) {
                --race->racers[0].jump_countdown;
            }
            if (race->racers[0].jump_countdown <= 4u &&
                race->racers[0].jump_state == 1u) {
                race->racers[0].jump_state = 2u;
            }
        }
        collision_context.race = race;
        if (!ww_physics_player_step(
                &race->player, controls, &race->track,
                renderer->trig_data, WW_TRIG_BYTES,
                renderer->ndist_data, WW_NDIST_BYTES, race->velocity_table,
                race->velocity_table_size, (unsigned)detail,
                ww_race_player_collision_probe, &collision_context)) {
            return false;
        }
        if (!ww_race_apply_terrain_scrape(race, renderer)) return false;
        ww_water_update(
            &race->water, race->player.surface_class,
            race->racers[0].jump_state == 0u &&
                race->racers[0].crash_state == WW_RACER_CRASH_NONE);
        if (race->player.collision_status == 4u &&
            race->racers[0].jump_state == 0u) {
            race->racers[0].jump_state = 1u;
            race->racers[0].jump_height = 8u;
            race->racers[0].jump_countdown = 8u;
            race->racers[0].twirl_frame = 0u;
            race->racers[0].twirl_active = true;
            race->player.jump_active = true;
        }
        if (!ww_physics_player_anchor(
                &race->player, renderer->trig_data, WW_TRIG_BYTES,
                &race->racers[0].world_x, &race->racers[0].world_y)) {
            return false;
        }
        race->racers[0].heading = race->player.heading;

        /* sub_216A0 only marks a world-space contact.  sub_22BF4 later
         * decides whether it is a crash from the projected queue item. */
        if (race->player.speed_index >= 0x64u &&
            race->racers[0].jump_state == 0u &&
            race->finish.phase == WW_FINISH_RACING) {
            int16_t crash_y = (int16_t)(
                (int)renderer->projection_right[WW_RACER_PLAYER_DISTANCE] -
                (int)race->perspective_scale[0].level[0].height);
            bool crash = false;
            if (race->player_collision_spawn >= 0) {
                size_t collided = (size_t)race->player_collision_spawn;
                int type = race->track.spawn_records[collided].sprite_type;
                if (type < 0 || type >= WW_WORLD_SPRITE_DESCRIPTORS) {
                    return false;
                }
                /* loc_2334D is the +24h == -1 static-object path and only
                 * non-positive SPRITE.ATR classifications reach it. */
                if (race->world_sprites.descriptor[type].classification <= 0) {
                    crash = ww_race_project_spawn_collision(
                        race, renderer, collided, &crash_y);
                }
            } else if (race->player_collision_racer >= 0) {
                unsigned collided = (unsigned)race->player_collision_racer;
                crash = ww_race_project_racer_collision(
                    race, renderer, collided, &crash_y);
            }
            if (crash) {
                ww_race_begin_player_crash(race, crash_y);
            } else if (race->player_collision_racer >= 0) {
                /* Native contact marks the other racer, draws the side-hit
                 * effect, and lets steering separate the pair.  Keeping the
                 * same world-space probe solid forever deadlocks two cars in
                 * the fixed-step port, so admit that marked racer for the
                 * short separation interval. */
                race->player_collision_ignore_racer =
                    race->player_collision_racer;
                race->player_collision_ignore_ticks = 6u;
                race->racers[(unsigned)race->player_collision_racer]
                    .collision_state = 0u;
            }
        }
        if (race->player_collision_racer >= 0 &&
            race->racers[0].crash_state == WW_RACER_CRASH_NONE &&
            race->racers[0].jump_state == 0u) {
            race->player_collision_ignore_racer =
                race->player_collision_racer;
            race->player_collision_ignore_ticks = 6u;
            race->racers[(unsigned)race->player_collision_racer]
                .collision_state = 0u;
        }
        if (race->player_collision_spawn >= 0 &&
            race->racers[0].crash_state == WW_RACER_CRASH_NONE) {
            WwSpawnRecord *spawn = &race->track.spawn_records[
                (size_t)race->player_collision_spawn];
            int type = spawn->sprite_type;
            bool consumed;
            if (type < 0 || type >= WW_WORLD_SPRITE_DESCRIPTORS ||
                !ww_weapon_collect(
                    &race->player_weapon,
                    race->world_sprites.descriptor[type].classification,
                    &consumed)) {
                return false;
            }
            if (consumed) spawn->state = -1;
            else if (race->player.collision_status == 2u) {
                uint16_t reverse_heading = (uint16_t)(
                    race->player.heading + WW_PHYSICS_ANGLE_COUNT / 2u);
                uint16_t separated_x;
                uint16_t separated_y;
                int reduced_speed;
                if (reverse_heading >= WW_PHYSICS_ANGLE_COUNT) {
                    reverse_heading = (uint16_t)(reverse_heading -
                                                 WW_PHYSICS_ANGLE_COUNT);
                }
                /* The DOS line cursor never admits the occupied point.  A
                 * short reverse separation recreates that clearance when a
                 * fixed update begins already inside the 14-unit pillar
                 * radius, then leaves steering free to escape. */
                if (!ww_physics_offset_point(
                        renderer->trig_data, WW_TRIG_BYTES,
                        race->player.camera_x, race->player.camera_y,
                        reverse_heading, 0x10, &separated_x, &separated_y)) {
                    return false;
                }
                race->player.camera_x = separated_x;
                race->player.camera_y = separated_y;
                reduced_speed = (int)race->player.speed_index - 0x10;
                if (reduced_speed < 0) reduced_speed = 0;
                race->player.speed_index = (uint16_t)reduced_speed;
                race->player.velocity = (int16_t)ww_read_le16(
                    race->velocity_table + (size_t)reduced_speed * 2u);
                if (!ww_physics_player_anchor(
                        &race->player, renderer->trig_data, WW_TRIG_BYTES,
                        &race->racers[0].world_x,
                        &race->racers[0].world_y)) {
                    return false;
                }
                race->racers[0].heading = race->player.heading;
            }
        }
    } else if (!ww_race_update_crash_motion(race, renderer)) {
        return false;
    }
    {
        unsigned racer_index;
        for (racer_index = 1; racer_index < WW_RACER_COUNT; ++racer_index) {
            if (race->duck.active) {
                if (race->racers[racer_index].hit_effect != 0u) {
                    if (!ww_duck_target_hit_step(
                            &race->duck, racer_index, race->racers,
                            race->opponent_path, &race->track)) {
                        return false;
                    }
                    continue;
                }
            } else if (ww_racer_hit_effect_step(
                           &race->racers[racer_index])) {
                continue;
            }
            if (!ww_ai_racer_path_step(
                    &race->opponent_path[racer_index], &race->track,
                    &race->racers[racer_index].world_x,
                    &race->racers[racer_index].world_y,
                    &race->racers[racer_index].heading)) {
                return false;
            }
        }
    }
    ++race->update_tick;
    race->elapsed_136_ticks += elapsed_136_ticks;
    /* main registers dword_7E71C as a 10 Hz TaskMan counter.  One normal
     * race update follows the 12-tick/136-Hz loc_31111 wait, so retain the
     * fractional conversion instead of advancing once per rendered frame. */
    race->race_time_136_remainder = (uint16_t)(
        race->race_time_136_remainder + elapsed_136_ticks * 10u);
    race->race_time_tenths += race->race_time_136_remainder /
                              WW_GAME_TICK_HZ;
    race->race_time_136_remainder %= WW_GAME_TICK_HZ;
    if (race->race_time_tenths > 0x8ca0u) race->race_time_tenths = 0u;

    /* loc_314A2 waits 0x0d task ticks after state four, consumes +6Ch,
     * exits at zero, and otherwise jumps directly back to loc_3006A. */
    if (race->racers[0].crash_state == WW_RACER_CRASH_COMPLETE &&
        race->elapsed_136_ticks - race->racers[0].crash_phase_tick >=
            WW_RACER_CRASH_RESTART_TICKS) {
        if (!race->duck.active && race->lives != 0u) --race->lives;
        if (race->lives == 0u) {
            ww_race_close(race);
            return true;
        }
        if (!ww_race_restart_after_crash(race)) return false;
    }
    if (!race->duck.active &&
        race->racers[0].crash_state == WW_RACER_CRASH_NONE) {
        if (!ww_lap_update_player(
                &race->racer_lap[0], &race->track,
                race->racers[0].world_x, race->racers[0].world_y,
                race->lap_count, race->player.terrain_collision,
                race->update_tick, &race->next_finish_place)) {
            return false;
        }
    }
    if (!race->duck.active) {
        unsigned racer_index;
        for (racer_index = 1; racer_index < WW_RACER_COUNT; ++racer_index) {
            if (!ww_lap_update_opponent(
                    &race->racer_lap[racer_index], &race->track,
                    race->racers[racer_index].world_x,
                    race->racers[racer_index].world_y, race->lap_count,
                    &race->next_finish_place)) {
                return false;
            }
        }
    }
    if (!race->duck.active && race->racer_lap[0].last_lap_alert_pending) {
        race->racer_lap[0].last_lap_alert_pending = false;
        ww_hud_lap_alert_begin(&race->lap_alert);
        race->last_lap_sound_pending = true;
    }
    if (race->duck.active) {
        ww_duck_update_timer(&race->duck, race->race_time_tenths);
        if (!ww_weapon_update_fire(
                &race->player_weapon,
                !race->duck.finished &&
                    race->racers[0].crash_state == WW_RACER_CRASH_NONE &&
                    input->fire,
                &race->dynamic_objects, renderer, &race->racers[0])) {
            return false;
        }
        /* The native mode is an uninterrupted shooting session; retain the
         * ordinary hedgehog shot selected by sub_24364 for the full timer. */
        race->player_weapon.selected_type = 0u;
        race->player_weapon.ammunition = 0x63u;
    } else {
        ww_lap_update_ranks(race->racer_lap, race->racer_rank,
                            WW_RACER_COUNT);
        previous_finish_phase = race->finish.phase;
        if (!ww_finish_update(&race->finish, race->racer_lap,
                              race->race_class, race->next_finish_place,
                              race->race_time_tenths,
                              race->elapsed_136_ticks) ||
            !ww_weapon_update_fire(
                &race->player_weapon,
                race->finish.phase == WW_FINISH_RACING &&
                    race->racers[0].crash_state == WW_RACER_CRASH_NONE &&
                    input->fire,
                &race->dynamic_objects, renderer, &race->racers[0])) {
            return false;
        }
        if (previous_finish_phase == WW_FINISH_RACING &&
            race->finish.phase != WW_FINISH_RACING) {
            race->racers[0].finish_frame = 0u;
        } else if (race->finish.phase != WW_FINISH_RACING) {
            ww_racer_player_finish_animation_step(
                &race->racers[0], race->racer_lap[0].finish_place);
        }
    }
    /* sub_24834 scrolls dword_7D860 by the two steering accumulators at
     * racer offsets +52h/+54h.  The normal branch ramps each from one to
     * four.  Its boundary jumps preserve the wrapped visual position while
     * keeping the 0x50-byte source window inside the four-row panorama. */
    if (controls.steer_left) {
        if (race->horizon_left_step < 4u) ++race->horizon_left_step;
        if (race->horizon_source_offset == 0u) {
            race->horizon_source_offset = 0xa0u;
        }
        if (race->horizon_source_offset < race->horizon_left_step) {
            race->horizon_source_offset = 0u;
        } else {
            race->horizon_source_offset = (uint16_t)(
                race->horizon_source_offset - race->horizon_left_step);
        }
    } else if (race->horizon_left_step != 0u) {
        --race->horizon_left_step;
    }
    if (controls.steer_right) {
        if (race->horizon_right_step < 4u) ++race->horizon_right_step;
        if (race->horizon_source_offset == 0xf0u) {
            race->horizon_source_offset = 0x50u;
        }
        race->horizon_source_offset = (uint16_t)(
            race->horizon_source_offset + race->horizon_right_step);
        if (race->horizon_source_offset > 0xf0u) {
            race->horizon_source_offset = 0xf0u;
        }
    } else if (race->horizon_right_step != 0u) {
        --race->horizon_right_step;
    }
    {
        uint8_t target = controls.steer_right ? 4u
                         : controls.steer_left ? 0u : 2u;
        if (race->steering_frame < target) {
            ++race->steering_frame;
        } else if (race->steering_frame > target) {
            --race->steering_frame;
        }
    }
    return ww_dynamic_object_update(
        &race->dynamic_objects, &race->dynamic_sprites, &race->track,
        renderer, race->racers);
}

void ww_race_close(WwRace *race)
{
    if (race == NULL) {
        return;
    }
    ww_minimap_close(&race->minimap);
    ww_track_close(&race->track);
    memset(race, 0, sizeof(*race));
}

static bool ww_race_enqueue_start_light(WwRace *race,
                                        const WwRenderer *renderer,
                                        WwRenderQueue *queue)
{
    WwPerspectiveProjection projection;
    WwRenderQueueItem item;
    const WwSpriteScale *scale;
    uint16_t world_x;
    uint16_t world_y;
    size_t source_offset;
    if (race != NULL && race->duck.active) return true;
    if (race == NULL || renderer == NULL || queue == NULL ||
        !race->hud.loaded ||
        race->start_light_frame >= WW_HUD_START_LIGHT_FRAMES) {
        return false;
    }
    if (race->start_released &&
        ww_race_manhattan(race->player.camera_x, race->player.camera_y,
                          race->initial.camera_x,
                          race->initial.camera_y) >= 0x100u) {
        return true;
    }
    /* word_88C4A/word_88C4C are the first .GAM parameter pair displaced
     * by (+0x28,-0x0a), exactly as initialized at loc_30513. */
    world_x = (uint16_t)(race->track.game.parameter_pair[0][0] + 0x28);
    world_y = (uint16_t)(race->track.game.parameter_pair[0][1] - 0x0a);
    if (!ww_perspective_project(renderer, world_x, world_y,
                                race->player.camera_x,
                                race->player.camera_y,
                                race->player.heading, &projection)) {
        return true;
    }
    scale = &race->perspective_scale[0].level[projection.scale_level];
    source_offset = WW_HUD_START_LIGHT_OFFSET +
                    (size_t)race->start_light_frame * WW_CAR_SOURCE_BYTES;
    if (source_offset + WW_CAR_SOURCE_BYTES >
        race->hud.general_effects_size) {
        return false;
    }
    memset(&item, 0, sizeof(item));
    item.x = (int16_t)(projection.center_x - (int)scale->width / 2);
    item.y = (int16_t)((int)renderer->projection_right[projection.distance] -
                       (int)scale->height);
    item.distance = projection.distance;
    item.source = race->hud.general_effects + source_offset;
    item.source_size = race->hud.general_effects_size - source_offset;
    item.scale = scale;
    return ww_render_queue_push(queue, &item);
}

static bool ww_race_enqueue_player_view(WwRace *race,
                                        const WwRenderer *renderer,
                                        WwRenderQueue *queue)
{
    if (race->water.submerged) {
        return ww_water_enqueue_periscope(
            &race->water_assets, &race->water, renderer,
            &race->perspective_scale[3], queue);
    }
    return ww_racer_enqueue_player(
        queue, renderer, &race->perspective_scale[0], &race->racers[0],
        race->car_sprites, race->car_sprites_size,
        race->vehicle_sprites, race->vehicle_sprites_size,
        race->steering_frame, race->racer_lap[0].finish_place,
        race->elapsed_136_ticks);
}

/* Live translated frame: the .GAM background PCX, lower sub_37E94 horizon/road,
 * sub_255D4/sub_26A3C racers, sub_25A78 .SPW and sub_2632C dynamic objects,
 * player effects, and the normal single-player HUD/lap status paths. */
bool ww_race_render_bringup(WwRace *race, const WwRenderer *renderer,
                            WwDisplay *display, WwRoadDetail detail)
{
    uint8_t *pixels;
    WwRenderQueue queue;
    unsigned racer_index;
    if (race == NULL || !race->open || renderer == NULL || display == NULL) {
        return false;
    }
    ww_racer_player_jump_render_step(&race->racers[0]);
    ww_display_set_draw_page(display, 0);
    if (!ww_display_blit_pcx(display, &race->track.background)) {
        return false;
    }
    /* sub_279E8 reads the final 0x300 bytes of the first A_ tile atlas into
     * dword_7D718.  That is the live race palette; BACK*.PCX supplies only
     * backdrop indices. */
    ww_display_set_palette(display, race->track.tile_palette);
    pixels = ww_display_draw_pixels(display);
    if (!ww_renderer_render_horizon_split(
            &race->track, pixels, WW_SCREEN_WIDTH, true,
            race->horizon_source_offset) ||
        !ww_renderer_render_road_split(renderer, &race->track, pixels,
                                       WW_SCREEN_WIDTH, true,
                                       race->player.heading,
                                       race->player.camera_x,
                                       race->player.camera_y, detail)) {
        return false;
    }
    ww_render_queue_init(&queue);
    if (!(race->duck.active
              ? ww_duck_enqueue_targets(
                    &race->duck, &queue, renderer,
                    &race->perspective_scale[0], race->racers,
                    race->player.camera_x, race->player.camera_y,
                    race->player.heading, race->car_sprites,
                    race->car_sprites_size)
              : ww_racer_enqueue_opponents(
                    &queue, renderer, &race->perspective_scale[0],
                    race->racers, race->player.camera_x,
                    race->player.camera_y, race->player.heading,
                    race->car_sprites, race->car_sprites_size)) ||
        !ww_race_enqueue_start_light(race, renderer, &queue) ||
        !ww_world_object_enqueue(
            &race->track, &race->world_sprites, race->perspective_scale,
            WW_RACE_SCALE_SETS, renderer, race->player.camera_x,
            race->player.camera_y, race->player.heading, &queue) ||
        !ww_race_enqueue_player_view(race, renderer, &queue) ||
        !ww_dynamic_object_enqueue(
            &race->dynamic_objects, &race->dynamic_sprites,
            &race->perspective_scale[5], renderer, race->player.camera_x,
            race->player.camera_y, race->player.heading, &queue) ||
        !ww_render_queue_draw(&queue, pixels, WW_SCREEN_WIDTH) ||
        ((race->racers[0].jump_state != 0u ||
          race->racers[0].crash_state != WW_RACER_CRASH_NONE)
             ? false
             : !ww_water_draw_shallow_spray(
                   &race->water_assets, &race->water, renderer,
                   &race->perspective_scale[0], display)) ||
        !ww_water_draw_splash(&race->water_assets, &race->water, display) ||
        (!race->water.submerged && !ww_racer_draw_player_spark(
            display, renderer, &race->perspective_scale[0], &race->racers[0],
            race->dynamic_sprites.spark, race->dynamic_sprites.spark_size)) ||
        !ww_hud_draw_race_frame(&race->hud, display)) {
        return false;
    }
    for (racer_index = 0u; !race->duck.active &&
                            racer_index < WW_RACER_COUNT; ++racer_index) {
        unsigned place = race->racer_lap[racer_index].finish_place;
        if (place >= 1u && place <= 3u &&
            !ww_hud_draw_finish_place(
                &race->hud, display, race->car_sprites,
                race->car_sprites_size, race->racers[racer_index].vehicle,
                place)) {
            return false;
        }
    }
    if (!ww_minimap_draw(&race->minimap, display, race->racers) ||
        (race->duck.active &&
         (!ww_hud_draw_duck_score(&race->hud, display, race->duck.score) ||
          !ww_hud_draw_race_time(
              &race->hud, display,
              ww_duck_time_remaining(&race->duck,
                                     race->race_time_tenths), true))) ||
        (!race->duck.active &&
         (
        !ww_hud_draw_wrong_way(
            &race->hud, display, race->racer_lap[0].wrong_way_active,
            &race->wrong_way_blink) ||
        !ww_hud_draw_rank(&race->hud, display, race->racer_rank[0]) ||
        !ww_hud_draw_race_time(&race->hud, display,
                               race->race_time_tenths,
                               race->clock_display) ||
        !ww_hud_draw_speedometer(&race->hud, display, race->player.velocity,
                                 &race->displayed_speed,
                                 race->speedometer) ||
        !ww_hud_draw_ammunition(&race->hud, display,
                                race->player_weapon.ammunition) ||
        !ww_hud_draw_lives(&race->hud, display, race->lives) ||
        !ww_hud_draw_lap_alert(&race->hud, display, &race->lap_alert) ||
        !ww_hud_draw_lap_status(
            &race->hud, display,
            (unsigned)race->racer_lap[0].current_lap,
            race->lap_count,
            race->racer_lap[0].show_lap_status && !race->lap_alert.active,
            race->racer_lap[0].finished))) ||
        !ww_world_object_update(&race->track, &race->world_sprites)) {
        return false;
    }
    if (race->perspective_scale[0].level[0].width != 38u ||
        race->perspective_scale[0].level[0].height != 28u) {
        return false;
    }
    ww_display_set_visible_page(display, 0);
    ww_racer_player_jump_render_finish(&race->racers[0]);
    if (!ww_display_present(display)) return false;
    ww_water_render_step(&race->water, &race->water_assets);
    ww_hud_lap_alert_step(&race->lap_alert, &race->hud);
    return true;
}
