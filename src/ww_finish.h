#ifndef WW_FINISH_H
#define WW_FINISH_H

#include "ww_lap.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WW_FINISH_RACER_COUNT = 8,
    WW_FINISH_RESULTS_DELAY_136_TICKS = 0x320
};

typedef enum WwFinishPhase {
    WW_FINISH_RACING = 0,
    WW_FINISH_PLAYER_FINISHED = 1,
    WW_FINISH_POINTS_READY = 2
} WwFinishPhase;

typedef struct WwFinishState {
    WwFinishPhase phase;
    uint32_t final_time_tenths;
    uint32_t points_ready_136_tick;
    uint16_t pending_points[WW_FINISH_RACER_COUNT];
} WwFinishState;

void ww_finish_reset(WwFinishState *state);
bool ww_finish_update(WwFinishState *state,
                      const WwLapState racers[WW_FINISH_RACER_COUNT],
                      unsigned race_class, uint16_t finished_count,
                      uint32_t race_time_tenths,
                      uint32_t elapsed_136_ticks);
bool ww_finish_results_due(const WwFinishState *state,
                           uint32_t elapsed_136_ticks);

#endif
