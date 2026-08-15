#include "ww_font.h"

#include <string.h>

bool ww_font_load_menu(WwMenuFont *font, const WwArchive *archive)
{
    WwArchiveView view;
    if (font == NULL || !ww_archive_view(archive, "WFONT1.SP", &view) ||
        view.size != WW_MENU_FONT_GLYPHS * WW_MENU_FONT_WIDTH *
                         WW_MENU_FONT_HEIGHT) {
        return false;
    }
    font->pixels = view.data;
    font->size = view.size;
    return true;
}

/* Character-to-sprite mapping from sub_146D4. */
int ww_font_menu_glyph(char character)
{
    unsigned c = (unsigned char)character;
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z') return (int)(c - 'A');
    if (c >= '0' && c <= '9') return (int)(c - 0x16u);
    switch (c) {
    case ' ': return 0x24;
    case '.': return 0x25;
    case '<': return 0x26;
    case '#':
    case '>': return 0x27;
    case '!': return 0x28;
    case '?':
    case '~': return 0x29;
    default: return 0x24;
    }
}

void ww_font_draw_menu_text(const WwMenuFont *font, WwDisplay *display,
                            int x, int y, const char *text)
{
    if (font == NULL || font->pixels == NULL || display == NULL || text == NULL) {
        return;
    }
    while (*text != '\0') {
        int glyph = ww_font_menu_glyph(*text++);
        const uint8_t *pixels = font->pixels +
            (size_t)glyph * WW_MENU_FONT_WIDTH * WW_MENU_FONT_HEIGHT;
        ww_display_blit_column_major(display, x, y,
                                     WW_MENU_FONT_WIDTH, WW_MENU_FONT_HEIGHT,
                                     pixels, WW_MENU_FONT_HEIGHT, 0);
        x += WW_MENU_FONT_ADVANCE;
    }
}

int ww_font_menu_text_width(const char *text)
{
    return text == NULL ? 0 : (int)strlen(text) * WW_MENU_FONT_ADVANCE;
}
