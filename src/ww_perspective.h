#ifndef WW_PERSPECTIVE_H
#define WW_PERSPECTIVE_H

#include "ww_renderer.h"
#include "ww_sprite.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct WwPerspectiveProjection {
    int16_t center_x;
    uint16_t distance;
    uint8_t scale_level;
} WwPerspectiveProjection;

bool ww_perspective_project(const WwRenderer *renderer,
                            uint16_t world_x, uint16_t world_y,
                            uint16_t camera_x, uint16_t camera_y,
                            uint16_t camera_heading,
                            WwPerspectiveProjection *projection);

#endif
