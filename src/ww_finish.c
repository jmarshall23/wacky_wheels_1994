#include "ww_finish.h"

#include <string.h>

void ww_finish_reset(WwFinishState *state)
{
    if (state != NULL) memset(state, 0, sizeof(*state));
}

/* The normal-race finish branch at loc_31609/loc_31773.  word_88C3C moves
 * from zero to one when the player completes lap_count + 1, capturing the
 * 10 Hz dword_7E71C clock.  Once three racers have finished, the class switch
 * builds the eight pending point words consumed later at loc_31CF0. */
bool ww_finish_update(WwFinishState *state,
                      const WwLapState racers[WW_FINISH_RACER_COUNT],
                      unsigned race_class, uint16_t finished_count,
                      uint32_t race_time_tenths,
                      uint32_t elapsed_136_ticks)
{
    static const uint16_t points_by_class[3][3] = {
        {9u, 6u, 3u},
        {12u, 9u, 6u},
        {15u, 12u, 9u}
    };
    unsigned racer_index;
    if (state == NULL || racers == NULL || race_class < 1u ||
        race_class > 5u || finished_count > WW_FINISH_RACER_COUNT) {
        return false;
    }

    if (state->phase == WW_FINISH_RACING && racers[0].finished) {
        state->final_time_tenths = race_time_tenths;
        state->phase = WW_FINISH_PLAYER_FINISHED;
    }
    if (state->phase != WW_FINISH_PLAYER_FINISHED ||
        (finished_count < 3u && race_class != 4u)) {
        return true;
    }

    memset(state->pending_points, 0, sizeof(state->pending_points));
    /* sub_19EAC converts kid mode (selection five) to class one before the
     * race.  Accepting it here as class one keeps this boundary safe for a
     * direct caller while time trial (four) retains its zero point table. */
    if (race_class == 5u) race_class = 1u;
    if (race_class <= 3u) {
        for (racer_index = 0; racer_index < WW_FINISH_RACER_COUNT;
             ++racer_index) {
            uint16_t place = racers[racer_index].finish_place;
            if (place >= 1u && place <= 3u) {
                state->pending_points[racer_index] =
                    points_by_class[race_class - 1u][place - 1u];
            }
        }
    }
    state->points_ready_136_tick = elapsed_136_ticks;
    state->phase = WW_FINISH_POINTS_READY;
    return true;
}

/* loc_31919 waits 0x320 ticks after dword_7EA6C before presenting the
 * post-race choice/results path.  Unsigned subtraction preserves wraparound
 * behavior for the monotonic replacement counter. */
bool ww_finish_results_due(const WwFinishState *state,
                           uint32_t elapsed_136_ticks)
{
    return state != NULL && state->phase == WW_FINISH_POINTS_READY &&
           elapsed_136_ticks - state->points_ready_136_tick >=
               WW_FINISH_RESULTS_DELAY_136_TICKS;
}
