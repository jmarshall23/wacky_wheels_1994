#include "ww_intro.h"

#include "ww_common.h"

#include <stdlib.h>
#include <string.h>

/* sub_348F4 enters these three screens in this order.  Its TaskMan counter
 * runs at 136 Hz; BEAVIS is held for 0x110 ticks and WINTRO begins its title
 * animation after 0x3c ticks. */
static const char *const ww_intro_assets[3] = {
    "APOG1.PCX", "BEAVIS.PCX", "WINTRO.PCX"
};

enum {
    WW_INTRO_WACKY_HOLD_TICKS = 0x3c,
    WW_INTRO_RACER_STEP_TICKS = 9,
    WW_INTRO_RACER_START_DISTANCE = 0x3c,
    WW_INTRO_RACER_ADVANCE = 0x1c,
    WW_INTRO_RACER_ACTIVATE_GAP = 0x190,
    WW_INTRO_RACER_END_DISTANCE = 0x3e8,
    WW_INTRO_RACER_FRAME = 4
};

static bool ww_intro_key_pressed(const WwInput *input)
{
    unsigned scan;
    if (input == NULL) return false;
    for (scan = 0; scan < sizeof(input->pressed); ++scan) {
        if (input->pressed[scan] != 0u) return true;
    }
    return false;
}

bool ww_intro_open(WwIntro *intro, const WwArchive *archive,
                   WwDisplay *display, WwAudio *audio,
                   const WwRenderer *renderer,
                   uint8_t music_volume)
{
    unsigned i;
    WwArchiveView view;
    if (intro == NULL || archive == NULL || display == NULL ||
        renderer == NULL) return false;
    memset(intro, 0, sizeof(*intro));
    intro->archive = archive;
    intro->display = display;
    intro->audio = audio;
    intro->renderer = renderer;
    intro->pass_voice = -1;
    for (i = 0; i < 3u; ++i) {
        if (!ww_archive_view(archive, ww_intro_assets[i], &view) ||
            !ww_pcx_decode(view.data, view.size, &intro->image[i])) {
            ww_error("cannot decode intro screen %s", ww_intro_assets[i]);
            ww_intro_close(intro);
            return false;
        }
    }
    if (!ww_sprite_scale_set_load(&intro->car_scale, archive,
                                  "38X28.INF")) {
        ww_error("cannot load intro car scale 38X28.INF");
        ww_intro_close(intro);
        return false;
    }
    if (!ww_audio_load_voc(archive, "PASS.VOC", &intro->pass_sound)) {
        ww_error("cannot load intro PASS.VOC");
        ww_intro_close(intro);
        return false;
    }
    /* sub_328D4 makes dword_73C08..dword_73C24 point at frame zero of
     * each 12-frame vehicle block in CARS.SP. sub_34208 adds four frames
     * through dword_73C18. The individual animal sheets contain the crash
     * and celebration art at this index, not the complete intro karts. */
    if (!ww_archive_view(archive, "CARS.SP", &view) ||
        view.size != WW_CAR_BYTES) {
        ww_error("cannot load intro CARS.SP");
        ww_intro_close(intro);
        return false;
    }
    for (i = 0; i < 8u; ++i) {
        size_t offset = ((size_t)i * WW_CAR_FRAMES +
                         WW_INTRO_RACER_FRAME) * WW_CAR_SOURCE_BYTES;
        intro->racer[i].source = view.data + offset;
        intro->racer[i].source_size = view.size - offset;
    }
    intro->stage = WW_INTRO_APOGEE;
    intro->open = true;
    intro->dirty = true;
    if (audio != NULL) {
        (void)ww_audio_play_midi_asset(audio, archive, "APOGEE.MID", true,
                                      music_volume);
    }
    return true;
}

static void ww_intro_start_racer_animation(WwIntro *intro)
{
    unsigned i;
    for (i = 0; i < 8u; ++i) {
        intro->racer[i].state = 0u;
        intro->racer[i].distance = WW_INTRO_RACER_START_DISTANCE;
    }
    intro->racer[0].state = 1u;
    intro->last_activated_racer = 0u;
    intro->animation_tick_accumulator = 0u;
    intro->racer_animation_started = true;
    intro->dirty = true;
    if (intro->audio != NULL) {
        intro->pass_voice = ww_audio_play(
            intro->audio, &intro->pass_sound, 255u);
    }
}

static bool ww_intro_step_racers(WwIntro *intro)
{
    unsigned i;
    unsigned next;
    for (i = 0; i < 8u; ++i) {
        WwIntroRacer *racer = &intro->racer[i];
        if (racer->state == 1u) {
            racer->distance = (uint16_t)(racer->distance +
                                         WW_INTRO_RACER_ADVANCE);
            if (racer->distance > WW_INTRO_RACER_END_DISTANCE) {
                racer->state = 2u;
            }
        }
    }
    next = (unsigned)intro->last_activated_racer + 1u;
    if (next < 8u && intro->racer[next].state == 0u) {
        int gap = (int)intro->racer[intro->last_activated_racer].distance -
                  (int)intro->racer[next].distance;
        if (gap < 0) gap = -gap;
        if (gap >= WW_INTRO_RACER_ACTIVATE_GAP) {
            if (intro->audio != NULL) {
                if (intro->pass_voice >= 0) {
                    ww_audio_stop(intro->audio, intro->pass_voice);
                }
                intro->pass_voice = ww_audio_play(
                    intro->audio, &intro->pass_sound, 255u);
            }
            intro->racer[next].state = 1u;
            intro->last_activated_racer = (uint8_t)next;
        }
    }
    intro->dirty = true;
    return intro->racer[7].state == 2u;
}

void ww_intro_close(WwIntro *intro)
{
    unsigned i;
    if (intro == NULL) return;
    if (intro->audio != NULL && intro->pass_voice >= 0) {
        ww_audio_stop(intro->audio, intro->pass_voice);
    }
    ww_audio_free_sound(&intro->pass_sound);
    for (i = 0; i < 3u; ++i) ww_pcx_free(&intro->image[i]);
    memset(intro, 0, sizeof(*intro));
}

void ww_intro_update(WwIntro *intro, const WwInput *input,
                     unsigned elapsed_136_ticks)
{
    unsigned limit;
    if (intro == NULL || !intro->open ||
        intro->stage == WW_INTRO_COMPLETE) {
        return;
    }
    if (ww_intro_key_pressed(input)) {
        intro->stage = WW_INTRO_COMPLETE;
        intro->dirty = false;
        return;
    }
    intro->stage_ticks += elapsed_136_ticks;
    /* APOG1's zoom loop lasts approximately the same interval as the exact
     * 0x110-tick BEAVIS hold.  WINTRO then owns the longer falling-racer
     * title interval before CHECK.PCX is installed. */
    if (intro->stage == WW_INTRO_WACKY) {
        if (!intro->racer_animation_started) {
            if (intro->stage_ticks >= WW_INTRO_WACKY_HOLD_TICKS) {
                intro->stage_ticks -= WW_INTRO_WACKY_HOLD_TICKS;
                ww_intro_start_racer_animation(intro);
            }
            return;
        }
        intro->animation_tick_accumulator += elapsed_136_ticks;
        while (intro->animation_tick_accumulator >=
               WW_INTRO_RACER_STEP_TICKS) {
            intro->animation_tick_accumulator -=
                WW_INTRO_RACER_STEP_TICKS;
            if (ww_intro_step_racers(intro)) {
                intro->stage = WW_INTRO_COMPLETE;
                intro->dirty = false;
                return;
            }
        }
        return;
    }
    limit = 0x110u;
    if (intro->stage_ticks >= limit) {
        intro->stage_ticks = 0;
        intro->stage = (WwIntroStage)(intro->stage + 1);
        /* sub_348F4 replaces APOGEE with dword_7C648 (MAINMENU.MID)
         * immediately before BEAVIS.PCX and retains it through WINTRO. */
        if (intro->stage == WW_INTRO_BEAVIS && !intro->music_handed_off &&
            intro->audio != NULL && intro->archive != NULL) {
            (void)ww_audio_play_midi_asset(
                intro->audio, intro->archive, "MAINMENU.MID", true,
                intro->audio->music_volume);
            intro->music_handed_off = true;
        }
        intro->dirty = intro->stage != WW_INTRO_COMPLETE;
    }
}

static bool ww_intro_draw_racers(WwIntro *intro)
{
    uint8_t *pixels;
    unsigned i;
    if (intro == NULL || intro->renderer == NULL) return false;
    pixels = ww_display_draw_pixels(intro->display);
    if (pixels == NULL) return false;
    for (i = 0; i < 8u; ++i) {
        const WwIntroRacer *racer = &intro->racer[i];
        const WwSpriteScale *scale;
        unsigned level;
        int delta;
        int x;
        int y;
        if (racer->state != 1u ||
            racer->distance >= WW_PROJECTION_TABLE_COUNT) {
            continue;
        }
        delta = (int)racer->distance - 0x7c;
        level = (unsigned)abs(delta / 0x46);
        if (level >= WW_SPRITE_SCALE_LEVELS) {
            level = WW_SPRITE_SCALE_LEVELS - 1u;
        }
        scale = &intro->car_scale.level[level];
        x = 0xa0 - (int)scale->width / 2;
        y = (int)intro->renderer->projection_right[racer->distance] -
            (int)scale->height;
        if (!ww_sprite_draw_scaled_column_major(
                pixels, WW_SCREEN_WIDTH, WW_SCREEN_WIDTH, WW_SCREEN_HEIGHT,
                x, y, racer->source, racer->source_size, scale, 0)) {
            return false;
        }
    }
    return true;
}

bool ww_intro_render(WwIntro *intro)
{
    if (intro == NULL || !intro->open) return false;
    if (intro->stage == WW_INTRO_COMPLETE) return true;
    if (intro->dirty ||
        (intro->stage == WW_INTRO_WACKY &&
         intro->racer_animation_started)) {
        ww_display_set_draw_page(intro->display, 0);
        if (!ww_display_blit_pcx(intro->display,
                                 &intro->image[(unsigned)intro->stage])) {
            return false;
        }
        if (intro->stage == WW_INTRO_WACKY &&
            intro->racer_animation_started &&
            !ww_intro_draw_racers(intro)) {
            return false;
        }
        ww_display_set_visible_page(intro->display, 0);
        intro->dirty = false;
    }
    return ww_display_present(intro->display);
}

bool ww_intro_complete(const WwIntro *intro)
{
    return intro == NULL || !intro->open || intro->stage == WW_INTRO_COMPLETE;
}
