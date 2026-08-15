#ifndef WW_MUSIC_H
#define WW_MUSIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct WwMidiInfo {
    uint16_t format;
    uint16_t tracks;
    uint16_t division;
    uint32_t events;
} WwMidiInfo;

bool ww_midi_inspect(const uint8_t *data, size_t size, WwMidiInfo *info);
bool ww_music_race_asset(bool bonus_pack, unsigned board_page,
                         unsigned playlist_index, bool fm_device,
                         char *asset_name, size_t asset_name_size);

#endif
