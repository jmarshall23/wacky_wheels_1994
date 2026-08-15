#ifndef WW_INPUT_H
#define WW_INPUT_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum WwDosScanCode {
    WW_SCAN_ESCAPE = 0x01,
    WW_SCAN_1 = 0x02,
    WW_SCAN_2 = 0x03,
    WW_SCAN_3 = 0x04,
    WW_SCAN_4 = 0x05,
    WW_SCAN_5 = 0x06,
    WW_SCAN_6 = 0x07,
    WW_SCAN_ENTER = 0x1c,
    WW_SCAN_SPACE = 0x39,
    WW_SCAN_F1 = 0x3b,
    WW_SCAN_F10 = 0x44,
    WW_SCAN_UP = 0x48,
    WW_SCAN_LEFT = 0x4b,
    WW_SCAN_RIGHT = 0x4d,
    WW_SCAN_DOWN = 0x50
} WwDosScanCode;

typedef struct WwInput {
    uint8_t keys[128];
    uint8_t pressed[128];
    bool quit_requested;
    bool toggle_fullscreen;
    SDL_GameController *controller;
    int16_t steering;
    int16_t throttle;
    bool fire;
} WwInput;

void ww_input_init(WwInput *input);
void ww_input_shutdown(WwInput *input);
void ww_input_poll(WwInput *input);
bool ww_input_down(const WwInput *input, WwDosScanCode code);
bool ww_input_pressed(const WwInput *input, WwDosScanCode code);

#endif

