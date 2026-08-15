#include "ww_perspective.h"

#include "ww_common.h"
#include "ww_fixed_math.h"

#include <limits.h>

enum {
    WW_PERSPECTIVE_NEAR_DISTANCE = 0x50,
    WW_PERSPECTIVE_FAR_DISTANCE = 0x462,
    WW_PERSPECTIVE_PLAYER_DISTANCE = 0x7c,
    WW_PERSPECTIVE_SCALE_DISTANCE_STEP = 0x46,
    WW_PERSPECTIVE_FACTOR = 0x93cd
};

static int32_t ww_perspective_s32_from_bits(uint32_t bits)
{
    if (bits <= INT32_MAX) {
        return (int32_t)bits;
    }
    return -1 - (int32_t)(UINT32_MAX - bits);
}

static int32_t ww_perspective_mul_low(int32_t a, int32_t b)
{
    return ww_perspective_s32_from_bits(
        (uint32_t)((int64_t)a * (int64_t)b));
}

static int32_t ww_perspective_add_low(int32_t a, int32_t b)
{
    return ww_perspective_s32_from_bits((uint32_t)a + (uint32_t)b);
}

static int32_t ww_perspective_sub_low(int32_t a, int32_t b)
{
    return ww_perspective_s32_from_bits((uint32_t)a - (uint32_t)b);
}

static int32_t ww_perspective_sar(int32_t value, unsigned bits)
{
    int64_t magnitude;
    if (value >= 0) {
        return value >> bits;
    }
    magnitude = -(int64_t)value;
    return (int32_t)(-((magnitude + ((INT64_C(1) << bits) - 1)) >> bits));
}

/* Common world-to-screen core repeated by sub_255D4, sub_25A78,
 * sub_2632C, and sub_266A8.  VIEW is the original dword_6B7F8 correction
 * table.  sub_24A08's angular wedge is represented by the forward and final
 * 0..319 screen tests, which use the same +/-0xa0 view rays. */
bool ww_perspective_project(const WwRenderer *renderer,
                            uint16_t world_x, uint16_t world_y,
                            uint16_t camera_x, uint16_t camera_y,
                            uint16_t camera_heading,
                            WwPerspectiveProjection *projection)
{
    const uint8_t *trig;
    int32_t trig_x;
    int32_t trig_y;
    int32_t dx;
    int32_t dy;
    int32_t forward;
    int32_t lateral;
    int32_t denominator;
    int32_t screen_x = 0xa0;
    uint32_t square_sum;
    int32_t distance;
    int32_t view;
    int32_t product;
    int32_t scale_level;

    if (renderer == NULL || projection == NULL ||
        camera_heading >= WW_TRIG_ENTRY_COUNT ||
        renderer->trig_data == NULL || renderer->view_data == NULL) {
        return false;
    }

    trig = renderer->trig_data +
           (size_t)camera_heading * WW_TRIG_ENTRY_BYTES;
    trig_x = (int32_t)ww_read_le32(trig);
    trig_y = (int32_t)ww_read_le32(trig + 4);
    dx = (int16_t)(uint16_t)(world_x - camera_x);
    dy = (int16_t)(uint16_t)(world_y - camera_y);

    forward = ww_perspective_sar(
        ww_perspective_add_low(ww_perspective_mul_low(trig_x, dx),
                               ww_perspective_mul_low(trig_y, dy)),
        16);
    lateral = ww_perspective_sar(
        ww_perspective_sub_low(ww_perspective_mul_low(trig_x, dy),
                               ww_perspective_mul_low(trig_y, dx)),
        16);
    if (forward <= 0) {
        return false;
    }

    denominator = ww_perspective_sar(
        ww_perspective_mul_low(forward, WW_PERSPECTIVE_FACTOR), 16);
    if ((int16_t)dy < (int16_t)world_y) {
        denominator = -denominator;
    }
    if ((int16_t)denominator != 0) {
        int32_t numerator = ww_perspective_mul_low(lateral, 0xa0);
        screen_x -= numerator / (int16_t)denominator;
    }
    if (screen_x < 0 || screen_x >= WW_VIEW_ENTRY_COUNT) {
        return false;
    }

    square_sum = (uint32_t)ww_perspective_mul_low(forward, forward) +
                 (uint32_t)ww_perspective_mul_low(lateral, lateral);
    distance = (int32_t)ww_integer_sqrt(square_sum);
    view = (int32_t)ww_read_le32(renderer->view_data +
                                 (size_t)screen_x * WW_VIEW_ENTRY_BYTES);
    if (view != 0) {
        int32_t remainder;
        product = ww_perspective_mul_low(distance, view);
        distance = ww_perspective_sar(product, 14);
        remainder = product - (distance << 14);
        if (remainder >= 0x1fa0) {
            ++distance;
        }
    }
    if (distance < WW_PERSPECTIVE_NEAR_DISTANCE ||
        distance > WW_PERSPECTIVE_FAR_DISTANCE) {
        return false;
    }

    scale_level = (distance - WW_PERSPECTIVE_PLAYER_DISTANCE) /
                  WW_PERSPECTIVE_SCALE_DISTANCE_STEP;
    if (scale_level < 0) {
        scale_level = 0;
    }
    if (scale_level >= WW_SPRITE_SCALE_LEVELS) {
        scale_level = WW_SPRITE_SCALE_LEVELS - 1;
    }
    projection->center_x = (int16_t)screen_x;
    projection->distance = (uint16_t)distance;
    projection->scale_level = (uint8_t)scale_level;
    return true;
}
