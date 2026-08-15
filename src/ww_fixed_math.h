#ifndef WW_FIXED_MATH_H
#define WW_FIXED_MATH_H

#include <stdint.h>

typedef int32_t WwFixed16;

#define WW_FIXED_ONE ((WwFixed16)0x10000)

WwFixed16 ww_fixed_from_int(int32_t value);
int32_t ww_fixed_to_int(WwFixed16 value);
WwFixed16 ww_fixed_mul(WwFixed16 a, WwFixed16 b);
WwFixed16 ww_fixed_div(WwFixed16 a, WwFixed16 b);
uint32_t ww_integer_sqrt(uint32_t value);

#endif

