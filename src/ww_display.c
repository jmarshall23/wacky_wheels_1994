#include "ww_display.h"

#include <string.h>

/*
 * Original display boundary:
 *   sub_248D0  BIOS mode setup
 *   sub_36CCA  VGA palette read
 *   sub_36E41  Mode X setup
 *   sub_36F14  page copy
 *   sub_375CD  PCX RLE draw
 *   sub_37CF1  VGA palette write
 *   sub_37DE0  CRTC page flip
 *
 * The DOS code stores four 320x200 pages as four VGA planes with 80 bytes per
 * scanline. The port deliberately exposes the same page-level behavior using
 * chunky indexed pixels. This keeps game code palette-indexed while removing
 * all privileged VGA operations.
 */

bool ww_display_open(WwDisplay *display, const char *title)
{
    if (display == NULL) {
        return false;
    }
    memset(display, 0, sizeof(*display));
    display->aspect_correct = true;
    display->window = SDL_CreateWindow(title,
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       960, 720,
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (display->window == NULL) {
        ww_error("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    display->renderer = SDL_CreateRenderer(display->window, -1,
                                           SDL_RENDERER_ACCELERATED |
                                               SDL_RENDERER_PRESENTVSYNC);
    if (display->renderer == NULL) {
        display->renderer = SDL_CreateRenderer(display->window, -1,
                                               SDL_RENDERER_SOFTWARE);
    }
    if (display->renderer == NULL) {
        ww_error("SDL_CreateRenderer failed: %s", SDL_GetError());
        ww_display_close(display);
        return false;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    display->texture = SDL_CreateTexture(display->renderer,
                                         SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         WW_SCREEN_WIDTH,
                                         WW_SCREEN_HEIGHT);
    if (display->texture == NULL) {
        ww_error("SDL_CreateTexture failed: %s", SDL_GetError());
        ww_display_close(display);
        return false;
    }
    SDL_SetTextureBlendMode(display->texture, SDL_BLENDMODE_NONE);
    return true;
}

void ww_display_close(WwDisplay *display)
{
    if (display == NULL) {
        return;
    }
    if (display->texture != NULL) {
        SDL_DestroyTexture(display->texture);
    }
    if (display->renderer != NULL) {
        SDL_DestroyRenderer(display->renderer);
    }
    if (display->window != NULL) {
        SDL_DestroyWindow(display->window);
    }
    memset(display, 0, sizeof(*display));
}

void ww_display_set_title(WwDisplay *display, const char *title)
{
    if (display != NULL && display->window != NULL && title != NULL) {
        SDL_SetWindowTitle(display->window, title);
    }
}

void ww_display_set_palette(WwDisplay *display, const uint8_t palette[WW_PALETTE_BYTES])
{
    if (display != NULL && palette != NULL) {
        memcpy(display->palette, palette, WW_PALETTE_BYTES);
    }
}

void ww_display_set_vga_palette(WwDisplay *display,
                                const uint8_t palette[WW_PALETTE_BYTES])
{
    unsigned i;
    if (display == NULL || palette == NULL) {
        return;
    }
    /* VGA DAC components are six-bit; PCX components are eight-bit. */
    for (i = 0; i < WW_PALETTE_BYTES; ++i) {
        display->palette[i] = (uint8_t)((palette[i] & 0x3fu) << 2);
    }
}

void ww_display_set_draw_page(WwDisplay *display, unsigned page)
{
    if (display != NULL) {
        display->draw_page = page % WW_DISPLAY_PAGES;
    }
}

void ww_display_set_visible_page(WwDisplay *display, unsigned page)
{
    if (display != NULL) {
        display->visible_page = page % WW_DISPLAY_PAGES;
    }
}

uint8_t *ww_display_draw_pixels(WwDisplay *display)
{
    return display == NULL ? NULL : display->pages[display->draw_page];
}

void ww_display_clear(WwDisplay *display, uint8_t color)
{
    uint8_t *pixels = ww_display_draw_pixels(display);
    if (pixels != NULL) {
        memset(pixels, color, WW_SCREEN_WIDTH * WW_SCREEN_HEIGHT);
    }
}

void ww_display_copy_page(WwDisplay *display, unsigned destination, unsigned source)
{
    if (display == NULL) {
        return;
    }
    destination %= WW_DISPLAY_PAGES;
    source %= WW_DISPLAY_PAGES;
    memcpy(display->pages[destination], display->pages[source],
           WW_SCREEN_WIDTH * WW_SCREEN_HEIGHT);
}

/* Chunky equivalents of the Mode X column blitters at sub_36FD0, sub_3707E,
 * sub_37132 and sub_373EE.  Their source images are stored by column, not by
 * scanline.  A negative transparent_color selects the opaque path. */
void ww_display_blit_column_major(WwDisplay *display, int x, int y,
                                  unsigned width, unsigned height,
                                  const uint8_t *source, size_t source_pitch,
                                  int transparent_color)
{
    uint8_t *pixels;
    unsigned column;
    if (display == NULL || source == NULL || source_pitch < height) {
        return;
    }
    pixels = ww_display_draw_pixels(display);
    for (column = 0; column < width; ++column) {
        unsigned row;
        int destination_x = x + (int)column;
        const uint8_t *source_column = source + (size_t)column * source_pitch;
        if (destination_x < 0 || destination_x >= WW_SCREEN_WIDTH) {
            continue;
        }
        for (row = 0; row < height; ++row) {
            int destination_y = y + (int)row;
            uint8_t color = source_column[row];
            if (destination_y >= 0 && destination_y < WW_SCREEN_HEIGHT &&
                (transparent_color < 0 || color != (uint8_t)transparent_color)) {
                pixels[(size_t)destination_y * WW_SCREEN_WIDTH + destination_x] = color;
            }
        }
    }
}

/* Chunky equivalent of sub_372C7. */
void ww_display_capture_column_major(const WwDisplay *display, int x, int y,
                                     unsigned width, unsigned height,
                                     uint8_t *destination, size_t destination_pitch)
{
    const uint8_t *pixels;
    unsigned column;
    if (display == NULL || destination == NULL || destination_pitch < height) {
        return;
    }
    pixels = display->pages[display->draw_page];
    for (column = 0; column < width; ++column) {
        unsigned row;
        for (row = 0; row < height; ++row) {
            int source_x = x + (int)column;
            int source_y = y + (int)row;
            destination[(size_t)column * destination_pitch + row] =
                source_x >= 0 && source_x < WW_SCREEN_WIDTH &&
                        source_y >= 0 && source_y < WW_SCREEN_HEIGHT
                    ? pixels[(size_t)source_y * WW_SCREEN_WIDTH + source_x]
                    : 0;
        }
    }
}

/* Single-pixel replacement for sub_37BF0. */
void ww_display_put_pixel(WwDisplay *display, int x, int y, uint8_t color)
{
    if (display != NULL && x >= 0 && x < WW_SCREEN_WIDTH &&
        y >= 0 && y < WW_SCREEN_HEIGHT) {
        display->pages[display->draw_page][(size_t)y * WW_SCREEN_WIDTH + x] = color;
    }
}

/* Rectangle behavior of sub_371E4: a solid edge on the first/last rows and
 * columns with a separately colored interior. */
void ww_display_draw_box(WwDisplay *display, int x, int y,
                         unsigned width, unsigned height,
                         uint8_t edge_color, uint8_t fill_color)
{
    unsigned column;
    if (display == NULL || width == 0 || height == 0) {
        return;
    }
    for (column = 0; column < width; ++column) {
        unsigned row;
        for (row = 0; row < height; ++row) {
            bool edge = column == 0 || column + 1 == width ||
                        row == 0 || row + 1 == height;
            ww_display_put_pixel(display, x + (int)column, y + (int)row,
                                 edge ? edge_color : fill_color);
        }
    }
}

bool ww_display_blit_pcx(WwDisplay *display, const WwPcxImage *image)
{
    if (display == NULL || image == NULL || image->pixels == NULL ||
        image->width != WW_SCREEN_WIDTH || image->height != WW_SCREEN_HEIGHT) {
        return false;
    }
    memcpy(ww_display_draw_pixels(display), image->pixels,
           WW_SCREEN_WIDTH * WW_SCREEN_HEIGHT);
    ww_display_set_palette(display, image->palette);
    return true;
}

bool ww_display_present(WwDisplay *display)
{
    const uint8_t *indexed;
    size_t i;
    int output_width;
    int output_height;
    SDL_Rect destination;

    if (display == NULL || display->texture == NULL || display->renderer == NULL) {
        return false;
    }
    indexed = display->pages[display->visible_page];
    for (i = 0; i < WW_SCREEN_WIDTH * WW_SCREEN_HEIGHT; ++i) {
        unsigned color = indexed[i];
        unsigned p = color * 3u;
        display->rgba[i] = 0xff000000u |
                           ((uint32_t)display->palette[p] << 16) |
                           ((uint32_t)display->palette[p + 1] << 8) |
                           display->palette[p + 2];
    }
    if (SDL_UpdateTexture(display->texture, NULL, display->rgba,
                          WW_SCREEN_WIDTH * (int)sizeof(uint32_t)) != 0) {
        ww_error("SDL_UpdateTexture failed: %s", SDL_GetError());
        return false;
    }
    SDL_GetRendererOutputSize(display->renderer, &output_width, &output_height);
    destination.x = 0;
    destination.y = 0;
    destination.w = output_width;
    destination.h = output_height;
    if (!display->aspect_correct) {
        const float source_aspect = (float)WW_SCREEN_WIDTH / WW_SCREEN_HEIGHT;
        const float output_aspect = output_height == 0 ? source_aspect
                                                        : (float)output_width / output_height;
        if (output_aspect > source_aspect) {
            destination.w = (int)(output_height * source_aspect);
            destination.x = (output_width - destination.w) / 2;
        } else {
            destination.h = (int)(output_width / source_aspect);
            destination.y = (output_height - destination.h) / 2;
        }
    }
    SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 255);
    SDL_RenderClear(display->renderer);
    if (SDL_RenderCopy(display->renderer, display->texture, NULL, &destination) != 0) {
        return false;
    }
    SDL_RenderPresent(display->renderer);
    return true;
}

void ww_display_toggle_fullscreen(WwDisplay *display)
{
    if (display == NULL || display->window == NULL) {
        return;
    }
    display->fullscreen = !display->fullscreen;
    SDL_SetWindowFullscreen(display->window,
                            display->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}
