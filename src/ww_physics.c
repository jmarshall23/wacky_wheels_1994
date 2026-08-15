#include "ww_physics.h"

#include "ww_common.h"

#include <stdlib.h>

static int16_t ww_physics_scaled_direction(const uint8_t *trig_data,
                                           uint16_t heading,
                                           unsigned component,
                                           int32_t scale)
{
    const uint8_t *entry = trig_data +
                           (size_t)heading * WW_PHYSICS_TRIG_ENTRY_BYTES;
    int32_t direction = (int32_t)ww_read_le32(entry + component * 4u);
    int64_t product = (int64_t)direction * scale + 0x8000;
    return (int16_t)(product >> 16);
}

bool ww_physics_offset_point(const uint8_t *trig_data, size_t trig_size,
                             uint16_t origin_x, uint16_t origin_y,
                             uint16_t heading, int32_t distance,
                             uint16_t *world_x, uint16_t *world_y)
{
    if (trig_data == NULL || world_x == NULL || world_y == NULL ||
        heading >= WW_PHYSICS_ANGLE_COUNT ||
        trig_size < WW_PHYSICS_ANGLE_COUNT * WW_PHYSICS_TRIG_ENTRY_BYTES) {
        return false;
    }
    *world_x = (uint16_t)(origin_x + ww_physics_scaled_direction(
        trig_data, heading, 0, distance));
    *world_y = (uint16_t)(origin_y + ww_physics_scaled_direction(
        trig_data, heading, 1, distance));
    return true;
}

static bool ww_physics_validate_player(const WwPlayerMotion *motion,
                                       const uint8_t *trig_data,
                                       size_t trig_size,
                                       const uint8_t *ndist_data,
                                       size_t ndist_size,
                                       const uint8_t *velocity_data,
                                       size_t velocity_size,
                                       unsigned detail)
{
    return motion != NULL && trig_data != NULL && ndist_data != NULL &&
           velocity_data != NULL &&
           trig_size >= WW_PHYSICS_ANGLE_COUNT * WW_PHYSICS_TRIG_ENTRY_BYTES &&
           ndist_size >= 4u && velocity_size >= WW_PHYSICS_VELOCITY_BYTES &&
           motion->heading < WW_PHYSICS_ANGLE_COUNT && detail >= 1u &&
           detail <= 3u && motion->speed_index <= WW_PHYSICS_SPEED_MAX;
}

static void ww_physics_update_speed(WwPlayerMotion *motion,
                                    WwPlayerControls *controls,
                                    const uint8_t *velocity_data)
{
    int speed = motion->speed_index;
    if (controls->brake && speed > 0) {
        speed -= 8;
        if (speed < 0) speed = 0;
        controls->accelerate = false;
    }
    if (controls->accelerate) {
        speed += 10;
        if (speed > WW_PHYSICS_SPEED_MAX) speed = WW_PHYSICS_SPEED_MAX;
    } else {
        speed -= 4;
        if (speed < 0) speed = 0;
    }
    motion->speed_index = (uint16_t)speed;
    motion->velocity =
        (int16_t)ww_read_le16(velocity_data + (size_t)speed * 2u);
}

static void ww_physics_apply_turn(WwPlayerMotion *motion,
                                  WwPlayerControls controls,
                                  const uint8_t *trig_data,
                                  const uint8_t *ndist_data,
                                  unsigned detail)
{
    static const uint16_t turn_by_detail[] = {0, 0x28, 0x24, 0x20};
    uint16_t old_heading = motion->heading;
    uint16_t new_heading = old_heading;
    uint16_t turn = turn_by_detail[detail];
    int32_t pivot;

    if (controls.steer_left) {
        new_heading = old_heading < turn
                          ? (uint16_t)(old_heading +
                                       WW_PHYSICS_ANGLE_COUNT - turn)
                          : (uint16_t)(old_heading - turn);
    }
    /* sub_224EC's right branch is second, so it wins if both are held. */
    if (controls.steer_right) {
        new_heading = (uint16_t)(old_heading + turn);
        if (new_heading >= WW_PHYSICS_ANGLE_COUNT) {
            new_heading = (uint16_t)(new_heading - WW_PHYSICS_ANGLE_COUNT);
        }
    }
    if (new_heading == old_heading) return;

    pivot = (int32_t)ww_read_le32(ndist_data);
    motion->camera_x = (uint16_t)(motion->camera_x +
        ww_physics_scaled_direction(trig_data, old_heading, 0, pivot) -
        ww_physics_scaled_direction(trig_data, new_heading, 0, pivot));
    motion->camera_y = (uint16_t)(motion->camera_y +
        ww_physics_scaled_direction(trig_data, old_heading, 1, pivot) -
        ww_physics_scaled_direction(trig_data, new_heading, 1, pivot));
    motion->heading = new_heading;
}

static bool ww_physics_surface_class(const WwTrack *track, int x, int y,
                                     uint32_t *surface_class)
{
    WwTrackSurfaceSample sample;
    if (x < 0) x = 0;
    if (x > 0xfff) x = 0xfff;
    if (y < 0) y = 0;
    if (y > 0xfff) y = 0xfff;
    if (!ww_track_surface_sample(track, (uint16_t)x, (uint16_t)y, &sample)) {
        return false;
    }
    *surface_class = sample.sub_37afc_value;
    return true;
}

/* Exact positive-velocity terrain resistance in sub_224EC.  The descriptor
 * was selected by sub_237E4 on the preceding movement update, so this uses
 * the player's retained surface class before the new forward probe runs. */
static void ww_physics_apply_surface_drag(WwPlayerMotion *motion,
                                          const WwTrack *track)
{
    uint32_t coefficient;
    int32_t reduction;
    if (motion == NULL || track == NULL || motion->velocity <= 0 ||
        motion->surface_class >= track->surface_class_count) {
        return;
    }
    coefficient = track->surface_drag[motion->surface_class];
    if (coefficient == 0u) return;
    reduction = (int32_t)(((int64_t)motion->velocity * coefficient) >> 16);
    motion->velocity = (int16_t)(motion->velocity - reduction);
}

bool ww_physics_player_anchor(const WwPlayerMotion *motion,
                              const uint8_t *trig_data, size_t trig_size,
                              uint16_t *world_x, uint16_t *world_y)
{
    if (motion == NULL || trig_data == NULL || world_x == NULL ||
        world_y == NULL || motion->heading >= WW_PHYSICS_ANGLE_COUNT ||
        trig_size < WW_PHYSICS_ANGLE_COUNT * WW_PHYSICS_TRIG_ENTRY_BYTES) {
        return false;
    }
    return ww_physics_offset_point(trig_data, trig_size,
                                   motion->camera_x, motion->camera_y,
                                   motion->heading, 0x7c,
                                   world_x, world_y);
}

/* Inclusive rectangle collision test from sub_36D69. */
bool ww_physics_rects_overlap(WwRect16 a, WwRect16 b)
{
    int a_right;
    int b_right;
    int a_bottom;
    int b_bottom;
    if (a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0) {
        return false;
    }
    a_right = (int)a.x + a.width - 1;
    b_right = (int)b.x + b.width - 1;
    if (a.x > b_right || b.x > a_right) {
        return false;
    }
    a_bottom = (int)a.y + a.height - 1;
    b_bottom = (int)b.y + b.height - 1;
    return a.y <= b_bottom && b.y <= a_bottom;
}

/* Unobstructed player-motion path through sub_290A0, sub_224EC, and
 * sub_237E4.  sub_21DE0 normally walks the forward line one pixel at a time
 * and stops it on collision; when it finds no collision, the endpoint is the
 * same rounded TRIG.DAT displacement calculated here.  The steering pivot is
 * NDIST[0], which is 133 in the registered archive. */
bool ww_physics_player_step_unblocked(WwPlayerMotion *motion,
                                      WwPlayerControls controls,
                                      const uint8_t *trig_data,
                                      size_t trig_size,
                                      const uint8_t *ndist_data,
                                      size_t ndist_size,
                                      const uint8_t *velocity_data,
                                      size_t velocity_size,
                                      unsigned detail)
{
    uint16_t old_heading;

    if (!ww_physics_validate_player(motion, trig_data, trig_size, ndist_data,
                                    ndist_size, velocity_data, velocity_size,
                                    detail)) {
        return false;
    }

    /* sub_290A0 applies the brake first.  It clears acceleration, then its
     * ordinary coasting branch removes another four speed-index units. */
    if (!motion->jump_active) {
        ww_physics_update_speed(motion, &controls, velocity_data);
    }

    old_heading = motion->heading;
    motion->camera_x = (uint16_t)(motion->camera_x +
        ww_physics_scaled_direction(trig_data, old_heading, 0,
                                    motion->velocity));
    motion->camera_y = (uint16_t)(motion->camera_y +
        ww_physics_scaled_direction(trig_data, old_heading, 1,
                                    motion->velocity));

    /* sub_237E4 continues through the word_88B92/word_88B90 steering path
     * while racer +0Ah marks a flight.  Flight freezes speed above, not
     * heading. */
    ww_physics_apply_turn(motion, controls, trig_data, ndist_data, detail);
    return true;
}

/* sub_237E4 builds a camera-motion line, and sub_21DE0 walks every pixel in
 * the same strict Bresenham order as sub_21554.  sub_216A0 samples the point
 * 0x7c units in front of the camera and stops on surface class 3.  The class
 * 3 branch then backs the camera 0x20 units away before skipping steering for
 * that frame.  Status four is the class-0x0a jump trigger; object/racer
 * collision statuses two and three remain separate consumers. */
bool ww_physics_player_step(WwPlayerMotion *motion,
                            WwPlayerControls controls,
                            const WwTrack *track,
                            const uint8_t *trig_data,
                            size_t trig_size,
                            const uint8_t *ndist_data,
                            size_t ndist_size,
                            const uint8_t *velocity_data,
                            size_t velocity_size,
                            unsigned detail,
                            WwPhysicsCollisionProbe collision_probe,
                            void *collision_context)
{
    WwTrackPoint points[WW_PHYSICS_SPEED_MAX + 2u];
    uint16_t old_heading;
    int16_t front_x;
    int16_t front_y;
    int16_t move_x;
    int16_t move_y;
    int16_t start_x;
    int16_t start_y;
    int16_t end_x;
    int16_t end_y;
    size_t count;
    size_t i;

    if (track == NULL ||
        !ww_physics_validate_player(motion, trig_data, trig_size, ndist_data,
                                    ndist_size, velocity_data, velocity_size,
                                    detail)) {
        return false;
    }
    if (!motion->jump_active) {
        ww_physics_update_speed(motion, &controls, velocity_data);
        ww_physics_apply_surface_drag(motion, track);
    }
    old_heading = motion->heading;
    front_x = ww_physics_scaled_direction(trig_data, old_heading, 0, 0x7c);
    front_y = ww_physics_scaled_direction(trig_data, old_heading, 1, 0x7c);
    move_x = ww_physics_scaled_direction(trig_data, old_heading, 0,
                                         motion->velocity);
    move_y = ww_physics_scaled_direction(trig_data, old_heading, 1,
                                         motion->velocity);
    start_x = (int16_t)motion->camera_x;
    start_y = (int16_t)motion->camera_y;
    end_x = (int16_t)(start_x + move_x);
    end_y = (int16_t)(start_y + move_y);
    count = ww_track_rasterize_line(start_x, start_y, end_x, end_y,
                                    points, sizeof(points) / sizeof(points[0]));
    if (count == 0) return false;

    motion->terrain_collision = false;
    motion->collision_status = 0u;
    for (i = 0; i < count; ++i) {
        uint32_t surface_class;
        if (!ww_physics_surface_class(track,
                                      (int)points[i].x + front_x,
                                      (int)points[i].y + front_y,
                                      &surface_class)) {
            return false;
        }
        motion->camera_x = (uint16_t)points[i].x;
        motion->camera_y = (uint16_t)points[i].y;
        motion->surface_class = surface_class;
        if (surface_class == 0x0au && !motion->jump_active) {
            motion->collision_status = 4u;
            break;
        }
        if (surface_class == 3u) {
            uint16_t reverse_heading = (uint16_t)(
                old_heading + WW_PHYSICS_ANGLE_COUNT / 2u);
            if (reverse_heading >= WW_PHYSICS_ANGLE_COUNT) {
                reverse_heading = (uint16_t)(reverse_heading -
                                             WW_PHYSICS_ANGLE_COUNT);
            }
            motion->camera_x = (uint16_t)(motion->camera_x +
                ww_physics_scaled_direction(trig_data, reverse_heading,
                                            0, 0x20));
            motion->camera_y = (uint16_t)(motion->camera_y +
                ww_physics_scaled_direction(trig_data, reverse_heading,
                                            1, 0x20));
            motion->terrain_collision = true;
            motion->collision_status = 1u;
            return true;
        }
        if (collision_probe != NULL) {
            uint16_t collision = collision_probe(
                collision_context,
                (uint16_t)((int)points[i].x + front_x),
                (uint16_t)((int)points[i].y + front_y));
            if (collision != 0u) {
                motion->collision_status = collision;
                /* sub_21DE0 stops its line walker on the first occupied
                 * candidate.  Keep the last admitted point, not the contact
                 * point, so a fixed-step retry cannot bury the player in a
                 * pillar or another racer. */
                if (i != 0u) {
                    motion->camera_x = (uint16_t)points[i - 1u].x;
                    motion->camera_y = (uint16_t)points[i - 1u].y;
                } else {
                    motion->camera_x = (uint16_t)start_x;
                    motion->camera_y = (uint16_t)start_y;
                }
                break;
            }
        }
    }

    /* sub_237E4 continues steering while racer +0Ah marks a flight. */
    ww_physics_apply_turn(motion, controls, trig_data, ndist_data, detail);
    return true;
}
