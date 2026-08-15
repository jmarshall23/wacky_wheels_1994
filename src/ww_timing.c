#include "ww_timing.h"

#include <SDL.h>

/* Replaces TaskMan registrations created through sub_40E38 in main.  The
 * race loop at loc_31021 snapshots the 136 Hz counter and loc_31111 waits
 * until dword_7EA5C ticks have elapsed.  sub_16CA0 copies the normal default
 * dword_7ED08 value (0x0c) into that limit. */

void ww_timing_init(WwTiming *timing)
{
    timing->frequency = SDL_GetPerformanceFrequency();
    timing->previous_counter = SDL_GetPerformanceCounter();
    timing->accumulator = 0.0;
    timing->tick_136 = 0;
    timing->tick_10 = 0;
    timing->tick_1 = 0;
}

unsigned ww_timing_begin_frame(WwTiming *timing)
{
    uint64_t now;
    uint64_t target;
    uint64_t elapsed_counter;
    double elapsed;
    unsigned ticks;

    if (timing == NULL || timing->frequency == 0) {
        return 0;
    }

    target = timing->previous_counter +
             timing->frequency * WW_GAME_FRAME_TICKS / WW_GAME_TICK_HZ;
    now = SDL_GetPerformanceCounter();
    while (now < target) {
        uint64_t remaining = target - now;
        uint64_t milliseconds = remaining * 1000u / timing->frequency;
        SDL_Delay(milliseconds > 1u ? (Uint32)(milliseconds - 1u) : 0u);
        now = SDL_GetPerformanceCounter();
    }

    elapsed_counter = now - timing->previous_counter;
    elapsed = (double)elapsed_counter / (double)timing->frequency;
    timing->previous_counter = now;
    if (elapsed > 0.25) {
        elapsed = 0.25;
    }
    timing->accumulator += elapsed * WW_GAME_TICK_HZ;
    ticks = (unsigned)timing->accumulator;
    /* Integer deadline rounding can report eleven ticks even though the
     * loc_31111 wait has completed its twelve-tick frame.  The DOS counter
     * snapshot advances by the full configured frame quantum here. */
    if (ticks < WW_GAME_FRAME_TICKS) ticks = WW_GAME_FRAME_TICKS;
    timing->accumulator -= ticks;
    timing->tick_136 += ticks;
    timing->tick_10 = timing->tick_136 * 10u / WW_GAME_TICK_HZ;
    timing->tick_1 = timing->tick_136 / WW_GAME_TICK_HZ;
    return ticks;
}

void ww_timing_idle(void)
{
    SDL_Delay(0);
}
