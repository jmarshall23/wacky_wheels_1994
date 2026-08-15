#include "ww_audio.h"

#include "ww_common.h"
#include "ww_voc.h"

#include <timidity.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Replaces the linked FX_MAN/Multivoc/Sound Blaster boundary at sub_41550+. */

static int16_t ww_clamp_s16(int32_t sample)
{
    if (sample > 32767) return 32767;
    if (sample < -32768) return -32768;
    return (int16_t)sample;
}

static void ww_audio_callback(void *userdata, Uint8 *stream, int length)
{
    WwAudio *audio = (WwAudio *)userdata;
    int16_t *output = (int16_t *)stream;
    int frames = length / (int)(sizeof(int16_t) * 2);
    int frame;
    memset(stream, 0, (size_t)length);
    if (audio->music_song != NULL) {
        MidSong *song = (MidSong *)audio->music_song;
        size_t filled = mid_song_read_wave(song, (sint8 *)stream,
                                           (size_t)length);
        if (filled < (size_t)length && audio->music_loop) {
            mid_song_start(song);
            filled += mid_song_read_wave(song, (sint8 *)stream + filled,
                                         (size_t)length - filled);
        }
        if (filled < (size_t)length) {
            memset(stream + filled, 0, (size_t)length - filled);
        }
    }
    for (frame = 0; frame < frames; ++frame) {
        int32_t left = output[frame * 2];
        int32_t right = output[frame * 2 + 1];
        unsigned i;
        for (i = 0; i < WW_AUDIO_VOICES; ++i) {
            WwAudioVoice *voice = &audio->voices[i];
            int32_t sample;
            if (!voice->active || voice->sound == NULL ||
                voice->sound->sample_count == 0) {
                continue;
            }
            {
                size_t position = (size_t)(voice->position_fp >> 16);
                size_t next = position + 1u;
                unsigned fraction = (unsigned)(voice->position_fp & 0xffffu);
                int32_t a = voice->sound->samples[position];
                int32_t b;
                if (next >= voice->sound->sample_count) {
                    next = voice->sound->loop ? 0u : position;
                }
                b = voice->sound->samples[next];
                sample = (a * (int32_t)(0x10000u - fraction) +
                          b * (int32_t)fraction) >> 16;
            }
            sample = sample *
                     voice->volume *
                     (voice->use_master_volume ? audio->master_volume : 255) /
                     (255 * 255);
            left += sample;
            right += sample;
            voice->position_fp += voice->step_fp;
            if ((voice->position_fp >> 16) >= voice->sound->sample_count) {
                if (voice->sound->loop) {
                    uint64_t loop_length =
                        (uint64_t)voice->sound->sample_count << 16;
                    voice->position_fp %= loop_length;
                } else {
                    voice->active = false;
                }
            }
        }
        output[frame * 2] = ww_clamp_s16(left);
        output[frame * 2 + 1] = ww_clamp_s16(right);
    }
}

static bool ww_audio_file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    fclose(file);
    return true;
}

static bool ww_audio_timidity_config(char path[1024])
{
    static const char relative[] = "src/thirdparty/freepats/crude.cfg";
    static const char parent_relative[] = "../src/thirdparty/freepats/crude.cfg";
    char *base;
    size_t length;
    if (ww_audio_file_exists(relative)) {
        memcpy(path, relative, sizeof(relative));
        return true;
    }
    base = SDL_GetBasePath();
    if (base == NULL) return false;
    length = strlen(base);
    if (length + sizeof(relative) <= 1024u) {
        memcpy(path, base, length);
        memcpy(path + length, relative, sizeof(relative));
        if (ww_audio_file_exists(path)) {
            SDL_free(base);
            return true;
        }
    }
    if (length + sizeof(parent_relative) <= 1024u) {
        memcpy(path, base, length);
        memcpy(path + length, parent_relative, sizeof(parent_relative));
        if (ww_audio_file_exists(path)) {
            SDL_free(base);
            return true;
        }
    }
    SDL_free(base);
    return false;
}

bool ww_audio_open(WwAudio *audio)
{
    SDL_AudioSpec desired;
    char config_path[1024];
    memset(audio, 0, sizeof(*audio));
    memset(&desired, 0, sizeof(desired));
    desired.freq = 44100;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = ww_audio_callback;
    desired.userdata = audio;
    audio->master_volume = 255;
    audio->music_volume = 255;
    if (ww_audio_timidity_config(config_path) && mid_init(config_path) == 0) {
        audio->timidity_initialized = true;
    } else {
        ww_error("MIDI synthesizer unavailable: cannot load FreePats");
    }
    audio->device = SDL_OpenAudioDevice(NULL, 0, &desired, &audio->specification, 0);
    if (audio->device == 0) {
        ww_error("SDL audio unavailable: %s", SDL_GetError());
        if (audio->timidity_initialized) mid_exit();
        audio->timidity_initialized = false;
        return false;
    }
    SDL_PauseAudioDevice(audio->device, 0);
    return true;
}

void ww_audio_close(WwAudio *audio)
{
    if (audio == NULL) {
        return;
    }
    if (audio->device != 0) {
        SDL_CloseAudioDevice(audio->device);
    }
    if (audio->music_song != NULL) {
        mid_song_free((MidSong *)audio->music_song);
    }
    if (audio->timidity_initialized) mid_exit();
    memset(audio, 0, sizeof(*audio));
}

bool ww_audio_load_voc(const WwArchive *archive, const char *name, WwSound *sound)
{
    WwArchiveView view;
    WwVocPcm pcm;
    size_t destination_count;
    size_t i;

    memset(sound, 0, sizeof(*sound));
    if (!ww_archive_view(archive, name, &view) ||
        !ww_voc_decode(view.data, view.size, &pcm)) {
        return false;
    }
    destination_count = (pcm.sample_count * 44100u + pcm.sample_rate - 1u) /
                        pcm.sample_rate;
    sound->samples = (int16_t *)malloc(destination_count * sizeof(int16_t));
    if (sound->samples == NULL) {
        ww_voc_free(&pcm);
        return false;
    }
    for (i = 0; i < destination_count; ++i) {
        uint64_t position = (uint64_t)i * pcm.sample_rate;
        size_t source = (size_t)(position / 44100u);
        unsigned fraction = (unsigned)(position % 44100u);
        int a = (int)pcm.samples[source] - 128;
        int b = source + 1 < pcm.sample_count ? (int)pcm.samples[source + 1] - 128 : a;
        int interpolated = (a * (44100 - (int)fraction) + b * (int)fraction) / 44100;
        sound->samples[i] = (int16_t)(interpolated * 256);
    }
    sound->sample_count = destination_count;
    sound->loop = pcm.loop;
    ww_voc_free(&pcm);
    return true;
}

void ww_audio_free_sound(WwSound *sound)
{
    if (sound != NULL) {
        free(sound->samples);
        memset(sound, 0, sizeof(*sound));
    }
}

static int ww_audio_play_internal(WwAudio *audio, const WwSound *sound,
                                  uint8_t volume, bool use_master_volume)
{
    unsigned i;
    if (audio == NULL || audio->device == 0 || sound == NULL || sound->samples == NULL) {
        return -1;
    }
    SDL_LockAudioDevice(audio->device);
    for (i = 0; i < WW_AUDIO_VOICES; ++i) {
        if (!audio->voices[i].active) {
            audio->voices[i].sound = sound;
            audio->voices[i].position_fp = 0u;
            audio->voices[i].step_fp = 0x10000u;
            audio->voices[i].volume = volume;
            audio->voices[i].use_master_volume = use_master_volume;
            audio->voices[i].active = true;
            SDL_UnlockAudioDevice(audio->device);
            return (int)i;
        }
    }
    SDL_UnlockAudioDevice(audio->device);
    return -1;
}

int ww_audio_play(WwAudio *audio, const WwSound *sound, uint8_t volume)
{
    return ww_audio_play_internal(audio, sound, volume, true);
}

int ww_audio_play_unscaled(WwAudio *audio, const WwSound *sound,
                           uint8_t volume)
{
    return ww_audio_play_internal(audio, sound, volume, false);
}

void ww_audio_stop(WwAudio *audio, int voice)
{
    if (audio == NULL || audio->device == 0 || voice < 0 || voice >= WW_AUDIO_VOICES) {
        return;
    }
    SDL_LockAudioDevice(audio->device);
    memset(&audio->voices[voice], 0, sizeof(audio->voices[voice]));
    SDL_UnlockAudioDevice(audio->device);
}

bool ww_audio_voice_active(WwAudio *audio, int voice)
{
    bool active;
    if (audio == NULL || audio->device == 0 || voice < 0 ||
        voice >= WW_AUDIO_VOICES) {
        return false;
    }
    SDL_LockAudioDevice(audio->device);
    active = audio->voices[voice].active;
    SDL_UnlockAudioDevice(audio->device);
    return active;
}

/* MultiVoc's FX_SetPitch call at sub_41B44 receives a signed cents offset.
 * The race HUD passes racer velocity * 20 - 0x708 once per frame. */
void ww_audio_set_pitch_cents(WwAudio *audio, int voice, int cents)
{
    double ratio;
    uint32_t step;
    if (audio == NULL || audio->device == 0 || voice < 0 ||
        voice >= WW_AUDIO_VOICES) {
        return;
    }
    ratio = pow(2.0, (double)cents / 1200.0);
    if (ratio < 1.0 / 256.0) ratio = 1.0 / 256.0;
    if (ratio > 255.0) ratio = 255.0;
    step = (uint32_t)(ratio * 65536.0 + 0.5);
    SDL_LockAudioDevice(audio->device);
    if (audio->voices[voice].active) {
        audio->voices[voice].step_fp = step;
    }
    SDL_UnlockAudioDevice(audio->device);
}

void ww_audio_stop_all(WwAudio *audio)
{
    if (audio == NULL || audio->device == 0) {
        return;
    }
    SDL_LockAudioDevice(audio->device);
    memset(audio->voices, 0, sizeof(audio->voices));
    SDL_UnlockAudioDevice(audio->device);
}

bool ww_audio_play_midi(WwAudio *audio, const uint8_t *data, size_t size,
                        bool loop, uint8_t volume)
{
    MidIStream *stream;
    MidSongOptions options;
    MidSong *song;
    MidSong *old_song;
    if (audio == NULL || audio->device == 0 || !audio->timidity_initialized ||
        data == NULL || size == 0u) {
        return false;
    }
    stream = mid_istream_open_mem((void *)data, size);
    if (stream == NULL) return false;
    memset(&options, 0, sizeof(options));
    options.rate = audio->specification.freq;
    options.format = MID_AUDIO_S16LSB;
    options.channels = 2;
    options.buffer_size = audio->specification.samples;
    song = mid_song_load(stream, &options);
    mid_istream_close(stream);
    if (song == NULL) return false;
    mid_song_set_volume(song, (int)volume * 100 / 255);
    mid_song_start(song);
    SDL_LockAudioDevice(audio->device);
    old_song = (MidSong *)audio->music_song;
    audio->music_song = song;
    audio->music_loop = loop;
    audio->music_volume = volume;
    SDL_UnlockAudioDevice(audio->device);
    if (old_song != NULL) mid_song_free(old_song);
    return true;
}

bool ww_audio_play_midi_asset(WwAudio *audio, const WwArchive *archive,
                              const char *name, bool loop, uint8_t volume)
{
    WwArchiveView view;
    return archive != NULL && ww_archive_view(archive, name, &view) &&
           ww_audio_play_midi(audio, view.data, view.size, loop, volume);
}

void ww_audio_stop_music(WwAudio *audio)
{
    MidSong *song;
    if (audio == NULL || audio->device == 0) return;
    SDL_LockAudioDevice(audio->device);
    song = (MidSong *)audio->music_song;
    audio->music_song = NULL;
    SDL_UnlockAudioDevice(audio->device);
    if (song != NULL) mid_song_free(song);
}

void ww_audio_set_music_volume(WwAudio *audio, uint8_t volume)
{
    if (audio == NULL || audio->device == 0) return;
    SDL_LockAudioDevice(audio->device);
    audio->music_volume = volume;
    if (audio->music_song != NULL) {
        mid_song_set_volume((MidSong *)audio->music_song,
                            (int)volume * 100 / 255);
    }
    SDL_UnlockAudioDevice(audio->device);
}

bool ww_audio_music_playing(WwAudio *audio)
{
    bool playing;
    if (audio == NULL || audio->device == 0) return false;
    SDL_LockAudioDevice(audio->device);
    playing = audio->music_song != NULL &&
              mid_song_get_time((MidSong *)audio->music_song) <
                  mid_song_get_total_time((MidSong *)audio->music_song);
    SDL_UnlockAudioDevice(audio->device);
    return playing;
}
