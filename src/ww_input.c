#include "ww_input.h"

#include <string.h>

/* Replaces keyboard IRQ sub_40907 and joystick port routines sub_377AE/sub_3784B. */

static int ww_sdl_to_dos_scan(SDL_Scancode scan)
{
    switch (scan) {
    case SDL_SCANCODE_ESCAPE: return WW_SCAN_ESCAPE;
    case SDL_SCANCODE_1: return WW_SCAN_1;
    case SDL_SCANCODE_2: return WW_SCAN_2;
    case SDL_SCANCODE_3: return WW_SCAN_3;
    case SDL_SCANCODE_4: return WW_SCAN_4;
    case SDL_SCANCODE_5: return WW_SCAN_5;
    case SDL_SCANCODE_6: return WW_SCAN_6;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER: return WW_SCAN_ENTER;
    case SDL_SCANCODE_SPACE: return WW_SCAN_SPACE;
    case SDL_SCANCODE_F1: return WW_SCAN_F1;
    case SDL_SCANCODE_F10: return WW_SCAN_F10;
    case SDL_SCANCODE_UP: return WW_SCAN_UP;
    case SDL_SCANCODE_LEFT: return WW_SCAN_LEFT;
    case SDL_SCANCODE_RIGHT: return WW_SCAN_RIGHT;
    case SDL_SCANCODE_DOWN: return WW_SCAN_DOWN;
    default: return -1;
    }
}

void ww_input_init(WwInput *input)
{
    int i;
    memset(input, 0, sizeof(*input));
    for (i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            input->controller = SDL_GameControllerOpen(i);
            if (input->controller != NULL) {
                break;
            }
        }
    }
}

void ww_input_shutdown(WwInput *input)
{
    if (input != NULL && input->controller != NULL) {
        SDL_GameControllerClose(input->controller);
    }
    if (input != NULL) {
        memset(input, 0, sizeof(*input));
    }
}

void ww_input_poll(WwInput *input)
{
    SDL_Event event;
    memset(input->pressed, 0, sizeof(input->pressed));
    input->toggle_fullscreen = false;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            input->quit_requested = true;
        } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            int scan = ww_sdl_to_dos_scan(event.key.keysym.scancode);
            bool down = event.type == SDL_KEYDOWN;
            if (scan >= 0) {
                if (down && !event.key.repeat && !input->keys[scan]) {
                    input->pressed[scan] = 1;
                }
                input->keys[scan] = down ? 1u : 0u;
            }
            if (down && !event.key.repeat && event.key.keysym.scancode == SDL_SCANCODE_F11) {
                input->toggle_fullscreen = true;
            }
        } else if (event.type == SDL_CONTROLLERDEVICEADDED && input->controller == NULL) {
            input->controller = SDL_GameControllerOpen(event.cdevice.which);
        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED && input->controller != NULL) {
            SDL_Joystick *joystick = SDL_GameControllerGetJoystick(input->controller);
            if (SDL_JoystickInstanceID(joystick) == event.cdevice.which) {
                SDL_GameControllerClose(input->controller);
                input->controller = NULL;
            }
        }
    }
    input->steering = 0;
    input->throttle = 0;
    input->fire = ww_input_down(input, WW_SCAN_SPACE) ||
                  ww_input_down(input, WW_SCAN_ENTER);
    if (ww_input_down(input, WW_SCAN_LEFT)) input->steering = -32767;
    if (ww_input_down(input, WW_SCAN_RIGHT)) input->steering = 32767;
    if (ww_input_down(input, WW_SCAN_UP)) input->throttle = 32767;
    if (ww_input_down(input, WW_SCAN_DOWN)) input->throttle = -32767;
    if (input->controller != NULL) {
        input->steering = SDL_GameControllerGetAxis(input->controller,
                                                     SDL_CONTROLLER_AXIS_LEFTX);
        input->throttle = (int16_t)(SDL_GameControllerGetAxis(
            input->controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) -
                                    SDL_GameControllerGetAxis(
                                        input->controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
        input->fire = input->fire || SDL_GameControllerGetButton(
                                        input->controller, SDL_CONTROLLER_BUTTON_A) != 0;
    }
}

bool ww_input_down(const WwInput *input, WwDosScanCode code)
{
    return input != NULL && input->keys[(unsigned)code] != 0;
}

bool ww_input_pressed(const WwInput *input, WwDosScanCode code)
{
    return input != NULL && input->pressed[(unsigned)code] != 0;
}

