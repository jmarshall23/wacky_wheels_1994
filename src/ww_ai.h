#ifndef WW_AI_H
#define WW_AI_H

#include "ww_fixed_math.h"
#include "ww_track.h"

#include <stdint.h>

typedef struct WwVehicleState {
    WwFixed16 x;
    WwFixed16 y;
    WwFixed16 velocity_x;
    WwFixed16 velocity_y;
    uint16_t heading;
    uint16_t flags;
} WwVehicleState;

typedef struct WwDriverControl {
    int16_t steering;
    int16_t throttle;
    uint8_t fire;
} WwDriverControl;

typedef struct WwAiPathState {
    uint16_t segment_index;
    uint16_t points_remaining;
    uint16_t segment_value;
    uint32_t next_point;
    int16_t target_x;
    int16_t target_y;
} WwAiPathState;

typedef struct WwAiRacerPathMotion {
    WwAiPathState path;
    int16_t lateral_offset;
    uint16_t initial_countdown;
    uint16_t approach_countdown;
    uint16_t speed_mode;
    uint16_t launch_steps;
    uint16_t approach_steps;
    uint16_t cruise_steps;
} WwAiRacerPathMotion;

void ww_ai_reset(void);
void ww_ai_update_unresolved(const WwVehicleState *vehicle, WwDriverControl *control);
bool ww_ai_path_begin(WwAiPathState *state, const WwTrack *track,
                      uint16_t segment_index);
bool ww_ai_path_copy_advance(WwAiPathState *destination,
                             const WwAiPathState *source,
                             const WwTrack *track, unsigned steps);
bool ww_ai_racer_path_begin(WwAiRacerPathMotion *motion,
                            const WwTrack *track, unsigned racer_index,
                            uint16_t world_x, uint16_t world_y,
                            uint16_t cruise_index,
                            const uint8_t *velocity_table,
                            size_t velocity_table_size);
bool ww_ai_racer_path_step(WwAiRacerPathMotion *motion,
                           const WwTrack *track,
                           uint16_t *world_x, uint16_t *world_y,
                           uint16_t *heading);

#endif
