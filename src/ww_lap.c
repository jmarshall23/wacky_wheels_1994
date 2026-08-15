#include "ww_lap.h"

#include <string.h>

void ww_lap_state_reset(WwLapState *state)
{
    if (state != NULL) memset(state, 0, sizeof(*state));
}

static void ww_lap_update_progress(WwLapState *state, int16_t position)
{
    int32_t progress = (int32_t)state->current_lap * 0x400 + position;
    state->course_progress = progress < 0 ? 0u : (uint32_t)progress;
}

/* Normal single-player portion of sub_12B44.  A valid forward lap is the
 * position_count -> 1 transition.  The inverse transition arms +C0h so that
 * immediately recrossing the line cannot award a lap. */
bool ww_lap_update_player(WwLapState *state, const WwTrack *track,
                          uint16_t world_x, uint16_t world_y,
                          unsigned lap_count, bool terrain_collision,
                          uint32_t update_tick,
                          uint16_t *next_finish_place)
{
    int16_t position;
    if (state == NULL || track == NULL || next_finish_place == NULL ||
        lap_count == 0u ||
        track->position_count <= 0) return false;
    if (!ww_track_position_sample(track, world_x, world_y, &position) ||
        position == 0) {
        return true;
    }
    if (state->finished) return true;

    if (position == 1 &&
        state->previous_position == track->position_count) {
        state->wrong_way_active = false;
        if (state->backwards_finish_crossing) {
            state->backwards_finish_crossing = false;
        } else {
            ++state->current_lap;
            if ((unsigned)state->current_lap <= lap_count) {
                state->show_lap_status = true;
            }
            if ((unsigned)state->current_lap == lap_count) {
                state->last_lap_alert_pending = true;
            }
            if ((unsigned)state->current_lap == lap_count + 1u) {
                state->finished = true;
                ++*next_finish_place;
                state->finish_place = *next_finish_place;
            }
        }
    } else if (position == track->position_count &&
               state->previous_position == 1) {
        if (terrain_collision && !state->wrong_way_active) {
            state->wrong_way_started_tick = update_tick;
        }
        if (terrain_collision) state->wrong_way_active = true;
        state->backwards_finish_crossing = true;
    } else if (position < state->previous_position) {
        if (!terrain_collision) {
            if (!state->wrong_way_active) {
                state->wrong_way_started_tick = update_tick;
            }
            state->wrong_way_active = true;
        }
    } else if (state->wrong_way_active &&
               (position > state->previous_position ||
                update_tick - state->wrong_way_started_tick >= 10u)) {
        state->wrong_way_active = false;
    }

    state->previous_position = position;
    ww_lap_update_progress(state, position);
    return true;
}

/* CPU-racer loop at loc_12E86.  It uses the same .POS gate transition but
 * does not carry the player's backwards-crossing suppression state. */
bool ww_lap_update_opponent(WwLapState *state, const WwTrack *track,
                            uint16_t world_x, uint16_t world_y,
                            unsigned lap_count, uint16_t *next_finish_place)
{
    int16_t position;
    if (state == NULL || track == NULL || next_finish_place == NULL ||
        lap_count == 0u || track->position_count <= 0) return false;
    if (!ww_track_position_sample(track, world_x, world_y, &position) ||
        position == 0) {
        return true;
    }
    if (!state->finished && position == 1 &&
        state->previous_position == track->position_count &&
        (unsigned)state->current_lap < lap_count + 1u) {
        ++state->current_lap;
        if ((unsigned)state->current_lap == lap_count + 1u) {
            state->finished = true;
            ++*next_finish_place;
            state->finish_place = *next_finish_place;
        }
    }
    state->previous_position = position;
    ww_lap_update_progress(state, position);
    return true;
}

/* Descending dword_6843A sort in sub_128A4.  If another racer has identical
 * coarse progress, the assembly retains the previous displayed place. */
void ww_lap_update_ranks(const WwLapState states[], uint8_t ranks[],
                         unsigned racer_count)
{
    unsigned order[8];
    unsigned i;
    if (states == NULL || ranks == NULL || racer_count == 0u ||
        racer_count > 8u) return;
    for (i = 0; i < racer_count; ++i) order[i] = i;
    for (i = 0; i + 1u < racer_count; ++i) {
        unsigned j;
        for (j = i + 1u; j < racer_count; ++j) {
            if (states[order[i]].course_progress <
                states[order[j]].course_progress) {
                unsigned swap = order[i];
                order[i] = order[j];
                order[j] = swap;
            }
        }
    }
    for (i = 0; i < racer_count; ++i) {
        unsigned racer = order[i];
        unsigned other;
        bool tied = false;
        for (other = 0; other < racer_count; ++other) {
            if (other != racer &&
                states[other].course_progress ==
                    states[racer].course_progress) {
                tied = true;
                break;
            }
        }
        if (!tied) ranks[racer] = (uint8_t)(i + 1u);
    }
    /* sub_128A4 performs this override after sorting: once +0Ch holds a
     * finish ordinal, the left HUD panel uses that latched value forever. */
    for (i = 0; i < racer_count; ++i) {
        if (states[i].finish_place != 0u) {
            unsigned place = states[i].finish_place;
            if (place < 1u) place = 1u;
            if (place > 8u) place = 8u;
            ranks[i] = (uint8_t)place;
        }
    }
}
