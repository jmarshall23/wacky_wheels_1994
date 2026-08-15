#ifndef WW_INTRO_H
#define WW_INTRO_H

#include "ww_archive.h"
#include "ww_audio.h"
#include "ww_display.h"
#include "ww_input.h"
#include "ww_pcx.h"
#include "ww_renderer.h"
#include "ww_sprite.h"

#include <stdbool.h>

typedef enum WwIntroStage {
    WW_INTRO_APOGEE,
    WW_INTRO_BEAVIS,
    WW_INTRO_WACKY,
    WW_INTRO_COMPLETE
} WwIntroStage;

typedef struct WwIntroRacer {
    const uint8_t *source;
    size_t source_size;
    uint16_t distance;
    uint8_t state;
} WwIntroRacer;

typedef struct WwIntro {
    const WwArchive *archive;
    WwDisplay *display;
    WwAudio *audio;
    const WwRenderer *renderer;
    WwPcxImage image[3];
    WwSpriteScaleSet car_scale;
    WwSound pass_sound;
    int pass_voice;
    WwIntroRacer racer[8];
    WwIntroStage stage;
    unsigned stage_ticks;
    unsigned animation_tick_accumulator;
    uint8_t last_activated_racer;
    bool racer_animation_started;
    bool music_handed_off;
    bool open;
    bool dirty;
} WwIntro;

bool ww_intro_open(WwIntro *intro, const WwArchive *archive,
                   WwDisplay *display, WwAudio *audio,
                   const WwRenderer *renderer,
                   uint8_t music_volume);
void ww_intro_close(WwIntro *intro);
void ww_intro_update(WwIntro *intro, const WwInput *input,
                     unsigned elapsed_136_ticks);
bool ww_intro_render(WwIntro *intro);
bool ww_intro_complete(const WwIntro *intro);

#endif
