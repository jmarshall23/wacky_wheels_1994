#ifndef WW_DISPLAY_H
#define WW_DISPLAY_H

#include "ww_common.h"
#include "ww_pcx.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct WwDisplay {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint8_t pages[WW_DISPLAY_PAGES][WW_SCREEN_WIDTH * WW_SCREEN_HEIGHT];
    uint32_t rgba[WW_SCREEN_WIDTH * WW_SCREEN_HEIGHT];
    uint8_t palette[WW_PALETTE_BYTES];
    unsigned draw_page;
    unsigned visible_page;
    bool fullscreen;
    bool aspect_correct;
} WwDisplay;

bool ww_display_open(WwDisplay *display, const char *title);
void ww_display_close(WwDisplay *display);
void ww_display_set_title(WwDisplay *display, const char *title);
void ww_display_set_palette(WwDisplay *display, const uint8_t palette[WW_PALETTE_BYTES]);
void ww_display_set_vga_palette(WwDisplay *display,
                                const uint8_t palette[WW_PALETTE_BYTES]);
void ww_display_set_draw_page(WwDisplay *display, unsigned page);
void ww_display_set_visible_page(WwDisplay *display, unsigned page);
uint8_t *ww_display_draw_pixels(WwDisplay *display);
void ww_display_clear(WwDisplay *display, uint8_t color);
void ww_display_copy_page(WwDisplay *display, unsigned destination, unsigned source);
void ww_display_put_pixel(WwDisplay *display, int x, int y, uint8_t color);
void ww_display_blit_column_major(WwDisplay *display, int x, int y,
                                  unsigned width, unsigned height,
                                  const uint8_t *source, size_t source_pitch,
                                  int transparent_color);
void ww_display_capture_column_major(const WwDisplay *display, int x, int y,
                                     unsigned width, unsigned height,
                                     uint8_t *destination, size_t destination_pitch);
void ww_display_draw_box(WwDisplay *display, int x, int y,
                         unsigned width, unsigned height,
                         uint8_t edge_color, uint8_t fill_color);
bool ww_display_blit_pcx(WwDisplay *display, const WwPcxImage *image);
bool ww_display_present(WwDisplay *display);
void ww_display_toggle_fullscreen(WwDisplay *display);

#endif
