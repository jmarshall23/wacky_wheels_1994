#ifndef WW_TIMING_H
#define WW_TIMING_H

#include "ww_common.h"

#include <stdint.h>

typedef struct WwTiming {
    uint64_t frequency;
    uint64_t previous_counter;
    double accumulator;
    uint64_t tick_136;
    uint64_t tick_10;
    uint64_t tick_1;
} WwTiming;

void ww_timing_init(WwTiming *timing);
unsigned ww_timing_begin_frame(WwTiming *timing);
void ww_timing_idle(void);

#endif

