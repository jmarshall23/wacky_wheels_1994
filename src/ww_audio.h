#ifndef WW_AUDIO_H
#define WW_AUDIO_H

#include "ww_archive.h"

#include <SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WW_AUDIO_VOICES 32

typedef struct WwSound {
    int16_t *samples;
    size_t sample_count;
    bool loop;
} WwSound;

typedef struct WwAudioVoice {
    const WwSound *sound;
    uint64_t position_fp;
    uint32_t step_fp;
    uint8_t volume;
    bool use_master_volume;
    bool active;
} WwAudioVoice;

typedef struct WwAudio {
    SDL_AudioDeviceID device;
    SDL_AudioSpec specification;
    WwAudioVoice voices[WW_AUDIO_VOICES];
    uint8_t master_volume;
    void *music_song;
    uint8_t music_volume;
    bool music_loop;
    bool timidity_initialized;
} WwAudio;

bool ww_audio_open(WwAudio *audio);
void ww_audio_close(WwAudio *audio);
bool ww_audio_load_voc(const WwArchive *archive, const char *name, WwSound *sound);
void ww_audio_free_sound(WwSound *sound);
int ww_audio_play(WwAudio *audio, const WwSound *sound, uint8_t volume);
int ww_audio_play_unscaled(WwAudio *audio, const WwSound *sound,
                           uint8_t volume);
void ww_audio_stop(WwAudio *audio, int voice);
bool ww_audio_voice_active(WwAudio *audio, int voice);
void ww_audio_set_pitch_cents(WwAudio *audio, int voice, int cents);
void ww_audio_stop_all(WwAudio *audio);
bool ww_audio_play_midi(WwAudio *audio, const uint8_t *data, size_t size,
                        bool loop, uint8_t volume);
bool ww_audio_play_midi_asset(WwAudio *audio, const WwArchive *archive,
                              const char *name, bool loop, uint8_t volume);
void ww_audio_stop_music(WwAudio *audio);
void ww_audio_set_music_volume(WwAudio *audio, uint8_t volume);
bool ww_audio_music_playing(WwAudio *audio);

#endif
