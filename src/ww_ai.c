#include "ww_ai.h"

#include "ww_physics.h"

#include <stdlib.h>
#include <string.h>

/*
 * The AI call group has not yet been named confidently. Keep this boundary
 * explicit rather than assigning incorrect source names to sub_XXXXX routines.
 */

void ww_ai_reset(void)
{
}

void ww_ai_update_unresolved(const WwVehicleState *vehicle, WwDriverControl *control)
{
    (void)vehicle;
    memset(control, 0, sizeof(*control));
}

static bool ww_ai_load_segment(WwAiPathState *state, const WwTrack *track,
                               uint16_t segment_index)
{
    const WwRoadSegment *segment;
    if (track == NULL || track->road_segment_count == 0 ||
        segment_index >= track->road_segment_count) {
        return false;
    }
    segment = &track->road_segments[segment_index];
    state->segment_index = segment_index;
    state->points_remaining = segment->point_count;
    state->segment_value = (uint16_t)segment->sub_279e8_value_5;
    state->next_point = segment->point_offset;
    return segment->point_count != 0;
}

bool ww_ai_path_begin(WwAiPathState *state, const WwTrack *track,
                      uint16_t segment_index)
{
    if (state == NULL) {
        return false;
    }
    memset(state, 0, sizeof(*state));
    return ww_ai_load_segment(state, track, segment_index);
}

/* Path-pointer/count/segment advancement from the AI branch of sub_16D68. */
bool ww_ai_path_copy_advance(WwAiPathState *destination,
                             const WwAiPathState *source,
                             const WwTrack *track, unsigned steps)
{
    unsigned i;
    if (destination == NULL || source == NULL || track == NULL ||
        track->road_segment_count == 0) {
        return false;
    }
    *destination = *source;
    for (i = 0; i < steps; ++i) {
        uint16_t next_segment;
        if (destination->points_remaining == 0 ||
            destination->next_point >= track->road_point_count) {
            return false;
        }
        destination->target_x = track->road_points[destination->next_point].x;
        destination->target_y = track->road_points[destination->next_point].y;
        ++destination->next_point;
        --destination->points_remaining;
        if (destination->points_remaining != 0) {
            continue;
        }
        next_segment = (uint16_t)(destination->segment_index + 1u);
        if (next_segment >= track->road_segment_count) {
            next_segment = 0;
        }
        if (!ww_ai_load_segment(destination, track, next_segment)) {
            return false;
        }
    }
    return true;
}

/* Normal single-player setup at loc_170F8 starts every CPU racer partway
 * through road segment zero.  The cursor displacement is the absolute Y
 * distance from the segment start; the X displacement is retained at racer
 * offset +14h and decays by four per frame in sub_28CAC. */
bool ww_ai_racer_path_begin(WwAiRacerPathMotion *motion,
                            const WwTrack *track, unsigned racer_index,
                            uint16_t world_x, uint16_t world_y,
                            uint16_t cruise_index,
                            const uint8_t *velocity_table,
                            size_t velocity_table_size)
{
    static const uint16_t approach_countdown[8] = {
        22, 28, 30, 34, 40, 50, 58, 60
    };
    const WwRoadSegment *segment;
    unsigned point_skip;
    int approach;
    int launch;
    if (motion == NULL || track == NULL || track->road_segment_count == 0 ||
        racer_index >= 8u || cruise_index < 64u ||
        velocity_table == NULL ||
        velocity_table_size < WW_PHYSICS_VELOCITY_BYTES) {
        return false;
    }
    segment = &track->road_segments[0];
    point_skip = (unsigned)abs((int)segment->y0 - (int)world_y);
    if (point_skip >= segment->point_count ||
        segment->point_offset + point_skip >= track->road_point_count) {
        return false;
    }
    memset(motion, 0, sizeof(*motion));
    motion->path.segment_index = 0;
    motion->path.points_remaining =
        (uint16_t)(segment->point_count - point_skip);
    motion->path.segment_value = (uint16_t)segment->sub_279e8_value_5;
    motion->path.next_point = segment->point_offset + point_skip;
    motion->lateral_offset = (int16_t)((int)world_x - (int)segment->x0);
    /* +6 is fixed to 0x18 for all racers.  +0e uses this eight-entry table. */
    motion->initial_countdown = 0x18u;
    motion->approach_countdown = approach_countdown[racer_index];
    motion->cruise_steps = ww_read_le16(
        velocity_table + (size_t)cruise_index * 2u);
    /* loc_17319 and loc_17394 iterate racer records from seven down to zero.
     * The prior translation applied their -10 step in the opposite record
     * direction, making the front car launch at 70 instead of 20. */
    approach = (int)cruise_index - 10 + (int)racer_index * 10;
    launch = (int)cruise_index - 74 + (int)racer_index * 10;
    if (approach < 0 || approach >= WW_PHYSICS_VELOCITY_ENTRIES) {
        return false;
    }
    /* loc_30CDA converts +08h (cruise) and +0Ah (approach) through the
     * selected velocity table.  +0Ch (the 24-frame launch burst) remains
     * the raw index installed by loc_17394. */
    motion->approach_steps = ww_read_le16(
        velocity_table + (size_t)approach * 2u);
    motion->launch_steps = (uint16_t)(launch < 1 ? 1 : launch);
    return motion->path.segment_value <= 7u;
}

/* Path-coordinate consumption and the 0->1->2 speed-state progression from
 * sub_28CAC.  The registered game's normal difficulty-4 setup supplies a
 * cruise value of 0x54; the two launch tables are derived from that value in
 * loc_17319/loc_17394.  RD value 5 is the eight-way road direction. */
bool ww_ai_racer_path_step(WwAiRacerPathMotion *motion,
                           const WwTrack *track,
                           uint16_t *world_x, uint16_t *world_y,
                           uint16_t *heading)
{
    WwAiPathState advanced;
    unsigned steps;
    int x;
    if (motion == NULL || track == NULL || world_x == NULL ||
        world_y == NULL || heading == NULL) {
        return false;
    }
    if (motion->speed_mode == 0u) {
        steps = motion->launch_steps;
        if (motion->initial_countdown != 0u) --motion->initial_countdown;
        if (motion->initial_countdown == 0u) motion->speed_mode = 1u;
    } else if (motion->speed_mode == 1u) {
        steps = motion->approach_steps;
        if (motion->approach_countdown != 0u) --motion->approach_countdown;
        if (motion->approach_countdown == 0u) motion->speed_mode = 2u;
    } else {
        steps = motion->cruise_steps;
    }
    if (!ww_ai_path_copy_advance(&advanced, &motion->path, track, steps) ||
        advanced.segment_value > 7u) {
        return false;
    }
    motion->path = advanced;
    x = (int)advanced.target_x + motion->lateral_offset;
    if (x < 0) x = 0;
    if (x > 0xffff) x = 0xffff;
    *world_x = (uint16_t)x;
    *world_y = (uint16_t)advanced.target_y;
    /* word_706A0's table uses RD direction 0 as the neutral frame for a
     * camera octant of 2 (heading 0x1e0). */
    *heading = (uint16_t)(((advanced.segment_value + 2u) & 7u) * 0xf0u);
    if (motion->lateral_offset < 0) {
        motion->lateral_offset = (int16_t)(motion->lateral_offset + 4);
        if (motion->lateral_offset > 0) motion->lateral_offset = 0;
    } else if (motion->lateral_offset > 0) {
        motion->lateral_offset = (int16_t)(motion->lateral_offset - 4);
        if (motion->lateral_offset < 0) motion->lateral_offset = 0;
    }
    return true;
}
