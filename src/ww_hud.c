#include "ww_hud.h"

#include "ww_sprite.h"

#include <stdio.h>
#include <string.h>

typedef struct WwHudRawSprite {
    size_t offset;
    int x;
    int y;
    unsigned width;
    unsigned height;
} WwHudRawSprite;

/* Normal single-player fixed overlays in sub_12F80.  The three source
 * boundaries are constructed from LAP.SP at loc_13DD5: +0x377c, then
 * 38*41, then 65*19, then 61*19. */
static const WwHudRawSprite ww_hud_race_frame[] = {
    {0x377cu, 17, 6, 38, 41},
    {0x3d92u, 68, 6, 65, 19},
    {0x4265u, 146, 6, 61, 19}
};

bool ww_hud_draw_rank(const WwHudAssets *assets, WwDisplay *display,
                      unsigned rank)
{
    enum {
        WW_HUD_RANK_WIDTH = 22,
        WW_HUD_RANK_HEIGHT = 29,
        WW_HUD_RANK_BYTES = 0x27e
    };
    size_t offset;
    if (assets == NULL || !assets->loaded || display == NULL ||
        rank < 1u || rank > 8u) return false;
    offset = (size_t)(rank - 1u) * WW_HUD_RANK_BYTES;
    if (offset + WW_HUD_RANK_BYTES > assets->lap_sprites_size) return false;
    /* sub_2A92C normal-player rank overlay inside the +0x377c panel. */
    ww_display_blit_column_major(display, 25, 12,
                                 WW_HUD_RANK_WIDTH, WW_HUD_RANK_HEIGHT,
                                 assets->lap_sprites + offset,
                                 WW_HUD_RANK_HEIGHT, 0);
    return true;
}

bool ww_hud_draw_wrong_way(const WwHudAssets *assets, WwDisplay *display,
                           bool active, bool *blink_frame)
{
    enum {
        WW_HUD_WRONG_WAY_FIRST = 0x2cdcu,
        WW_HUD_WRONG_WAY_SECOND = 0x322cu,
        WW_HUD_WRONG_WAY_WIDTH = 80,
        WW_HUD_WRONG_WAY_HEIGHT = 17
    };
    size_t offset;
    if (assets == NULL || !assets->loaded || display == NULL ||
        blink_frame == NULL) return false;
    if (!active) return true;
    offset = *blink_frame ? WW_HUD_WRONG_WAY_FIRST
                          : WW_HUD_WRONG_WAY_SECOND;
    *blink_frame = !*blink_frame;
    if (offset + WW_HUD_WRONG_WAY_WIDTH * WW_HUD_WRONG_WAY_HEIGHT >
        assets->lap_sprites_size) return false;
    /* dword_6E720/dword_6E724 alternating branch at loc_2B0A8. */
    ww_display_blit_column_major(display, 120, 182,
                                 WW_HUD_WRONG_WAY_WIDTH,
                                 WW_HUD_WRONG_WAY_HEIGHT,
                                 assets->lap_sprites + offset,
                                 WW_HUD_WRONG_WAY_HEIGHT, 0);
    return true;
}

bool ww_hud_assets_load(WwHudAssets *assets, const WwArchive *archive)
{
    WwHudAssets loaded;
    WwArchiveView view;
    if (assets == NULL || archive == NULL) return false;
    memset(&loaded, 0, sizeof(loaded));
    if (!ww_archive_view(archive, "LAP.SP", &view) ||
        view.size != WW_HUD_LAP_ASSET_BYTES) return false;
    loaded.lap_sprites = view.data;
    loaded.lap_sprites_size = view.size;
    if (!ww_archive_view(archive, "OFONT.SP", &view) ||
        view.size < WW_HUD_OUTPUT_FONT_BYTES) return false;
    /* sub_13A74 requests only 0x0f40 bytes from this larger archive entry. */
    loaded.output_font = view.data;
    loaded.output_font_size = WW_HUD_OUTPUT_FONT_BYTES;
    if (!ww_archive_view(archive, "GENEF.SP", &view) ||
        view.size != WW_HUD_GENERAL_EFFECT_BYTES ||
        WW_HUD_TIMER_GLYPH_OFFSET + 11u * WW_HUD_TIMER_GLYPH_BYTES >
            view.size) {
        return false;
    }
    loaded.general_effects = view.data;
    loaded.general_effects_size = view.size;
    if (!ww_archive_view(archive, "ICONS.SP", &view) ||
        view.size != WW_HUD_ICON_ASSET_BYTES ||
        WW_HUD_SPEEDOMETER_OFFSET + 42u * 19u > view.size ||
        WW_HUD_SPEED_DIGIT_OFFSET + 10u * WW_HUD_SPEED_DIGIT_BYTES >
            view.size) {
        return false;
    }
    loaded.icons = view.data;
    loaded.icons_size = view.size;
    if (!ww_archive_view(archive, "GIGGLES.SP", &view) ||
        view.size != WW_HUD_GIGGLES_BYTES) {
        return false;
    }
    loaded.giggles = view.data;
    loaded.giggles_size = view.size;
    if (!ww_archive_view(archive, "GIG.MOV", &view) ||
        view.size != WW_HUD_GIG_MOV_BYTES || ww_read_le16(view.data) != 80u) {
        return false;
    }
    loaded.gig_movement = view.data;
    loaded.gig_movement_size = view.size;
    if (!ww_archive_view(archive, "PUF.SP", &view) ||
        view.size != WW_HUD_PUFF_BYTES) {
        return false;
    }
    loaded.puff = view.data;
    loaded.puff_size = view.size;
    loaded.loaded = true;
    *assets = loaded;
    return true;
}

bool ww_hud_format_race_time(uint32_t tenths, char text[8])
{
    uint32_t total_seconds;
    uint32_t minutes;
    uint32_t seconds;
    if (text == NULL) return false;
    /* sub_2A618 clears dword_7E71C once the 10 Hz count exceeds 0x8ca0. */
    if (tenths > 0x8ca0u) tenths = 0u;
    total_seconds = tenths / 10u;
    minutes = total_seconds / 60u;
    seconds = total_seconds % 60u;
    text[0] = (char)('0' + minutes / 10u);
    text[1] = (char)('0' + minutes % 10u);
    text[2] = ':';
    text[3] = (char)('0' + seconds / 10u);
    text[4] = (char)('0' + seconds % 10u);
    text[5] = ':';
    text[6] = (char)('0' + tenths % 10u);
    text[7] = '\0';
    return true;
}

bool ww_hud_draw_race_time(const WwHudAssets *assets, WwDisplay *display,
                           uint32_t tenths, bool visible)
{
    char text[8];
    unsigned i;
    if (assets == NULL || !assets->loaded || display == NULL ||
        assets->general_effects == NULL ||
        assets->general_effects_size != WW_HUD_GENERAL_EFFECT_BYTES) {
        return false;
    }
    if (!visible) return true;
    if (!ww_hud_format_race_time(tenths, text)) return false;
    /* sub_2A6F8 uses the GENEF.SP pointer table beginning at character '0'.
     * Each glyph is an exact column-major 7-by-9 slice.  The normal
     * single-screen call at loc_2A9BD places the clock at (0x108, 0x70). */
    for (i = 0; i < 7u; ++i) {
        unsigned glyph = text[i] == ':' ? 10u : (unsigned)(text[i] - '0');
        size_t offset = WW_HUD_TIMER_GLYPH_OFFSET +
                        (size_t)glyph * WW_HUD_TIMER_GLYPH_BYTES;
        ww_display_blit_column_major(
            display, 0x108 + (int)i * 7, 0x70, 7, 9,
            assets->general_effects + offset, 9, 0);
    }
    return true;
}

bool ww_hud_draw_speedometer(const WwHudAssets *assets, WwDisplay *display,
                             int16_t velocity, int16_t *display_speed,
                             bool visible)
{
    int target;
    int shown;
    unsigned tens;
    unsigned ones;
    if (assets == NULL || !assets->loaded || display == NULL ||
        display_speed == NULL || assets->icons == NULL ||
        assets->icons_size != WW_HUD_ICON_ASSET_BYTES) {
        return false;
    }
    shown = *display_speed;
    if (shown < 0) shown = 0;
    if (shown > 99) shown = 99;
    if (visible) {
        tens = (unsigned)shown / 10u;
        ones = (unsigned)shown % 10u;
        /* loc_2ADDA: 42x19 frame at (0,181), then two 10x15 digits at
         * (3,183) and (15,183), all from the exact ICONS.SP slices built at
         * loc_13D21. */
        ww_display_blit_column_major(
            display, 0, 0xb5, 42, 19,
            assets->icons + WW_HUD_SPEEDOMETER_OFFSET, 19, 0);
        ww_display_blit_column_major(
            display, 3, 0xb7, 10, 15,
            assets->icons + WW_HUD_SPEED_DIGIT_OFFSET +
                (size_t)tens * WW_HUD_SPEED_DIGIT_BYTES,
            15, 0);
        ww_display_blit_column_major(
            display, 15, 0xb7, 10, 15,
            assets->icons + WW_HUD_SPEED_DIGIT_OFFSET +
                (size_t)ones * WW_HUD_SPEED_DIGIT_BYTES,
            15, 0);
    }

    /* loc_2AE62 smooths the displayed value toward twice racer +26h:
     * +4 while rising, -10 while falling, with an exact target clamp. */
    target = (int)velocity * 2;
    if ((int)*display_speed < target) {
        int next = (int)*display_speed + 4;
        *display_speed = (int16_t)(next > target ? target : next);
    } else if ((int)*display_speed > target) {
        int next = (int)*display_speed - 10;
        *display_speed = (int16_t)(next < target ? target : next);
    }
    return true;
}

bool ww_hud_draw_ammunition(const WwHudAssets *assets, WwDisplay *display,
                            uint16_t ammunition)
{
    unsigned value;
    unsigned digits[2];
    unsigned i;
    if (assets == NULL || !assets->loaded || display == NULL ||
        WW_HUD_AMMO_DIGIT_OFFSET + 10u * WW_HUD_AMMO_DIGIT_BYTES >
            assets->lap_sprites_size) {
        return false;
    }
    value = ammunition > 99u ? 99u : ammunition;
    digits[0] = value / 10u;
    digits[1] = value % 10u;
    /* loc_2AF64/loc_2B00E: two 10x9 LAP.SP digits at (181,11), with the
     * pointer table rooted at dword_6E380['0']. */
    for (i = 0; i < 2u; ++i) {
        size_t offset = WW_HUD_AMMO_DIGIT_OFFSET +
                        (size_t)digits[i] * WW_HUD_AMMO_DIGIT_BYTES;
        ww_display_blit_column_major(display, 0xb5 + (int)i * 10, 0x0b,
                                     10, 9, assets->lap_sprites + offset,
                                     9, 0);
    }
    return true;
}

/* dword_7EDB4 at loc_2AA15 uses the same two LAP.SP decimal glyphs as the
 * ammunition counter, positioned at (0x1a,0x20). */
bool ww_hud_draw_duck_score(const WwHudAssets *assets, WwDisplay *display,
                            unsigned score)
{
    unsigned digits[2];
    unsigned i;
    if (assets == NULL || !assets->loaded || display == NULL ||
        WW_HUD_AMMO_DIGIT_OFFSET + 10u * WW_HUD_AMMO_DIGIT_BYTES >
            assets->lap_sprites_size) {
        return false;
    }
    if (score > 99u) score = 99u;
    digits[0] = score / 10u;
    digits[1] = score % 10u;
    for (i = 0u; i < 2u; ++i) {
        size_t offset = WW_HUD_AMMO_DIGIT_OFFSET +
                        (size_t)digits[i] * WW_HUD_AMMO_DIGIT_BYTES;
        ww_display_blit_column_major(display, 0x1a + (int)i * 10, 0x20,
                                     10, 9, assets->lap_sprites + offset,
                                     9, 0);
    }
    return true;
}

bool ww_hud_draw_lives(const WwHudAssets *assets, WwDisplay *display,
                       uint16_t lives)
{
    unsigned glyph;
    size_t offset;
    if (assets == NULL || !assets->loaded || display == NULL ||
        WW_HUD_AMMO_DIGIT_OFFSET + 10u * WW_HUD_AMMO_DIGIT_BYTES >
            assets->lap_sprites_size) {
        return false;
    }
    glyph = lives > 9u ? 9u : lives;
    offset = WW_HUD_AMMO_DIGIT_OFFSET +
             (size_t)glyph * WW_HUD_AMMO_DIGIT_BYTES;
    /* sub_29E0C's normal player branch indexes dword_6E440 directly by
     * racer +6Ch.  That field is initialized to three and decremented by
     * loc_314A2 after a completed crash: it is the remaining-life count. */
    ww_display_blit_column_major(display, 0x76, 0x0b, 10, 9,
                                 assets->lap_sprites + offset, 9, 0);
    return true;
}

/* Character cases and indices from sub_148F4.  OFONT.SP contains 61
 * column-major 8-by-8 glyphs; unsupported characters still advance. */
static int ww_hud_output_glyph(char character)
{
    unsigned c = (unsigned char)character;
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z') return (int)(c - 'A');
    if (c >= '0' && c <= '9') return (int)(c - 0x16u);
    switch (c) {
    case '!': return 0x24;
    case '#': return 0x26;
    case '&': return 0x2a;
    case '\'': return 0x39;
    case ')': return 0x2d;
    case ',': return 0x36;
    case '-': return 0x37;
    case '.':
    case ':': return 0x35;
    case '?': return 0x34;
    case '@': return 0x38;
    default: return -1;
    }
}

static void ww_hud_draw_output_text(const WwHudAssets *assets,
                                    WwDisplay *display, int x, int y,
                                    const char *text)
{
    while (*text != '\0') {
        int glyph = ww_hud_output_glyph(*text++);
        if (glyph >= 0) {
            ww_display_blit_column_major(
                display, x, y, 8, 8,
                assets->output_font + (size_t)glyph * 64u, 8, 0);
        }
        x += 8;
    }
}

bool ww_hud_draw_finish_place(const WwHudAssets *assets, WwDisplay *display,
                              const uint8_t *car_sprites,
                              size_t car_sprites_size,
                              unsigned vehicle, unsigned place)
{
    static const char *const labels[3] = {"1ST", "2ND", "3RD"};
    size_t offset;
    int y;
    if (assets == NULL || !assets->loaded || display == NULL ||
        car_sprites == NULL || car_sprites_size != WW_CAR_BYTES ||
        vehicle >= WW_CAR_VEHICLES || place < 1u || place > 3u) {
        return false;
    }
    y = 4 + (int)(place - 1u) * 28;
    offset = (size_t)vehicle * WW_CAR_FRAMES * WW_CAR_SOURCE_BYTES;
    if (offset + WW_CAR_SOURCE_BYTES > car_sprites_size) return false;
    /* sub_12770 writes the ordinal at x=0xf0 and the racer portrait at
     * x=0x10e on both VGA pages.  The SDL frame is rebuilt, so finished
     * entries are redrawn here to preserve the same right-side strip. */
    ww_hud_draw_output_text(assets, display, 0xf0, y, labels[place - 1u]);
    ww_display_blit_column_major(display, 0x10e, y, 38, 28,
                                 car_sprites + offset, 28, 0);
    return true;
}

bool ww_hud_draw_lap_status(const WwHudAssets *assets, WwDisplay *display,
                            unsigned current_lap, unsigned lap_count,
                            bool show_status, bool finished)
{
    char text[16];
    if (assets == NULL || !assets->loaded || display == NULL ||
        assets->output_font == NULL ||
        assets->output_font_size != WW_HUD_OUTPUT_FONT_BYTES) {
        return false;
    }
    if (!show_status) return true;
    if (finished) {
        strcpy(text, "FINISHED!");
    } else if (current_lap >= lap_count) {
        strcpy(text, "LAST LAP!");
    } else {
        (void)snprintf(text, sizeof(text), "LAP %u", current_lap);
    }
    /* Normal single-player branch at loc_2B200. */
    ww_hud_draw_output_text(assets, display, 4, 0xa4, text);
    return true;
}

void ww_hud_lap_alert_reset(WwHudLapAlert *alert)
{
    if (alert != NULL) memset(alert, 0, sizeof(*alert));
}

void ww_hud_lap_alert_begin(WwHudLapAlert *alert)
{
    if (alert == NULL) return;
    memset(alert, 0, sizeof(*alert));
    alert->phase = WW_HUD_LAP_ALERT_DEVIL;
    alert->active = true;
}

/* sub_2A740 selects GIGGLES.SP character seven (the devil), four 38x44
 * frames, and moves it through every coordinate in GIG.MOV.  It then erases
 * the character with PUF.SP frames 3..0 at the same right-side HUD anchor. */
bool ww_hud_draw_lap_alert(const WwHudAssets *assets, WwDisplay *display,
                           const WwHudLapAlert *alert)
{
    enum {
        WW_HUD_DEVIL_CHARACTER = 6,
        WW_HUD_DEVIL_FRAME_BYTES = 0x688,
        WW_HUD_DEVIL_CHARACTER_BYTES = 0x1a20,
        WW_HUD_DEVIL_WIDTH = 38,
        WW_HUD_DEVIL_HEIGHT = 44,
        WW_HUD_PUFF_FRAME_BYTES = 0x428,
        WW_HUD_PUFF_WIDTH = 38,
        WW_HUD_PUFF_HEIGHT = 28
    };
    unsigned motion_count;
    int x;
    int y;
    size_t offset;
    const uint8_t *motion;
    if (assets == NULL || !assets->loaded || display == NULL ||
        alert == NULL) {
        return false;
    }
    if (!alert->active) return true;
    motion_count = ww_read_le16(assets->gig_movement);
    if (alert->motion_frame > motion_count ||
        2u + (size_t)alert->motion_frame * 4u + 4u >
            assets->gig_movement_size || alert->sprite_frame >= 4u) {
        return false;
    }
    motion = assets->gig_movement + 2u +
             (size_t)alert->motion_frame * 4u;
    x = 0xfa + (int16_t)ww_read_le16(motion);
    y = 0x78 + (int16_t)ww_read_le16(motion + 2u);
    if (alert->phase == WW_HUD_LAP_ALERT_DEVIL) {
        offset = WW_HUD_DEVIL_CHARACTER * WW_HUD_DEVIL_CHARACTER_BYTES +
                 (size_t)alert->sprite_frame * WW_HUD_DEVIL_FRAME_BYTES;
        if (offset + WW_HUD_DEVIL_FRAME_BYTES > assets->giggles_size) {
            return false;
        }
        ww_display_blit_column_major(
            display, x, y, WW_HUD_DEVIL_WIDTH, WW_HUD_DEVIL_HEIGHT,
            assets->giggles + offset, WW_HUD_DEVIL_HEIGHT, 0);
    } else {
        offset = (size_t)(3u - alert->sprite_frame) *
                 WW_HUD_PUFF_FRAME_BYTES;
        if (offset + WW_HUD_PUFF_FRAME_BYTES > assets->puff_size) {
            return false;
        }
        ww_display_blit_column_major(
            display, x, y + 8, WW_HUD_PUFF_WIDTH, WW_HUD_PUFF_HEIGHT,
            assets->puff + offset, WW_HUD_PUFF_HEIGHT, 0);
    }
    return true;
}

void ww_hud_lap_alert_step(WwHudLapAlert *alert,
                           const WwHudAssets *assets)
{
    unsigned motion_count;
    if (alert == NULL || assets == NULL || !alert->active ||
        assets->gig_movement == NULL || assets->gig_movement_size < 2u) {
        return;
    }
    motion_count = ww_read_le16(assets->gig_movement);
    if (alert->phase == WW_HUD_LAP_ALERT_DEVIL) {
        if (alert->motion_frame < motion_count) {
            ++alert->motion_frame;
        } else {
            alert->motion_frame = 0u;
            /* loc_2A8C0 still performs the common frame increment in the
             * transition call, so the first subsequently visible PUF frame
             * is index one (reverse source frame two). */
            alert->sprite_frame = 1u;
            alert->phase = WW_HUD_LAP_ALERT_PUFF;
            return;
        }
    }
    ++alert->sprite_frame;
    if (alert->sprite_frame >= 4u) {
        alert->sprite_frame = 0u;
        if (alert->phase == WW_HUD_LAP_ALERT_PUFF) {
            alert->active = false;
        }
    }
}

bool ww_hud_draw_race_frame(const WwHudAssets *assets, WwDisplay *display)
{
    size_t i;
    if (assets == NULL || !assets->loaded || display == NULL) return false;
    for (i = 0; i < sizeof(ww_hud_race_frame) /
                        sizeof(ww_hud_race_frame[0]); ++i) {
        const WwHudRawSprite *sprite = &ww_hud_race_frame[i];
        size_t bytes = (size_t)sprite->width * sprite->height;
        if (sprite->offset + bytes > assets->lap_sprites_size) return false;
        ww_display_blit_column_major(
            display, sprite->x, sprite->y, sprite->width, sprite->height,
            assets->lap_sprites + sprite->offset, sprite->height, 0);
    }
    return true;
}
