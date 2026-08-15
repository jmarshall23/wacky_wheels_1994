#ifndef WW_LAP_H
#define WW_LAP_H

#include "ww_track.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct WwLapState {
    int16_t current_lap;
    int16_t previous_position;
    bool backwards_finish_crossing;
    bool wrong_way_active;
    uint32_t wrong_way_started_tick;
    bool show_lap_status;
    bool last_lap_alert_pending;
    bool finished;
    uint16_t finish_place;
    uint32_t course_progress;
} WwLapState;

void ww_lap_state_reset(WwLapState *state);
bool ww_lap_update_player(WwLapState *state, const WwTrack *track,
                          uint16_t world_x, uint16_t world_y,
                          unsigned lap_count, bool terrain_collision,
                          uint32_t update_tick,
                          uint16_t *next_finish_place);
bool ww_lap_update_opponent(WwLapState *state, const WwTrack *track,
                            uint16_t world_x, uint16_t world_y,
                            unsigned lap_count, uint16_t *next_finish_place);
void ww_lap_update_ranks(const WwLapState states[], uint8_t ranks[],
                         unsigned racer_count);

#endif
