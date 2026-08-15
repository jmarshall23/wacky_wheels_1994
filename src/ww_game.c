#include "ww_game.h"

#include "ww_common.h"
#include "ww_ai.h"
#include "ww_music.h"
#include "ww_pcx.h"
#include "ww_physics.h"
#include "ww_race.h"
#include "ww_track.h"
#include "ww_voc.h"

#include <SDL.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Native replacement for top-level main routine sub_35CA4. */

static void ww_game_stop_water_audio(WwGame *game);

static void ww_game_play_menu_music(WwGame *game)
{
    if (game != NULL) {
        (void)ww_audio_play_midi_asset(&game->audio, &game->archive,
                                       "MAINMENU.MID", true,
                                       game->settings.music_volume);
    }
}

static void ww_game_play_race_music(WwGame *game)
{
    char asset[WW_ARCHIVE_NAME_BYTES + 1];
    if (game == NULL || !game->race.open) return;
    if (ww_music_race_asset(game->race.selection.bonus_pack,
                            game->race.selection.board_page,
                            game->race.selection.race_index % 3u, false,
                            asset, sizeof(asset))) {
        (void)ww_audio_play_midi_asset(&game->audio, &game->archive, asset,
                                       true, game->settings.music_volume);
    }
}

static void ww_game_stop_engine(WwGame *game)
{
    if (game != NULL && game->engine_voice >= 0) {
        ww_audio_stop(&game->audio, game->engine_voice);
        game->engine_voice = -1;
    }
    ww_game_stop_water_audio(game);
}

static void ww_game_start_engine(WwGame *game)
{
    if (game == NULL || game->engine_sound.samples == NULL) return;
    ww_game_stop_engine(game);
    game->engine_voice = ww_audio_play_unscaled(
        &game->audio, &game->engine_sound, game->settings.engine_volume);
    ww_audio_set_pitch_cents(&game->audio, game->engine_voice, -0x708);
}

static void ww_game_update_engine_pitch(WwGame *game)
{
    if (game == NULL || !game->race.open || game->engine_voice < 0) return;
    ww_audio_set_pitch_cents(
        &game->audio, game->engine_voice,
        (int)game->race.player.velocity * 0x14 - 0x708);
}

static void ww_game_stop_water_audio(WwGame *game)
{
    if (game == NULL) return;
    if (game->shallow_water_voice >= 0) {
        ww_audio_stop(&game->audio, game->shallow_water_voice);
        game->shallow_water_voice = -1;
    }
    if (game->water_horn_voice >= 0) {
        ww_audio_stop(&game->audio, game->water_horn_voice);
        game->water_horn_voice = -1;
    }
}

static void ww_game_update_race_effect_audio(WwGame *game)
{
    if (game == NULL || !game->race.open) {
        ww_game_stop_water_audio(game);
        return;
    }
    if (game->race.water.splash_sound_pending) {
        game->race.water.splash_sound_pending = false;
        if (game->splash_sound.samples != NULL) {
            (void)ww_audio_play(&game->audio, &game->splash_sound,
                                game->settings.sound_volume);
        }
    }
    if (game->race.last_lap_sound_pending) {
        game->race.last_lap_sound_pending = false;
        if (game->bell_sound.samples != NULL) {
            (void)ww_audio_play(&game->audio, &game->bell_sound,
                                game->settings.sound_volume);
        }
    }
    if (!game->race.water.shallow_active ||
        game->race.racers[0].jump_state != 0u ||
        game->race.racers[0].crash_state != WW_RACER_CRASH_NONE) {
        if (game->shallow_water_voice >= 0) {
            ww_audio_stop(&game->audio, game->shallow_water_voice);
            game->shallow_water_voice = -1;
        }
    } else if (game->plip_sound.samples != NULL &&
               !ww_audio_voice_active(&game->audio,
                                      game->shallow_water_voice)) {
        /* loc_2513C calls sound 0x10 (PLIP.VOC) whenever the 12x24
         * shallow-water wheel sprite is selected and its prior voice ended. */
        game->shallow_water_voice = ww_audio_play(
            &game->audio, &game->plip_sound, game->settings.sound_volume);
    }
    if (!game->race.water.submerged) {
        ww_game_stop_water_audio(game);
    } else if (game->horn_sound.samples != NULL &&
               !ww_audio_voice_active(&game->audio,
                                      game->water_horn_voice)) {
        /* sub_26A3C restarts HORN.VOC whenever sound 14 is no longer active
         * while the periscope state remains set. */
        game->water_horn_voice = ww_audio_play(
            &game->audio, &game->horn_sound, game->settings.sound_volume);
    }
}

static bool ww_game_advance_after_victory(WwGame *game)
{
    WwRaceSelection next;
    WwVictory completed;
    char title[64];
    if (game == NULL || !ww_victory_has_next_race(&game->victory)) {
        return false;
    }
    completed = game->victory;
    if (!ww_race_selection_init(
            &next, completed.race_selection.bonus_pack,
            completed.race_selection.board_page,
            completed.race_selection.race_index + 1u,
            completed.race_selection.engine_type)) {
        return false;
    }
    ww_victory_close(&game->victory);
    if (!ww_race_open(&game->race, &game->archive, &next,
                      completed.selected_vehicle, completed.lap_count,
                      completed.race_class, completed.clock_display,
                      completed.speedometer, completed.overhead_map)) {
        return false;
    }
    game->race_paused = false;
    game->menu.request_start_race = false;
    ww_game_play_race_music(game);
    ww_game_start_engine(game);
    if (ww_race_selection_title(&game->race.selection, title,
                                sizeof(title))) {
        ww_display_set_title(&game->display, title);
    }
    return true;
}

static bool ww_game_restart_race(WwGame *game)
{
    WwRaceSelection selection;
    unsigned selected_vehicle;
    unsigned lap_count;
    unsigned race_class;
    bool clock_display;
    bool speedometer;
    bool overhead_map;
    bool duck_mode;
    if (game == NULL || !game->race.open) return false;
    selection = game->race.selection;
    selected_vehicle = game->race.selected_vehicle;
    lap_count = game->race.lap_count;
    race_class = game->race.race_class;
    clock_display = game->race.clock_display;
    speedometer = game->race.speedometer;
    overhead_map = game->race.minimap.enabled;
    duck_mode = game->race.duck.active;
    ww_race_close(&game->race);
    if (duck_mode) {
        return ww_race_open_duck(&game->race, &game->archive, &selection,
                                 selected_vehicle, speedometer,
                                 overhead_map);
    }
    return ww_race_open(&game->race, &game->archive, &selection,
                        selected_vehicle, lap_count, race_class,
                        clock_display, speedometer, overhead_map);
}

static bool ww_test_archive(const WwArchive *archive, const WwRenderer *renderer)
{
    unsigned pcx_count = 0;
    unsigned voc_count = 0;
    unsigned midi_count = 0;
    unsigned track_gam_count = 0;
    uint16_t i;
    bool valid = true;

    for (i = 0; i < archive->entry_count; ++i) {
        const WwArchiveEntry *entry = &archive->entries[i];
        WwArchiveView view;
        if (!ww_archive_view_entry(archive, entry, &view)) {
            valid = false;
            continue;
        }
        if (ww_ascii_ends_with(entry->name, ".PCX")) {
            WwPcxImage image;
            ++pcx_count;
            if (!ww_pcx_decode(view.data, view.size, &image)) {
                ww_error("PCX validation failed: %s", entry->name);
                valid = false;
            } else {
                if (image.width != WW_SCREEN_WIDTH || image.height != WW_SCREEN_HEIGHT) {
                    ww_error("unexpected PCX dimensions: %s", entry->name);
                    valid = false;
                }
                ww_pcx_free(&image);
            }
        } else if (ww_ascii_ends_with(entry->name, ".VOC")) {
            WwVocPcm pcm;
            ++voc_count;
            if (!ww_voc_decode(view.data, view.size, &pcm)) {
                ww_error("VOC validation failed: %s", entry->name);
                valid = false;
            } else {
                ww_voc_free(&pcm);
            }
        } else if (ww_ascii_ends_with(entry->name, ".MID")) {
            WwMidiInfo info;
            ++midi_count;
            if (!ww_midi_inspect(view.data, view.size, &info)) {
                ww_error("MIDI validation failed: %s", entry->name);
                valid = false;
            }
        } else if (ww_ascii_ends_with(entry->name, ".GAM")) {
            char base_name[WW_ARCHIVE_NAME_BYTES + 1];
            char *extension;
            WwTrack track;
            ++track_gam_count;
            memset(&track, 0, sizeof(track));
            strncpy(base_name, entry->name, sizeof(base_name) - 1);
            base_name[sizeof(base_name) - 1] = '\0';
            extension = strrchr(base_name, '.');
            if (extension == NULL) {
                valid = false;
                continue;
            }
            *extension = '\0';
            if (!ww_track_load(&track, archive, base_name)) {
                ww_error("track validation failed: %s", entry->name);
                valid = false;
            } else {
                WwAiPathState path;
                WwAiPathState advanced;
                WwRaceInitialState initial;
                uint8_t road_frame[WW_SCREEN_WIDTH * WW_SCREEN_HEIGHT];
                unsigned steps = (unsigned)track.road_segments[0].point_count + 1u;
                memset(road_frame, 0xcd, sizeof(road_frame));
                if (!ww_ai_path_begin(&path, &track, 0) ||
                    !ww_ai_path_copy_advance(&advanced, &path, &track, steps)) {
                    ww_error("AI path validation failed: %s", entry->name);
                    valid = false;
                }
                if (!ww_race_initial_state(&track, &initial) ||
                    initial.grid_x[4] !=
                        (uint16_t)(track.game.parameter_pair[0][0] + 80) ||
                    initial.grid_x[0] !=
                        (uint16_t)track.game.parameter_pair[0][0] ||
                    initial.grid_x[5] !=
                        (uint16_t)(track.game.parameter_pair[0][0] - 60) ||
                    initial.camera_x !=
                        (uint16_t)track.game.parameter_pair[0][0] ||
                    initial.camera_y !=
                        (uint16_t)(track.game.parameter_pair[0][1] - 124) ||
                    initial.heading != WW_RACE_INITIAL_HEADING) {
                    ww_error("race-start validation failed: %s", entry->name);
                    valid = false;
                }
                if (!ww_renderer_render_road_split(
                        renderer, &track, road_frame, WW_SCREEN_WIDTH, false,
                        initial.heading,
                        initial.camera_x, initial.camera_y,
                        WW_ROAD_DETAIL_HIGH)) {
                    ww_error("road-render validation failed: %s", entry->name);
                    valid = false;
                }
                if (!ww_renderer_render_horizon_split(
                        &track, road_frame, WW_SCREEN_WIDTH, false, 0x50u)) {
                    ww_error("horizon-render validation failed: %s", entry->name);
                    valid = false;
                } else if (road_frame[10u * WW_SCREEN_WIDTH] !=
                               track.par_bytes[WW_SCREEN_WIDTH] ||
                           road_frame[10u * WW_SCREEN_WIDTH + 3u] !=
                               track.par_bytes[WW_SCREEN_WIDTH + 3u] ||
                           road_frame[10u * WW_SCREEN_WIDTH + 4u] !=
                               track.par_bytes[WW_SCREEN_WIDTH + 4u] ||
                           road_frame[11u * WW_SCREEN_WIDTH] !=
                               track.par_bytes[5u * WW_SCREEN_WIDTH]) {
                    ww_error("Mode X horizon panorama failed: %s", entry->name);
                    valid = false;
                }
                {
                    WwTrackSurfaceSample surface;
                    if (!ww_track_surface_sample(
                            &track, (uint16_t)track.game.origin[0],
                            (uint16_t)track.game.origin[1], &surface)) {
                        ww_error("surface-bank validation failed: %s", entry->name);
                        valid = false;
                    }
                }
            }
            ww_track_close(&track);
        }
    }

    ww_log("asset validation: %u PCX, %u VOC, %u MIDI, %u track definitions",
           pcx_count, voc_count, midi_count, track_gam_count);
    if (archive->entry_count != WW_ARCHIVE_EXPECTED_ENTRIES || pcx_count != 116 ||
        voc_count != 36 || midi_count != 16 || track_gam_count != 42) {
        ww_error("registered 1.1 archive inventory does not match the expected build");
        valid = false;
    }
    return valid;
}

static void ww_test_set_position(WwTrack *track, unsigned map_x,
                                 unsigned map_y, uint16_t position)
{
    size_t offset = ((size_t)map_y * 64u + map_x) * 2u;
    track->position_map[offset] = (uint8_t)position;
    track->position_map[offset + 1u] = (uint8_t)(position >> 8);
}

static bool ww_test_lap_progression(void)
{
    WwTrack track;
    WwLapState player;
    WwLapState opponent;
    WwLapState rank_state[WW_RACER_COUNT];
    uint8_t rank[WW_RACER_COUNT];
    uint16_t next_finish_place = 0u;
    int16_t position;
    memset(&track, 0, sizeof(track));
    track.position_count = 4;
    ww_test_set_position(&track, 0, 0, 1);
    ww_test_set_position(&track, 1, 0, 2);
    ww_test_set_position(&track, 2, 0, 3);
    ww_test_set_position(&track, 3, 0, 4);
    ww_lap_state_reset(&player);
    ww_lap_state_reset(&opponent);

    if (!ww_track_position_sample(&track, 0x460u, 0x400u, &position) ||
        position != 4 ||
        ww_track_position_sample(&track, 0x3ffu, 0x400u, &position)) {
        return false;
    }
    /* Forward 4->1 awards lap one.  The immediate inverse 1->4->1 is
     * suppressed; a subsequent ordered 3->4->1 awards the last lap. */
    if (!ww_lap_update_player(&player, &track, 0x460u, 0x400u, 2, false, 1,
                              &next_finish_place) ||
        !ww_lap_update_player(&player, &track, 0x400u, 0x400u, 2, false, 2,
                              &next_finish_place) ||
        player.current_lap != 1 || !player.show_lap_status ||
        !ww_lap_update_player(&player, &track, 0x460u, 0x400u, 2, false, 3,
                              &next_finish_place) ||
        !ww_lap_update_player(&player, &track, 0x400u, 0x400u, 2, false, 4,
                              &next_finish_place) ||
        player.current_lap != 1 ||
        !ww_lap_update_player(&player, &track, 0x440u, 0x400u, 2, false, 5,
                              &next_finish_place) ||
        !ww_lap_update_player(&player, &track, 0x460u, 0x400u, 2, false, 6,
                              &next_finish_place) ||
        !ww_lap_update_player(&player, &track, 0x400u, 0x400u, 2, false, 7,
                              &next_finish_place) ||
        player.current_lap != 2 || player.finished ||
        !player.last_lap_alert_pending ||
        !ww_lap_update_player(&player, &track, 0x440u, 0x400u, 2, false, 8,
                              &next_finish_place) ||
        !ww_lap_update_player(&player, &track, 0x460u, 0x400u, 2, false, 9,
                              &next_finish_place) ||
        !ww_lap_update_player(&player, &track, 0x400u, 0x400u, 2, false, 10,
                              &next_finish_place) ||
        player.current_lap != 3 || !player.finished ||
        player.finish_place != 1u || next_finish_place != 1u) {
        return false;
    }
    if (!ww_lap_update_opponent(&opponent, &track, 0x460u, 0x400u, 1,
                                &next_finish_place) ||
        !ww_lap_update_opponent(&opponent, &track, 0x400u, 0x400u, 1,
                                &next_finish_place) ||
        opponent.current_lap != 1 || opponent.finished ||
        !ww_lap_update_opponent(&opponent, &track, 0x440u, 0x400u, 1,
                                &next_finish_place) ||
        !ww_lap_update_opponent(&opponent, &track, 0x460u, 0x400u, 1,
                                &next_finish_place) ||
        !ww_lap_update_opponent(&opponent, &track, 0x400u, 0x400u, 1,
                                &next_finish_place) ||
        !opponent.finished || opponent.finish_place != 2u ||
        next_finish_place != 2u) {
        return false;
    }
    memset(rank_state, 0, sizeof(rank_state));
    memset(rank, 8, sizeof(rank));
    rank_state[0].course_progress = 50u;
    rank_state[1].course_progress = 80u;
    rank_state[2].course_progress = 70u;
    rank_state[3].course_progress = 60u;
    rank_state[4].course_progress = 40u;
    rank_state[5].course_progress = 30u;
    rank_state[6].course_progress = 20u;
    rank_state[7].course_progress = 10u;
    ww_lap_update_ranks(rank_state, rank, WW_RACER_COUNT);
    if (rank[0] != 4u || rank[1] != 1u || rank[7] != 8u) return false;
    rank_state[4].course_progress = rank_state[0].course_progress;
    ww_lap_update_ranks(rank_state, rank, WW_RACER_COUNT);
    if (rank[0] != 4u) return false;
    rank_state[0].finish_place = 2u;
    rank_state[0].course_progress = 100u;
    ww_lap_update_ranks(rank_state, rank, WW_RACER_COUNT);
    if (rank[0] != 2u) return false;
    return true;
}

static bool ww_test_finish_state(void)
{
    WwLapState racers[WW_FINISH_RACER_COUNT];
    WwFinishState finish;
    memset(racers, 0, sizeof(racers));
    ww_finish_reset(&finish);
    if (!ww_finish_update(&finish, racers, 2, 0, 1200u, 400u) ||
        finish.phase != WW_FINISH_RACING) {
        return false;
    }

    racers[0].finished = true;
    racers[0].finish_place = 2u;
    if (!ww_finish_update(&finish, racers, 2, 1, 1234u, 500u) ||
        finish.phase != WW_FINISH_PLAYER_FINISHED ||
        finish.final_time_tenths != 1234u) {
        return false;
    }
    racers[1].finished = true;
    racers[1].finish_place = 1u;
    racers[2].finished = true;
    racers[2].finish_place = 3u;
    if (!ww_finish_update(&finish, racers, 2, 3, 1300u, 600u) ||
        finish.phase != WW_FINISH_POINTS_READY ||
        finish.final_time_tenths != 1234u ||
        finish.pending_points[0] != 9u ||
        finish.pending_points[1] != 12u ||
        finish.pending_points[2] != 6u ||
        ww_finish_results_due(&finish, 1399u) ||
        !ww_finish_results_due(&finish, 1400u)) {
        return false;
    }

    ww_finish_reset(&finish);
    memset(racers, 0, sizeof(racers));
    racers[0].finished = true;
    racers[0].finish_place = 1u;
    if (!ww_finish_update(&finish, racers, 4, 1, 99u, 10u) ||
        finish.phase != WW_FINISH_POINTS_READY ||
        finish.pending_points[0] != 0u) {
        return false;
    }
    return true;
}

static uint16_t ww_test_solid_collision_probe(void *context,
                                              uint16_t world_x,
                                              uint16_t world_y)
{
    (void)context;
    (void)world_x;
    (void)world_y;
    return 2u;
}

static bool ww_test_player_crash_sequence(WwRace *race,
                                          const WwRenderer *renderer)
{
    WwRacerState player;
    WwRenderQueue queue;
    uint32_t tick;
    unsigned frame;
    if (race == NULL || renderer == NULL) return false;
    player = race->racers[0];
    ww_racer_player_begin_crash(&player, 165);
    if (player.crash_state != WW_RACER_CRASH_RISING ||
        player.crash_frame != 0u || player.crash_y != 165) {
        return false;
    }

    /* sub_26A3C consumes individual frames 4..11 while rising. */
    for (frame = 0; frame < 8u; ++frame) {
        tick = 100u + frame * 12u;
        ww_render_queue_init(&queue);
        if (!ww_racer_enqueue_player(
                &queue, renderer, &race->perspective_scale[0], &player,
                race->car_sprites, race->car_sprites_size,
                race->vehicle_sprites, race->vehicle_sprites_size, 2u,
                0u, tick) ||
            queue.count != 1u ||
            queue.item[0].source != race->vehicle_sprites +
                (4u + frame) * WW_CAR_SOURCE_BYTES ||
            queue.item[0].y != 159 - (int16_t)(frame * 6u)) {
            return false;
        }
    }

    /* The ninth step clamps to 0x72 and immediately selects frame 12. */
    ww_render_queue_init(&queue);
    if (!ww_racer_enqueue_player(
            &queue, renderer, &race->perspective_scale[0], &player,
            race->car_sprites, race->car_sprites_size,
            race->vehicle_sprites, race->vehicle_sprites_size, 2u,
            0u, 196u) ||
        player.crash_state != WW_RACER_CRASH_HOLD ||
        player.crash_y != 0x72 || player.crash_phase_tick != 196u ||
        queue.count != 1u ||
        queue.item[0].source !=
            race->vehicle_sprites + 12u * WW_CAR_SOURCE_BYTES) {
        return false;
    }
    ww_render_queue_init(&queue);
    if (!ww_racer_enqueue_player(
            &queue, renderer, &race->perspective_scale[0], &player,
            race->car_sprites, race->car_sprites_size,
            race->vehicle_sprites, race->vehicle_sprites_size, 2u,
            0u, 207u) ||
        player.crash_state != WW_RACER_CRASH_RECOVERING ||
        queue.item[0].source !=
            race->vehicle_sprites + 12u * WW_CAR_SOURCE_BYTES) {
        return false;
    }
    for (frame = 0; frame < 22u; ++frame) {
        ww_render_queue_init(&queue);
        tick = 219u + frame * 12u;
        if (!ww_racer_enqueue_player(
                &queue, renderer, &race->perspective_scale[0], &player,
                race->car_sprites, race->car_sprites_size,
                race->vehicle_sprites, race->vehicle_sprites_size, 2u,
                0u, tick)) {
            return false;
        }
    }
    ww_render_queue_init(&queue);
    if (player.crash_state != WW_RACER_CRASH_COMPLETE ||
        player.crash_y != 0xc7 ||
        !ww_racer_enqueue_player(
            &queue, renderer, &race->perspective_scale[0], &player,
            race->car_sprites, race->car_sprites_size,
            race->vehicle_sprites, race->vehicle_sprites_size, 2u,
            0u, tick + 12u) ||
        queue.count != 0u) {
        return false;
    }
    return true;
}

static bool ww_test_player_drive_and_finish_sprites(
    WwRace *race, const WwRenderer *renderer)
{
    static const unsigned steering_individual_frame[5] = {
        0u, 1u, UINT_MAX, 2u, 3u
    };
    static const unsigned lower_finish_frame[8] = {
        8u, 9u, 8u, 4u, 10u, 11u, 10u, 4u
    };
    WwRacerState player;
    unsigned steering;
    if (race == NULL || renderer == NULL) return false;
    player = race->racers[0];
    for (steering = 0u; steering < 5u; ++steering) {
        WwRenderQueue queue;
        const uint8_t *expected;
        ww_render_queue_init(&queue);
        if (!ww_racer_enqueue_player(
                &queue, renderer, &race->perspective_scale[0], &player,
                race->car_sprites, race->car_sprites_size,
                race->vehicle_sprites, race->vehicle_sprites_size,
                (uint8_t)steering, 0u, 0u) || queue.count != 1u) {
            return false;
        }
        expected = steering_individual_frame[steering] == UINT_MAX
            ? race->car_sprites +
                ((size_t)player.vehicle * WW_CAR_FRAMES + 4u) *
                    WW_CAR_SOURCE_BYTES
            : race->vehicle_sprites +
                (size_t)steering_individual_frame[steering] *
                    WW_CAR_SOURCE_BYTES;
        if (queue.item[0].source != expected) return false;
    }

    player.twirl_active = true;
    for (steering = 0u; steering < 8u; ++steering) {
        static const uint8_t twirl[8] = {4, 5, 6, 7, 0, 1, 2, 3};
        WwRenderQueue queue;
        const uint8_t *expected;
        player.twirl_frame = (uint8_t)steering;
        expected = race->car_sprites +
            ((size_t)player.vehicle * WW_CAR_FRAMES + twirl[steering]) *
                WW_CAR_SOURCE_BYTES;
        ww_render_queue_init(&queue);
        if (!ww_racer_enqueue_player(
                &queue, renderer, &race->perspective_scale[0], &player,
                race->car_sprites, race->car_sprites_size,
                race->vehicle_sprites, race->vehicle_sprites_size,
                2u, 0u, 0u) || queue.count != 1u ||
            queue.item[0].source != expected) {
            return false;
        }
    }
    player.twirl_active = false;

    player.finish_frame = 0u;
    for (steering = 0u; steering < 8u; ++steering) {
        WwRenderQueue queue;
        const uint8_t *expected = race->car_sprites +
            ((size_t)player.vehicle * WW_CAR_FRAMES +
             lower_finish_frame[steering]) * WW_CAR_SOURCE_BYTES;
        ww_render_queue_init(&queue);
        if (!ww_racer_enqueue_player(
                &queue, renderer, &race->perspective_scale[0], &player,
                race->car_sprites, race->car_sprites_size,
                race->vehicle_sprites, race->vehicle_sprites_size,
                2u, 4u, 0u) || queue.count != 1u ||
            queue.item[0].source != expected) {
            return false;
        }
        ww_racer_player_finish_animation_step(&player, 4u);
    }
    if (player.finish_frame != 0u) return false;

    player.finish_frame = 0u;
    for (steering = 0u; steering < 5u; ++steering) {
        ww_racer_player_finish_animation_step(&player, 1u);
    }
    {
        WwRenderQueue queue;
        size_t first_celebration = 12u + 4u;
        ww_render_queue_init(&queue);
        if (!ww_racer_enqueue_player(
                &queue, renderer, &race->perspective_scale[0], &player,
                race->car_sprites, race->car_sprites_size,
                race->vehicle_sprites, race->vehicle_sprites_size,
                2u, 1u, 0u) || queue.count != 1u ||
            queue.item[0].source != race->vehicle_sprites +
                first_celebration * WW_CAR_SOURCE_BYTES) {
            return false;
        }
    }
    return true;
}

static bool ww_sibling_path(char output[1024], const char *path,
                            const char *sibling)
{
    const char *slash = strrchr(path, '\\');
    const char *forward = strrchr(path, '/');
    size_t prefix;
    int written;
    if (forward != NULL && (slash == NULL || forward > slash)) slash = forward;
    prefix = slash == NULL ? 0u : (size_t)(slash - path + 1);
    if (prefix >= 1024) return false;
    memcpy(output, path, prefix);
    written = snprintf(output + prefix, 1024 - prefix, "%s", sibling);
    return written > 0 && (size_t)written < 1024 - prefix;
}

static bool ww_test_race_selection(const WwArchive *archive)
{
    WwRaceSelection selection;
    char title[64];
    char base_name[8];
    char music_name[16];
    unsigned group;
    unsigned song;

    if (!ww_race_selection_init(&selection, false, 0, 0, 1) ||
        selection.track_number != 1 ||
        !ww_race_selection_title(&selection, title, sizeof(title)) ||
        strcmp(title, "BRONZE WHEELS. TRACK 1. 12HP") != 0 ||
        !ww_race_selection_base_name(&selection, base_name, sizeof(base_name)) ||
        strcmp(base_name, "1") != 0) {
        return false;
    }
    if (!ww_race_selection_init(&selection, false, 2, 4, 2) ||
        selection.track_number != 15 ||
        !ww_race_selection_init(&selection, true, 0, 0, 1) ||
        selection.track_number != 22 ||
        !ww_race_selection_init(&selection, true, 2, 4, 2) ||
        selection.track_number != 36 ||
        !ww_race_selection_title(&selection, title, sizeof(title)) ||
        strcmp(title, "BONUS GOLD. TRACK 5. 6HP") != 0) {
        return false;
    }
    if (ww_race_selection_init(&selection, false, 3, 0, 1) ||
        ww_race_selection_init(&selection, false, 0, 5, 1) ||
        ww_race_selection_init(&selection, false, 0, 0, 0)) {
        return false;
    }
    for (group = 0; group < 6u; ++group) {
        const WwArchiveEntry *marker;
        if (!ww_race_selection_init(&selection, group >= 3u, group % 3u,
                                    0, 1)) {
            return false;
        }
        marker = ww_archive_find(archive, ww_race_marker_asset(&selection));
        if (marker == NULL || marker->stored_size != 19500u) {
            return false;
        }
        for (song = 0; song < 3u; ++song) {
            if (!ww_music_race_asset(group >= 3u, group % 3u, song, false,
                                     music_name, sizeof(music_name)) ||
                ww_archive_find(archive, music_name) == NULL ||
                !ww_music_race_asset(group >= 3u, group % 3u, song, true,
                                     music_name, sizeof(music_name)) ||
                ww_archive_find(archive, music_name) == NULL) {
                return false;
            }
        }
    }
    return true;
}

static bool ww_test_race_open(const WwArchive *archive,
                              const WwRenderer *renderer)
{
    WwRaceSelection selection;
    WwRace race;
    WwPlayerControls controls;
    WwRacerState projection_racer;
    WwProjectedRacer projected;
    bool projection_visible;
    bool valid = true;
    memset(&race, 0, sizeof(race));
    if (!ww_race_selection_init(&selection, false, 0, 0, 1) ||
        !ww_race_open(&race, archive, &selection, 0, 6, 1,
                      true, true, true) ||
        strcmp(race.track.base_name, "1") != 0 ||
        race.marker_size != WW_RACE_MARKER_BYTES ||
        race.velocity_table_size != WW_PHYSICS_VELOCITY_BYTES ||
        race.car_sprites_size != WW_CAR_BYTES ||
        race.vehicle_sprites_size != 18u * WW_CAR_SOURCE_BYTES ||
        !race.minimap.loaded || race.minimap.background == NULL ||
        race.minimap.background_size !=
            WW_MINIMAP_WIDTH * WW_MINIMAP_HEIGHT ||
        race.minimap.map_size != WW_MINIMAP_MAP_BYTES ||
        race.steering_frame != 2u ||
        race.perspective_scale[0].level[0].width != 38u ||
        race.perspective_scale[0].level[0].height != 28u ||
        race.perspective_scale[1].level[0].width != 28u ||
        race.perspective_scale[2].level[0].width != 14u ||
        race.perspective_scale[3].level[0].width != 32u ||
        race.perspective_scale[4].level[0].width != 15u ||
        race.perspective_scale[5].level[0].width != 18u ||
        !race.world_sprites.loaded ||
        !race.dynamic_sprites.loaded ||
        race.dynamic_sprites.hogmis_size != WW_DYNAMIC_HOGMIS_BYTES ||
        race.dynamic_sprites.spark_size != WW_DYNAMIC_SPARK_BYTES ||
        !race.water_assets.loaded ||
        race.water_assets.shallow.periscope_offset != 0x0b9au ||
        race.water_assets.shallow.periscope_stride != 0x0120u ||
        race.water_assets.shallow.periscope_width != 12u ||
        race.water_assets.shallow.periscope_height != 24u ||
        race.water_assets.shallow.periscope_frames != 2u ||
        race.water_assets.deep[0].splash_offset != 0x0ddau ||
        race.water_assets.deep[0].splash_stride != 0x0428u ||
        race.water_assets.deep[0].splash_frames != 4u ||
        race.water_assets.deep[0].periscope_offset != 0x1e7au ||
        race.water_assets.deep[0].periscope_stride != 0x0300u ||
        race.water_assets.deep[0].periscope_frames != 8u ||
        race.water_assets.deep[1].splash_offset != 0x4322u ||
        race.water_assets.deep[1].periscope_offset != 0x53c2u ||
        race.track.surface_class_count <= WW_WATER_DEEP_CLASS_SECOND ||
        race.track.surface_drag[0] != 0u ||
        race.track.surface_drag[WW_WATER_SHALLOW_CLASS] != 0x1999u ||
        race.track.surface_drag[WW_WATER_DEEP_CLASS_FIRST] != 0xb333u ||
        race.track.surface_drag[7] != 0x4cccu ||
        !race.hud.loaded ||
        race.hud.lap_sprites_size != WW_HUD_LAP_ASSET_BYTES ||
        race.hud.general_effects_size != WW_HUD_GENERAL_EFFECT_BYTES ||
        race.hud.icons_size != WW_HUD_ICON_ASSET_BYTES ||
        race.hud.giggles_size != WW_HUD_GIGGLES_BYTES ||
        race.hud.gig_movement_size != WW_HUD_GIG_MOV_BYTES ||
        race.hud.puff_size != WW_HUD_PUFF_BYTES ||
        !race.clock_display || !race.speedometer ||
        race.start_sequence_frame != 0u ||
        race.start_light_frame != 0u ||
        race.start_released || race.start_sound_pending ||
        race.player_collision_ignore_racer != -1 ||
        race.player_collision_ignore_ticks != 0u ||
        race.player_weapon.selected_type != 0u ||
        race.player_weapon.ammunition != 0u ||
        race.lives != 3u ||
        race.racers[0].crash_state != WW_RACER_CRASH_NONE ||
        strcmp(race.world_sprites.descriptor[0].asset_name, "H1.SP") != 0 ||
        race.world_sprites.descriptor[0].source_width != 32u ||
        race.world_sprites.descriptor[0].source_height != 24u ||
        race.world_sprites.descriptor[0].scale_bank != 3u ||
        race.world_sprites.descriptor[0].frame_count != 2u ||
        race.world_sprites.descriptor[0].sprites_size != 1536u ||
        strcmp(race.world_sprites.descriptor[23].asset_name, "OB1.SP") != 0 ||
        race.world_sprites.descriptor[23].scale_bank != 1u ||
        race.world_sprites.descriptor[23].sprites_size != 784u ||
        strcmp(race.world_sprites.descriptor[35].asset_name, "OB13.SP") != 0 ||
        race.world_sprites.descriptor[35].scale_bank != 2u ||
        race.racers[0].vehicle != 0u || race.racers[1].vehicle != 1u ||
        race.racers[7].vehicle != 7u ||
        race.racers[4].world_x != race.initial.grid_x[4] ||
        race.racers[4].world_y != race.initial.grid_y[4] ||
        race.racers[4].heading != WW_RACE_INITIAL_HEADING ||
        race.opponent_path[1].launch_steps != 20u ||
        race.opponent_path[1].approach_steps != 34u ||
        race.opponent_path[1].initial_countdown != 24u ||
        race.opponent_path[1].approach_countdown != 28u ||
        race.opponent_path[7].launch_steps != 80u ||
        race.opponent_path[7].approach_steps != 58u ||
        race.opponent_path[7].cruise_steps != 34u ||
        race.opponent_path[7].approach_countdown != 60u ||
        (int16_t)ww_read_le16(race.velocity_table + 100u * 2u) != 40 ||
        race.horizon_source_offset != 0x50u || race.race_class != 1u ||
        race.player.camera_x != race.initial.grid_x[0] ||
        race.player.camera_y !=
            (uint16_t)(race.initial.grid_y[0] - 0x7cu)) {
        valid = false;
    }
    {
        WwDisplay *water_display =
            (WwDisplay *)calloc(1u, sizeof(*water_display));
        WwRenderQueue water_queue;
        unsigned left_pixels = 0u;
        unsigned right_pixels = 0u;
        unsigned x;
        unsigned y;
        ww_water_update(&race.water, WW_WATER_SHALLOW_CLASS, true);
        if (race.water.submerged || !race.water.shallow_active ||
            race.water.shallow_frame != 0u || water_display == NULL ||
            !ww_water_draw_shallow_spray(
                &race.water_assets, &race.water, renderer,
                &race.perspective_scale[0], water_display)) {
            valid = false;
        }
        if (water_display != NULL) {
            int car_x = WW_SCREEN_WIDTH / 2 - 38 / 2;
            int car_y = (int)renderer->projection_right[
                            WW_RACER_PLAYER_DISTANCE] - 28;
            for (y = 0u; y < 24u; ++y) {
                for (x = 0u; x < 12u; ++x) {
                    if (water_display->pages[0][
                            (size_t)(car_y + 4 + (int)y) * WW_SCREEN_WIDTH +
                            (size_t)(car_x + 1 + (int)x)] != 0u) {
                        ++left_pixels;
                    }
                    if (water_display->pages[0][
                            (size_t)(car_y + 4 + (int)y) * WW_SCREEN_WIDTH +
                            (size_t)(car_x + 0x19 + (int)x)] != 0u) {
                        ++right_pixels;
                    }
                }
            }
            if (left_pixels == 0u || left_pixels != right_pixels) {
                valid = false;
            }
        }
        ww_water_render_step(&race.water, &race.water_assets);
        if (race.water.shallow_frame != 1u) valid = false;
        ww_water_update(&race.water, WW_WATER_DEEP_CLASS_FIRST, true);
        ww_render_queue_init(&water_queue);
        if (!race.water.submerged || race.water.shallow_active ||
            !race.water.splash_active ||
            !race.water.splash_sound_pending ||
            !ww_water_enqueue_periscope(
                &race.water_assets, &race.water, renderer,
                &race.perspective_scale[3], &water_queue) ||
            water_queue.count != 1u ||
            water_queue.item[0].source != race.water_assets.effects +
                race.water_assets.deep[0].periscope_offset) {
            valid = false;
        }
        ww_water_update(&race.water, 3u, true);
        if (!race.water.submerged) valid = false;
        ww_water_update(&race.water, WW_WATER_SHALLOW_CLASS, true);
        if (race.water.submerged || !race.water.shallow_active ||
            !race.water.splash_active) {
            valid = false;
        }
        ww_water_update(&race.water, 14u, true);
        if (race.water.shallow_active) valid = false;
        ww_water_state_reset(&race.water);
        free(water_display);
    }
    {
        WwHudLapAlert alert;
        unsigned frame;
        ww_hud_lap_alert_begin(&alert);
        for (frame = 0u; frame < 81u; ++frame) {
            ww_hud_lap_alert_step(&alert, &race.hud);
        }
        if (!alert.active || alert.phase != WW_HUD_LAP_ALERT_PUFF ||
            alert.motion_frame != 0u || alert.sprite_frame != 1u) {
            valid = false;
        }
        for (frame = 0u; frame < 3u; ++frame) {
            ww_hud_lap_alert_step(&alert, &race.hud);
        }
        if (alert.active) valid = false;
    }
    {
        WwAiRacerPathMotion motion = race.opponent_path[1];
        uint16_t x = race.racers[1].world_x;
        uint16_t y = race.racers[1].world_y;
        uint16_t heading = race.racers[1].heading;
        unsigned frame;
        for (frame = 0; frame < 24u; ++frame) {
            if (!ww_ai_racer_path_step(&motion, &race.track, &x, &y,
                                       &heading) ||
                (frame < 23u && motion.speed_mode != 0u)) {
                valid = false;
                break;
            }
        }
        if (motion.initial_countdown != 0u || motion.speed_mode != 1u ||
            motion.approach_countdown != 28u) {
            valid = false;
        }
    }
    {
        WwPlayerMotion drag_motion = race.player;
        WwPlayerMotion collision_motion = race.player;
        memset(&controls, 0, sizeof(controls));
        controls.accelerate = true;
        drag_motion.speed_index = 100u;
        drag_motion.surface_class = WW_WATER_SHALLOW_CLASS;
        if (!ww_physics_player_step(
                &drag_motion, controls, &race.track,
                renderer->trig_data, WW_TRIG_BYTES,
                renderer->ndist_data, WW_NDIST_BYTES, race.velocity_table,
                race.velocity_table_size, WW_ROAD_DETAIL_HIGH, NULL, NULL) ||
            drag_motion.velocity != 37) {
            ww_error("WACKY.SDX shallow drag validation failed: velocity=%d",
                     (int)drag_motion.velocity);
            valid = false;
        }
        drag_motion = race.player;
        drag_motion.speed_index = 100u;
        drag_motion.surface_class = WW_WATER_DEEP_CLASS_FIRST;
        if (!ww_physics_player_step(
                &drag_motion, controls, &race.track,
                renderer->trig_data, WW_TRIG_BYTES,
                renderer->ndist_data, WW_NDIST_BYTES, race.velocity_table,
                race.velocity_table_size, WW_ROAD_DETAIL_HIGH, NULL, NULL) ||
            drag_motion.velocity != 13) {
            ww_error("WACKY.SDX deep drag validation failed: velocity=%d",
                     (int)drag_motion.velocity);
            valid = false;
        }
        collision_motion.speed_index = 100u;
        if (!ww_physics_player_step(
                &collision_motion, controls, &race.track,
                renderer->trig_data, WW_TRIG_BYTES,
                renderer->ndist_data, WW_NDIST_BYTES, race.velocity_table,
                race.velocity_table_size, WW_ROAD_DETAIL_HIGH,
                ww_test_solid_collision_probe, NULL) ||
            collision_motion.collision_status != 2u ||
            collision_motion.camera_x != race.player.camera_x ||
            collision_motion.camera_y != race.player.camera_y) {
            ww_error("static collision safe-point validation failed: "
                     "status=%u camera=%u,%u expected=%u,%u",
                     (unsigned)collision_motion.collision_status,
                     (unsigned)collision_motion.camera_x,
                     (unsigned)collision_motion.camera_y,
                     (unsigned)race.player.camera_x,
                     (unsigned)race.player.camera_y);
            valid = false;
        }
    }
    {
        char time_text[8];
        if (!ww_hud_format_race_time(3723u, time_text) ||
            strcmp(time_text, "06:12:3") != 0 ||
            !ww_hud_format_race_time(0x8ca1u, time_text) ||
            strcmp(time_text, "00:00:0") != 0) {
            valid = false;
        }
    }
    if (!ww_test_player_crash_sequence(&race, renderer)) {
        valid = false;
    }
    if (!ww_test_player_drive_and_finish_sprites(&race, renderer)) {
        valid = false;
    }
    {
        WwDisplay *test_display =
            (WwDisplay *)calloc(1u, sizeof(*test_display));
        WwVictory victory;
        unsigned racer;
        memset(&victory, 0, sizeof(victory));
        race.racer_lap[0].finish_place = 2u;
        race.racer_lap[1].finish_place = 1u;
        race.racer_lap[2].finish_place = 3u;
        for (racer = 0; racer < WW_RACER_COUNT; ++racer) {
            race.racer_rank[racer] = (uint8_t)(racer + 1u);
        }
        if (test_display == NULL ||
            !ww_victory_open(&victory, archive, test_display, &race) ||
            victory.order_image.width != WW_SCREEN_WIDTH ||
            victory.win_image.height != WW_SCREEN_HEIGHT ||
            victory.puff_sprites_size != 4u * WW_CAR_SOURCE_BYTES ||
            victory.finish_order[0] != 1u ||
            victory.finish_order[1] != 0u ||
            victory.finish_order[2] != 2u || victory.final_race ||
            !ww_victory_has_next_race(&victory) ||
            victory.race_selection.race_index != 0u ||
            victory.selected_vehicle != race.selected_vehicle ||
            victory.lap_count != race.lap_count) {
            valid = false;
        }
        ww_victory_close(&victory);
        if (test_display != NULL) {
            size_t differences = 0u;
            size_t finish_pixels = 0u;
            unsigned x;
            unsigned y;
            if (!ww_hud_draw_finish_place(
                    &race.hud, test_display, race.car_sprites,
                    race.car_sprites_size, race.racers[1].vehicle, 1u)) {
                valid = false;
            }
            for (y = 4u; y < 32u; ++y) {
                for (x = 0xf0u; x < WW_SCREEN_WIDTH; ++x) {
                    if (test_display->pages[0][
                            y * WW_SCREEN_WIDTH + x] != 0u) {
                        ++finish_pixels;
                    }
                }
            }
            if (finish_pixels == 0u) valid = false;
            if (!ww_minimap_draw(&race.minimap, test_display, race.racers)) {
                valid = false;
            }
            for (x = 0; x < WW_MINIMAP_WIDTH; ++x) {
                for (y = 0; y < WW_MINIMAP_HEIGHT; ++y) {
                    uint8_t expected = race.minimap.background[
                        (size_t)x * WW_MINIMAP_HEIGHT + y];
                    uint8_t actual = test_display->pages[0][
                        (size_t)(WW_MINIMAP_SCREEN_Y + y) *
                            WW_SCREEN_WIDTH + x];
                    if (expected != actual) ++differences;
                }
            }
            /* At most one pixel per active racer differs from the restored
             * 78x50 board map; co-located grid markers can reduce the count. */
            if (differences > WW_RACER_COUNT) valid = false;
        }
        free(test_display);
        for (racer = 0; racer < WW_RACER_COUNT; ++racer) {
            race.racer_lap[racer].finish_place = 0u;
            race.racer_rank[racer] = 8u;
        }
    }
    {
        WwInput crash_input;
        /* The assembly keeps crash and vehicle updates behind word_88C44.
         * Put the regression fixture past the starting lights before asking
         * it to exercise the completed-crash restart path. */
        race.start_sequence_frame = 34u;
        race.start_light_frame = 2u;
        race.start_released = true;
        race.start_sound_pending = false;
        memset(&crash_input, 0, sizeof(crash_input));
        race.racers[0].crash_state = WW_RACER_CRASH_COMPLETE;
        race.racers[0].crash_phase_tick = race.elapsed_136_ticks;
        if (!ww_race_update(&race, renderer, &crash_input,
                            WW_ROAD_DETAIL_HIGH,
                            WW_RACER_CRASH_RESTART_TICKS) ||
            !race.open || race.lives != 2u ||
            race.racers[0].crash_state != WW_RACER_CRASH_NONE ||
            race.player.camera_x != race.initial.camera_x ||
            race.player.camera_y != race.initial.camera_y ||
            race.player_weapon.selected_type != 0u ||
            race.player_weapon.ammunition != 0u ||
            race.start_sequence_frame != 0u ||
            race.start_light_frame != 0u || race.start_released ||
            race.start_sound_pending) {
            valid = false;
        }
    }
    {
        bool consumed = false;
        if (!ww_weapon_collect(&race.player_weapon, 2, &consumed) ||
            !consumed || race.player_weapon.ammunition != 4u ||
            !ww_weapon_update_fire(
                &race.player_weapon, true, &race.dynamic_objects, renderer,
                &race.racers[0]) ||
            race.player_weapon.ammunition != 3u ||
            race.dynamic_objects.active_by_owner[0] != 1u ||
            !race.dynamic_objects.object[0].active ||
            race.dynamic_objects.object[0].type != 0u ||
            race.dynamic_objects.object[0].source_offset != 0x2beu ||
            race.dynamic_objects.object[0].animation_frames != 3u ||
            race.dynamic_objects.object[0].heading_or_mode != 0u ||
            !ww_weapon_update_fire(
                &race.player_weapon, true, &race.dynamic_objects, renderer,
                &race.racers[0]) ||
            race.dynamic_objects.active_by_owner[0] != 1u ||
            !ww_weapon_update_fire(
                &race.player_weapon, false, &race.dynamic_objects, renderer,
                &race.racers[0])) {
            valid = false;
        }
        ww_dynamic_object_clear(&race.dynamic_objects);
        ww_weapon_reset(&race.player_weapon);
        consumed = false;
        if (!ww_weapon_collect(&race.player_weapon, 4, &consumed) ||
            !consumed || race.player_weapon.selected_type != 4u ||
            !ww_weapon_update_fire(
                &race.player_weapon, true, &race.dynamic_objects, renderer,
                &race.racers[0]) ||
            race.player_weapon.selected_type != 0u ||
            race.dynamic_objects.active_by_owner[0] != 2u ||
            race.dynamic_objects.object[0].type != 1u ||
            race.dynamic_objects.object[1].type != 1u ||
            race.dynamic_objects.object[0].source_offset != 0x666u ||
            race.dynamic_objects.object[1].source_offset != 0x666u) {
            valid = false;
        }
        ww_dynamic_object_clear(&race.dynamic_objects);
        ww_weapon_reset(&race.player_weapon);
    }
    if (race.track.spawn_record_count != 0) {
        WwSpawnRecord *spawn = &race.track.spawn_records[0];
        int type = spawn->sprite_type;
        int16_t old_frame = spawn->animation_frame;
        int16_t expected_frame = (int16_t)(old_frame + 1);
        if (type < 0 || type >= WW_WORLD_SPRITE_DESCRIPTORS) {
            valid = false;
        } else {
            if (expected_frame ==
                (int16_t)race.world_sprites.descriptor[type].frame_count) {
                expected_frame = 0;
            }
            if (!ww_world_object_update(&race.track, &race.world_sprites) ||
                spawn->state != 0 ||
                spawn->animation_frame != expected_frame) {
                valid = false;
            }
        }
    }
    memset(&projection_racer, 0, sizeof(projection_racer));
    projection_racer.world_x = 2000u;
    projection_racer.world_y = 2124u;
    projection_racer.heading = WW_RACE_INITIAL_HEADING;
    projection_racer.vehicle = 1u;
    projection_racer.active = true;
    projection_visible = ww_racer_project(
        renderer, &race.perspective_scale[0], &projection_racer, 1,
        2000u, 2000u, WW_RACE_INITIAL_HEADING, &projected);
    if (!projection_visible ||
        projected.x != 139 || projected.y != 165 ||
        projected.width != 38u || projected.height != 28u ||
        projected.distance != 123u || projected.scale_level != 0u ||
        projected.direction_frame != 4u) {
        ww_error("sub_255D4 projection mismatch: visible=%u x=%d y=%d "
                 "size=%ux%u distance=%u scale=%u frame=%u",
                 projection_visible ? 1u : 0u,
                 projection_visible ? projected.x : 0,
                 projection_visible ? projected.y : 0,
                 projection_visible ? projected.width : 0u,
                 projection_visible ? projected.height : 0u,
                 projection_visible ? projected.distance : 0u,
                 projection_visible ? projected.scale_level : 0u,
                 projection_visible ? projected.direction_frame : 0u);
        valid = false;
    }
    projection_racer.heading = 0;
    if (!ww_racer_project(renderer, &race.perspective_scale[0],
                          &projection_racer, 1,
                          2000u, 2000u, WW_RACE_INITIAL_HEADING,
                          &projected) ||
        projected.direction_frame != 2u ||
        ww_racer_relative_direction_frame(WW_RACE_INITIAL_HEADING,
                                           WW_RACE_INITIAL_HEADING) != 4u ||
        ww_racer_relative_direction_frame(
            (uint16_t)(WW_RACE_INITIAL_HEADING +
                       WW_PHYSICS_ANGLE_COUNT / 2u),
            WW_RACE_INITIAL_HEADING) != 0u) {
        valid = false;
    }
    {
        WwPlayerMotion flight_motion = race.player;
        memset(&controls, 0, sizeof(controls));
        controls.steer_left = true;
        flight_motion.jump_active = true;
        flight_motion.speed_index = 100u;
        flight_motion.velocity =
            (int16_t)ww_read_le16(race.velocity_table + 100u * 2u);
        if (!ww_physics_player_step_unblocked(
                &flight_motion, controls, renderer->trig_data, WW_TRIG_BYTES,
                renderer->ndist_data, WW_NDIST_BYTES, race.velocity_table,
                race.velocity_table_size, WW_ROAD_DETAIL_HIGH) ||
            flight_motion.speed_index != 100u ||
            flight_motion.velocity != 40 ||
            flight_motion.heading != 0x1b8u) {
            ww_error("sub_237E4 airborne-steering validation failed");
            valid = false;
        }
    }
    memset(&controls, 0, sizeof(controls));
    controls.accelerate = true;
    controls.steer_left = true;
    if (!ww_physics_player_step_unblocked(
            &race.player, controls, renderer->trig_data, WW_TRIG_BYTES,
            renderer->ndist_data, WW_NDIST_BYTES, race.velocity_table,
            race.velocity_table_size, WW_ROAD_DETAIL_HIGH) ||
        race.player.speed_index != 10u || race.player.velocity != 4 ||
        race.player.heading != 0x1b8u ||
        race.player.camera_x != (uint16_t)(race.initial.camera_x - 17u) ||
        race.player.camera_y != (uint16_t)(race.initial.camera_y + 5u)) {
        valid = false;
    }
    ww_race_close(&race);
    if (!ww_race_selection_init(&selection, true, 2, 4, 2) ||
        !ww_race_open(&race, archive, &selection, 7, 10, 3,
                      false, false, false) ||
        strcmp(race.track.base_name, "36") != 0 || race.race_class != 3u ||
        race.selected_vehicle != 7 || race.lap_count != 10 ||
        race.clock_display || race.speedometer ||
        race.racers[0].vehicle != 7u || race.racers[1].vehicle != 0u ||
        race.racers[7].vehicle != 6u ||
        (int16_t)ww_read_le16(race.velocity_table + 100u * 2u) != 28) {
        valid = false;
    }
    ww_race_close(&race);
    if (!ww_race_selection_init(&selection, false, 1u, 0u, 1u) ||
        !ww_race_open_duck(&race, archive, &selection, 0u, true, true)) {
        ww_error("Wacky Duck Shoot track-six open failed");
        valid = false;
    } else if (strcmp(race.track.base_name, "6") != 0 ||
        !race.duck.active ||
        race.duck.digger_sprites_size != WW_DUCK_SPRITE_BYTES ||
        race.duck.ordinary_duck_vehicle != 1u ||
        race.duck.special_duck_vehicle != 2u ||
        !race.start_released || race.start_light_frame != 2u ||
        race.player_weapon.ammunition != 0x63u) {
        ww_error("Wacky Duck Shoot open state failed: track=%s active=%u "
                 "sprites=%zu ducks=%u/%u start=%u light=%u ammo=%u",
                 race.track.base_name, race.duck.active ? 1u : 0u,
                 race.duck.digger_sprites_size,
                 (unsigned)race.duck.ordinary_duck_vehicle,
                 (unsigned)race.duck.special_duck_vehicle,
                 race.start_released ? 1u : 0u,
                 (unsigned)race.start_light_frame,
                 (unsigned)race.player_weapon.ammunition);
        valid = false;
    } else {
        unsigned frame;
        race.racers[1].vehicle = race.duck.special_duck_vehicle;
        race.racers[1].hit_effect = 1u;
        race.racers[1].hit_age = 0u;
        for (frame = 0u; frame < WW_DUCK_HIT_TICKS; ++frame) {
            if (!ww_duck_target_hit_step(
                    &race.duck, 1u, race.racers, race.opponent_path,
                    &race.track)) {
                valid = false;
                break;
            }
        }
        if (race.duck.score != 2u || !race.duck.score_sound_pending ||
            race.racers[1].hit_effect != 0u ||
            race.duck.target_generation[1] != 1u ||
            ww_duck_time_remaining(&race.duck, 0u) !=
                WW_DUCK_DURATION_TENTHS) {
            ww_error("Wacky Duck Shoot target/score validation failed: "
                     "score=%u generation=%u effect=%u",
                     (unsigned)race.duck.score,
                     (unsigned)race.duck.target_generation[1],
                     (unsigned)race.racers[1].hit_effect);
            valid = false;
        }
        ww_duck_update_timer(&race.duck, WW_DUCK_DURATION_TENTHS);
        if (!race.duck.finished ||
            ww_duck_time_remaining(&race.duck,
                                   WW_DUCK_DURATION_TENTHS) != 0u) {
            ww_error("Wacky Duck Shoot timer validation failed");
            valid = false;
        }
    }
    ww_race_close(&race);
    return valid;
}

bool ww_game_self_test(const char *data_path)
{
    WwArchive archive;
    WwOriginalConfig original_config;
    WwRenderer renderer;
    bool result;
    if (!ww_archive_open(&archive, data_path)) {
        return false;
    }
    ww_renderer_init(&renderer);
    result = ww_renderer_load_assets(&renderer, &archive);
    if (!result) {
        ww_error("TRIG.DAT/NDIST/VIEW renderer-table validation failed");
    }
    result = ww_test_archive(&archive, &renderer) && result;
    if (!ww_original_config_defaults(&original_config, &archive)) {
        ww_error("WACKY.ING default configuration validation failed");
        result = false;
    }
    if (renderer.projection_left[0] != 0 ||
        renderer.projection_right[0] != 240 ||
        renderer.projection_right[WW_PROJECTION_TABLE_COUNT - 1] -
                renderer.projection_left[WW_PROJECTION_TABLE_COUNT - 1] != 21 ||
        renderer.view_data == NULL ||
        (int32_t)ww_read_le32(renderer.view_data) != 14185 ||
        (int32_t)ww_read_le32(renderer.view_data + 160u * 4u) != 16384 ||
        (int32_t)ww_read_le32(renderer.view_data + 319u * 4u) != 14212) {
        ww_error("sub_274A4/VIEW projection-table validation failed");
        result = false;
    }
    if (ww_integer_sqrt(0) != 0 || ww_integer_sqrt(1) != 1 ||
        ww_integer_sqrt(2) != 2 || ww_integer_sqrt(4) != 2 ||
        ww_integer_sqrt(5) != 3 ||
        !ww_physics_rects_overlap((WwRect16){0, 0, 4, 4},
                                  (WwRect16){3, 3, 2, 2}) ||
        ww_physics_rects_overlap((WwRect16){0, 0, 4, 4},
                                 (WwRect16){4, 0, 2, 2})) {
        ww_error("translated math/physics primitive validation failed");
        result = false;
    }
    if (!ww_test_lap_progression()) {
        ww_error("sub_1260C/sub_12B44 lap progression validation failed");
        result = false;
    }
    if (!ww_test_finish_state()) {
        ww_error("sub_2FD8C finish/point-table validation failed");
        result = false;
    }
    {
        WwRacerState hit_racer;
        unsigned frame;
        memset(&hit_racer, 0, sizeof(hit_racer));
        hit_racer.hit_effect = 1u;
        for (frame = 0; frame < 0x21u; ++frame) {
            if (!ww_racer_hit_effect_step(&hit_racer)) result = false;
        }
        if (hit_racer.hit_effect != 0u ||
            ww_racer_hit_effect_step(&hit_racer)) {
            ww_error("sub_289EC racer hit-duration validation failed");
            result = false;
        }
    }
    if (!ww_test_race_selection(&archive)) {
        ww_error("race-selection/sub_17928/sub_32E60 validation failed");
        result = false;
    }
    if (!ww_test_race_open(&archive, &renderer)) {
        ww_error("sub_16F90/sub_2FD8C race-open/motion validation failed");
        result = false;
    }
    ww_archive_close(&archive);
    return result;
}

bool ww_game_open(WwGame *game, const WwGameOptions *options)
{
    char original_config_path[1024];
    memset(game, 0, sizeof(*game));
    game->engine_voice = -1;
    game->shallow_water_voice = -1;
    game->water_horn_voice = -1;
    SDL_SetMainReady();
    ww_settings_load(&game->settings);
    if (!ww_archive_open(&game->archive, options->data_path)) {
        return false;
    }
    if (!ww_original_config_defaults(&game->original_config, &game->archive)) {
        ww_error("WACKY.ING is missing or has the wrong size");
        ww_game_close(game);
        return false;
    }
    if (ww_sibling_path(original_config_path, options->data_path, "WACKY.CFG")) {
        FILE *original_file = fopen(original_config_path, "rb");
        if (original_file != NULL) {
            fclose(original_file);
            if (!ww_original_config_overlay(&game->original_config,
                                            original_config_path)) {
                ww_error("WACKY.CFG has the wrong size or validity word");
                ww_game_close(game);
                return false;
            }
        }
    }
    ww_profiles_defaults(&game->profiles);
    if (ww_sibling_path(original_config_path, options->data_path, "WACKY.DTT")) {
        FILE *profile_file = fopen(original_config_path, "rb");
        if (profile_file != NULL) {
            fclose(profile_file);
            if (!ww_profiles_load(&game->profiles, original_config_path)) {
                ww_error("WACKY.DTT has the wrong record layout");
                ww_game_close(game);
                return false;
            }
        }
    }
    /* Original game files remain read-only.  A previously recorded Duck
     * Shoot table in SDL's per-user preference directory overrides the
     * shipped WACKY.DTT values. */
    (void)ww_profiles_load_user(&game->profiles);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER |
                 SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        ww_error("SDL_Init failed: %s", SDL_GetError());
        ww_game_close(game);
        return false;
    }
    game->sdl_initialized = true;
    if (!ww_display_open(&game->display, "Wacky Wheels 95")) {
        ww_game_close(game);
        return false;
    }
    game->display.aspect_correct = game->settings.aspect_correct;
    if (game->settings.fullscreen) {
        ww_display_toggle_fullscreen(&game->display);
    }
    ww_input_init(&game->input);
    ww_timing_init(&game->timing);
    ww_renderer_init(&game->renderer);
    if (!ww_renderer_load_assets(&game->renderer, &game->archive)) {
        ww_error("TRIG.DAT, NDIST, or VIEW is missing or has the wrong size");
        ww_game_close(game);
        return false;
    }
    (void)ww_audio_open(&game->audio);
    game->audio.master_volume = game->settings.sound_volume;
    ww_audio_set_music_volume(&game->audio, game->settings.music_volume);
    if (game->audio.device != 0 &&
        ww_audio_load_voc(&game->archive, "MOTOR.VOC", &game->engine_sound) &&
        ww_audio_load_voc(&game->archive, "BOOM.VOC", &game->crash_sound) &&
        ww_audio_load_voc(&game->archive, "START.VOC", &game->start_sound)) {
        /* MOTOR.VOC's type-six repeat block is the continuous engine source
         * loaded separately at sub_12124. */
        game->race_sounds_loaded = true;
    }
    if (game->audio.device != 0) {
        (void)ww_audio_load_voc(&game->archive, "SPLASH.VOC",
                                &game->splash_sound);
        (void)ww_audio_load_voc(&game->archive, "PLIP.VOC",
                                &game->plip_sound);
        (void)ww_audio_load_voc(&game->archive, "HORN.VOC",
                                &game->horn_sound);
        (void)ww_audio_load_voc(&game->archive, "BELL.VOC",
                                &game->bell_sound);
    }
    game->menu_initialized = true;
    if (!ww_menu_open(&game->menu, &game->archive, &game->display, &game->audio,
                      &game->settings, options->asset_name)) {
        ww_game_close(game);
        return false;
    }
    if (options->asset_name == NULL && options->smoke_frames == 0u &&
        options->race_smoke_track == 0u &&
        options->duck_smoke_track == 0u) {
        if (!ww_intro_open(&game->intro, &game->archive, &game->display,
                           &game->audio, &game->renderer,
                           game->settings.music_volume)) {
            ww_game_close(game);
            return false;
        }
        game->intro_initialized = true;
    } else if (options->race_smoke_track == 0u &&
               options->duck_smoke_track == 0u) {
        ww_game_play_menu_music(game);
        game->intro_music_handoff = true;
    }
    if (options->race_smoke_track != 0) {
        WwRaceSelection selection;
        unsigned zero_based = options->race_smoke_track - 1u;
        bool bonus_pack = zero_based >= 15u;
        unsigned within_pack = zero_based % 15u;
        if (!ww_race_selection_init(&selection, bonus_pack,
                                    within_pack / WW_RACES_PER_BOARD,
                                    within_pack % WW_RACES_PER_BOARD, 1) ||
            !ww_race_open(&game->race, &game->archive, &selection, 0, 6, 1,
                          game->settings.clock_display,
                          game->settings.speedometer,
                          game->settings.overhead_map)) {
            ww_error("race smoke initialization failed for track %u",
                     options->race_smoke_track);
            ww_game_close(game);
            return false;
        }
        ww_game_play_race_music(game);
        ww_game_start_engine(game);
    } else if (options->duck_smoke_track != 0u) {
        WwRaceSelection selection;
        unsigned track = options->duck_smoke_track;
        if (!ww_race_selection_init(
                &selection, false,
                (track - 1u) / WW_RACES_PER_BOARD,
                (track - 1u) % WW_RACES_PER_BOARD, 1u) ||
            !ww_race_open_duck(&game->race, &game->archive, &selection,
                               0u, true, true)) {
            ww_error("Duck Shoot smoke initialization failed for track %u",
                     track);
            ww_game_close(game);
            return false;
        }
        ww_game_play_race_music(game);
        ww_game_start_engine(game);
    }
    game->smoke_frames = options->smoke_frames;
    game->running = true;
    return true;
}

int ww_game_run(WwGame *game)
{
    while (game->running) {
        unsigned elapsed_136_ticks;
        ww_input_poll(&game->input);
        if (game->input.quit_requested || game->menu.request_quit) {
            game->running = false;
            continue;
        }
        if (game->input.toggle_fullscreen) {
            ww_display_toggle_fullscreen(&game->display);
        }
        elapsed_136_ticks = ww_timing_begin_frame(&game->timing);
        if (game->intro_initialized && !ww_intro_complete(&game->intro)) {
            ww_intro_update(&game->intro, &game->input, elapsed_136_ticks);
            if (ww_intro_complete(&game->intro)) {
                ww_game_play_menu_music(game);
                game->intro_music_handoff = true;
                game->menu.dirty = true;
            } else if (!ww_intro_render(&game->intro)) {
                return 1;
            }
        } else if (game->victory.open) {
            ww_victory_update(&game->victory, &game->input,
                              elapsed_136_ticks);
            if (ww_victory_complete(&game->victory)) {
                if (!ww_game_advance_after_victory(game)) {
                    ww_victory_close(&game->victory);
                    game->menu.request_start_race = false;
                    game->menu.dirty = true;
                    ww_display_set_title(&game->display, "Wacky Wheels 95");
                    ww_game_play_menu_music(game);
                }
            } else if (!ww_victory_render(&game->victory)) {
                return 1;
            }
        } else if (game->race.open) {
            WwRoadDetail detail =
                (WwRoadDetail)game->settings.single_screen_detail;
            if (game->race_paused) {
                if (ww_input_pressed(&game->input, WW_SCAN_ESCAPE)) {
                    game->race_paused = false;
                    ww_game_start_engine(game);
                } else if (ww_input_pressed(&game->input, WW_SCAN_UP)) {
                    game->race_pause_selection =
                        (game->race_pause_selection + 2u) % 3u;
                    ww_menu_play_navigation_sound(&game->menu);
                } else if (ww_input_pressed(&game->input, WW_SCAN_DOWN)) {
                    game->race_pause_selection =
                        (game->race_pause_selection + 1u) % 3u;
                    ww_menu_play_navigation_sound(&game->menu);
                } else if (ww_input_pressed(&game->input, WW_SCAN_ENTER) ||
                           ww_input_pressed(&game->input, WW_SCAN_SPACE)) {
                    if (game->menu.select_sound.samples != NULL) {
                        (void)ww_audio_play(&game->audio,
                                            &game->menu.select_sound,
                                            game->settings.sound_volume);
                    }
                    if (game->race_pause_selection == 0u) {
                        game->race_paused = false;
                        ww_game_start_engine(game);
                    } else if (game->race_pause_selection == 1u) {
                        ww_game_stop_engine(game);
                        ww_race_close(&game->race);
                        game->race_paused = false;
                        game->menu.request_start_race = false;
                        game->menu.request_start_duck = false;
                        game->menu.dirty = true;
                        ww_display_set_title(&game->display,
                                             "Wacky Wheels 95");
                        ww_game_play_menu_music(game);
                    } else {
                        if (!ww_game_restart_race(game)) return 1;
                        game->race_paused = false;
                        ww_game_play_race_music(game);
                        ww_game_start_engine(game);
                    }
                }
                if (game->race.open && game->race_paused) {
                    ww_display_copy_page(&game->display, 0, 1);
                    if (!ww_menu_render_race_pause(
                            &game->menu, game->race_pause_selection)) {
                        return 1;
                    }
                }
            } else if (ww_input_pressed(&game->input, WW_SCAN_ESCAPE)) {
                /* loc_32121 enters sub_155C0 with BACK TO RACE, LEAVE RACE,
                 * and RESTART RACE.  Escape never closes the race directly. */
                game->race_paused = true;
                game->race_pause_selection = 0u;
                ww_game_stop_engine(game);
                if (!ww_race_render_bringup(&game->race, &game->renderer,
                                             &game->display, detail)) {
                    return 1;
                }
                ww_display_copy_page(&game->display, 1, 0);
                if (
                    !ww_menu_render_race_pause(&game->menu, 0u)) {
                    return 1;
                }
            } else {
                WwRacerCrashState old_crash = game->race.racers[0].crash_state;
                /* sub_2FD8C draws first and sub_2F794 advances input/physics
                 * afterward, making the new state visible next frame. */
                if (!ww_race_render_bringup(&game->race, &game->renderer,
                                            &game->display, detail) ||
                    !ww_race_update(&game->race, &game->renderer,
                                    &game->input, detail,
                                    elapsed_136_ticks)) {
                    return 1;
                }
                if (game->race.open &&
                    old_crash == WW_RACER_CRASH_NONE &&
                    game->race.racers[0].crash_state !=
                        WW_RACER_CRASH_NONE &&
                    game->crash_sound.samples != NULL) {
                    (void)ww_audio_play(&game->audio, &game->crash_sound,
                                        game->settings.sound_volume);
                }
                if (game->race.open && game->race.start_sound_pending) {
                    game->race.start_sound_pending = false;
                    if (game->start_sound.samples != NULL) {
                        (void)ww_audio_play(&game->audio,
                                            &game->start_sound,
                                            game->settings.sound_volume);
                    }
                }
                ww_game_update_engine_pitch(game);
                ww_game_update_race_effect_audio(game);
                if (game->race.open && game->race.duck.active &&
                    game->race.duck.score_sound_pending) {
                    game->race.duck.score_sound_pending = false;
                    if (game->crash_sound.samples != NULL) {
                        (void)ww_audio_play(&game->audio,
                                            &game->crash_sound,
                                            game->settings.sound_volume);
                    }
                }
                if (game->race.open && game->race.duck.active &&
                    game->race.duck.finished) {
                    unsigned score = game->race.duck.score;
                    if (ww_profiles_record_score(
                            &game->profiles, "PLAYER", score)) {
                        (void)ww_profiles_save_user(&game->profiles);
                    }
                    ww_game_stop_engine(game);
                    ww_race_close(&game->race);
                    ww_menu_show_duck_results(&game->menu, score,
                                              &game->profiles);
                    ww_display_set_title(&game->display,
                                         "Wacky Wheels 95 - Duck Score");
                    (void)ww_audio_play_midi_asset(
                        &game->audio, &game->archive, "LEADRBRD.MID", true,
                        game->settings.music_volume);
                }
                if (game->race.open &&
                    ww_finish_results_due(&game->race.finish,
                                          game->race.elapsed_136_ticks)) {
                    if (!ww_victory_open(&game->victory, &game->archive,
                                         &game->display, &game->race)) {
                        ww_error("cannot initialize post-race presentation");
                        return 1;
                    }
                    ww_game_stop_engine(game);
                    ww_race_close(&game->race);
                    ww_display_set_title(&game->display,
                                         "Wacky Wheels 95 - Results");
                    (void)ww_audio_play_midi_asset(
                        &game->audio, &game->archive, "LEADRBRD.MID", true,
                        game->settings.music_volume);
                }
                if (!game->race.open && !game->victory.open) {
                    ww_game_stop_engine(game);
                    game->menu.request_start_race = false;
                    game->menu.dirty = true;
                    if (game->menu.screen != WW_MENU_DUCK_RESULTS) {
                        ww_game_play_menu_music(game);
                    }
                }
            }
        } else {
            ww_menu_update(&game->menu, &game->input);
            if (game->menu.request_start_race) {
                char race_title[64];
                if (!ww_race_open(&game->race, &game->archive,
                                  &game->menu.requested_race,
                                  game->menu.selected_vehicle,
                                  game->menu.lap_count,
                                  game->menu.race_class,
                                  game->settings.clock_display,
                                  game->settings.speedometer,
                                  game->settings.overhead_map)) {
                    ww_error("cannot initialize requested track %u",
                             (unsigned)game->menu.requested_race.track_number);
                    return 1;
                }
                game->menu.request_start_race = false;
                game->race_paused = false;
                ww_game_play_race_music(game);
                ww_game_start_engine(game);
                if (ww_race_selection_title(&game->race.selection, race_title,
                                            sizeof(race_title))) {
                    ww_display_set_title(&game->display, race_title);
                }
                if (!ww_race_render_bringup(&game->race, &game->renderer,
                                            &game->display,
                                            (WwRoadDetail)game->settings
                                                .single_screen_detail)) {
                    return 1;
                }
            } else if (game->menu.request_start_duck) {
                if (!ww_race_open_duck(
                        &game->race, &game->archive,
                        &game->menu.requested_race,
                        game->menu.selected_vehicle,
                        game->settings.speedometer,
                        game->settings.overhead_map)) {
                    ww_error("cannot initialize Wacky Duck Shoot track %u",
                             (unsigned)game->menu.requested_race.track_number);
                    return 1;
                }
                game->menu.request_start_duck = false;
                game->race_paused = false;
                ww_game_play_race_music(game);
                ww_game_start_engine(game);
                ww_display_set_title(&game->display,
                                     "Wacky Wheels 95 - Wacky Duck Shoot");
                if (!ww_race_render_bringup(
                        &game->race, &game->renderer, &game->display,
                        (WwRoadDetail)game->settings.single_screen_detail)) {
                    return 1;
                }
            } else if (!ww_menu_render(&game->menu)) {
                return 1;
            }
        }
        if (game->smoke_frames != 0 && --game->smoke_frames == 0) {
            game->running = false;
        }
        ww_timing_idle();
    }
    return 0;
}

void ww_game_close(WwGame *game)
{
    if (game == NULL) {
        return;
    }
    if (game->menu_initialized) {
        ww_menu_close(&game->menu);
    }
    if (game->intro_initialized) {
        ww_intro_close(&game->intro);
    }
    ww_victory_close(&game->victory);
    ww_race_close(&game->race);
    ww_game_stop_engine(game);
    ww_game_stop_water_audio(game);
    ww_audio_free_sound(&game->engine_sound);
    ww_audio_free_sound(&game->crash_sound);
    ww_audio_free_sound(&game->start_sound);
    ww_audio_free_sound(&game->splash_sound);
    ww_audio_free_sound(&game->plip_sound);
    ww_audio_free_sound(&game->horn_sound);
    ww_audio_free_sound(&game->bell_sound);
    game->settings.fullscreen = game->display.fullscreen;
    game->settings.aspect_correct = game->display.aspect_correct;
    (void)ww_settings_save(&game->settings);
    ww_audio_close(&game->audio);
    ww_input_shutdown(&game->input);
    ww_display_close(&game->display);
    if (game->sdl_initialized) {
        SDL_Quit();
    }
    ww_archive_close(&game->archive);
    memset(game, 0, sizeof(*game));
}
