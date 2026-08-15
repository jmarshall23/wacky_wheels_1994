#include "ww_fixed_math.h"

WwFixed16 ww_fixed_from_int(int32_t value)
{
    return (WwFixed16)((int64_t)value * 65536);
}

int32_t ww_fixed_to_int(WwFixed16 value)
{
    return value >> 16;
}

WwFixed16 ww_fixed_mul(WwFixed16 a, WwFixed16 b)
{
    return (WwFixed16)(((int64_t)a * b) >> 16);
}

WwFixed16 ww_fixed_div(WwFixed16 a, WwFixed16 b)
{
    return b == 0 ? 0 : (WwFixed16)(((int64_t)a * 65536) / b);
}

/* Exact rounded integer square-root behavior of sub_249B0. */
uint32_t ww_integer_sqrt(uint32_t value)
{
    uint32_t original = value;
    uint32_t result = 0;
    uint32_t remainder = 0;
    unsigned i;
    if (value <= 1) {
        return value;
    }
    for (i = 0; i < 16; ++i) {
        uint32_t candidate;
        remainder = (remainder << 2) | (value >> 30);
        value <<= 2;
        result <<= 1;
        candidate = (result << 1) + 1;
        if (remainder >= candidate) {
            ++result;
            remainder -= candidate;
        }
    }
    if (original - result * result >= result - 1u) {
        ++result;
    }
    return result;
}
