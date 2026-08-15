#ifndef WW_PHYSICS_H
#define WW_PHYSICS_H

#include "ww_track.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_PHYSICS_ANGLE_COUNT = 0x780,
    WW_PHYSICS_TRIG_ENTRY_BYTES = 8,
    WW_PHYSICS_VELOCITY_ENTRIES = 200,
    WW_PHYSICS_VELOCITY_BYTES = WW_PHYSICS_VELOCITY_ENTRIES * 2,
    WW_PHYSICS_SPEED_MAX = 100
};

typedef struct WwRect16 {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
} WwRect16;

typedef struct WwPlayerControls {
    bool steer_left;
    bool steer_right;
    bool accelerate;
    bool brake;
} WwPlayerControls;

typedef struct WwPlayerMotion {
    uint16_t camera_x;
    uint16_t camera_y;
    uint16_t heading;
    uint16_t speed_index;
    int16_t velocity;
    uint32_t surface_class;
    uint16_t collision_status;
    bool terrain_collision;
    bool jump_active;
} WwPlayerMotion;

typedef uint16_t (*WwPhysicsCollisionProbe)(void *context,
                                            uint16_t world_x,
                                            uint16_t world_y);

bool ww_physics_rects_overlap(WwRect16 a, WwRect16 b);
bool ww_physics_player_step_unblocked(WwPlayerMotion *motion,
                                      WwPlayerControls controls,
                                      const uint8_t *trig_data,
                                      size_t trig_size,
                                      const uint8_t *ndist_data,
                                      size_t ndist_size,
                                      const uint8_t *velocity_data,
                                      size_t velocity_size,
                                      unsigned detail);
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
                            void *collision_context);
bool ww_physics_player_anchor(const WwPlayerMotion *motion,
                              const uint8_t *trig_data, size_t trig_size,
                              uint16_t *world_x, uint16_t *world_y);
bool ww_physics_offset_point(const uint8_t *trig_data, size_t trig_size,
                             uint16_t origin_x, uint16_t origin_y,
                             uint16_t heading, int32_t distance,
                             uint16_t *world_x, uint16_t *world_y);

#endif
