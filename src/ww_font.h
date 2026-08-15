#ifndef WW_FONT_H
#define WW_FONT_H

#include "ww_archive.h"
#include "ww_display.h"

#include <stdbool.h>
#include <stddef.h>

enum {
    WW_MENU_FONT_GLYPHS = 42,
    WW_MENU_FONT_WIDTH = 15,
    WW_MENU_FONT_HEIGHT = 13,
    WW_MENU_FONT_ADVANCE = 11
};

typedef struct WwMenuFont {
    const uint8_t *pixels;
    size_t size;
} WwMenuFont;

bool ww_font_load_menu(WwMenuFont *font, const WwArchive *archive);
int ww_font_menu_glyph(char character);
void ww_font_draw_menu_text(const WwMenuFont *font, WwDisplay *display,
                            int x, int y, const char *text);
int ww_font_menu_text_width(const char *text);

#endif
