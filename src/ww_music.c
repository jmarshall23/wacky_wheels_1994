#include "ww_music.h"

#include "ww_common.h"

#include <stdio.h>
#include <string.h>

/* The six sub_32E60 cases each construct a repeating three-song race list.
 * These are the exact stems before byte_80ADC supplies .MID or .KLM. */
static const char *const ww_race_music_stems[6][3] = {
    {"TURBO", "DANCE", "FLIGHT"},
    {"STARBRAI", "BASSATTK", "KARD"},
    {"BANSHI", "STARBRAI", "BRICWALL"},
    {"FLIGHT", "KARD", "PUMPER"},
    {"DANCE", "BASSATTK", "TURBO"},
    {"BANSHI", "BRICWALL", "STARBRAI"}
};

bool ww_music_race_asset(bool bonus_pack, unsigned board_page,
                         unsigned playlist_index, bool fm_device,
                         char *asset_name, size_t asset_name_size)
{
    unsigned group;
    int written;
    if (board_page >= 3u || playlist_index >= 3u || asset_name == NULL ||
        asset_name_size == 0) {
        return false;
    }
    group = board_page + (bonus_pack ? 3u : 0u);
    written = snprintf(asset_name, asset_name_size, "%s.%s",
                       ww_race_music_stems[group][playlist_index],
                       fm_device ? "KLM" : "MID");
    return written >= 0 && (size_t)written < asset_name_size;
}

static bool ww_midi_vlq(const uint8_t *data, size_t size, size_t *position,
                        uint32_t *value)
{
    unsigned i;
    uint32_t result = 0;
    for (i = 0; i < 4; ++i) {
        uint8_t byte;
        if (*position >= size) return false;
        byte = data[(*position)++];
        result = (result << 7) | (byte & 0x7fu);
        if ((byte & 0x80u) == 0) {
            *value = result;
            return true;
        }
    }
    return false;
}

static bool ww_midi_track(const uint8_t *data, size_t size, uint32_t *events)
{
    size_t position = 0;
    uint8_t running_status = 0;
    bool end_of_track = false;
    while (position < size && !end_of_track) {
        uint32_t delta;
        uint8_t status;
        bool running;
        if (!ww_midi_vlq(data, size, &position, &delta) || position >= size) {
            return false;
        }
        (void)delta;
        status = data[position];
        running = status < 0x80u;
        if (running) {
            if (running_status == 0) return false;
            status = running_status;
        } else {
            ++position;
        }

        if (status >= 0x80u && status <= 0xefu) {
            unsigned data_bytes = (status & 0xf0u) == 0xc0u ||
                                  (status & 0xf0u) == 0xd0u ? 1u : 2u;
            unsigned i;
            running_status = status;
            for (i = 0; i < data_bytes; ++i) {
                if (position >= size || data[position] >= 0x80u) return false;
                ++position;
            }
        } else if (status == 0xffu) {
            uint8_t type;
            uint32_t length;
            running_status = 0;
            if (position >= size) return false;
            type = data[position++];
            if (!ww_midi_vlq(data, size, &position, &length) ||
                length > size - position) {
                return false;
            }
            if (type == 0x2fu) {
                if (length != 0) return false;
                end_of_track = true;
            }
            position += length;
        } else if (status == 0xf0u || status == 0xf7u) {
            uint32_t length;
            running_status = 0;
            if (!ww_midi_vlq(data, size, &position, &length) ||
                length > size - position) {
                return false;
            }
            position += length;
        } else {
            return false;
        }
        ++*events;
    }
    return end_of_track && position == size;
}

/* The DOS music manager selects KLM for FM devices and SMF MIDI otherwise. */
bool ww_midi_inspect(const uint8_t *data, size_t size, WwMidiInfo *info)
{
    uint32_t header_size;
    size_t position;
    uint16_t track;
    if (data == NULL || info == NULL || size < 14 || memcmp(data, "MThd", 4) != 0) {
        return false;
    }
    header_size = ww_read_be32(data + 4);
    if (header_size < 6 || 8u + header_size > size) {
        return false;
    }
    info->format = ww_read_be16(data + 8);
    info->tracks = ww_read_be16(data + 10);
    info->division = ww_read_be16(data + 12);
    info->events = 0;
    if (info->format > 2 || info->tracks == 0 || info->division == 0 ||
        (info->format == 0 && info->tracks != 1)) {
        return false;
    }
    position = 8u + header_size;
    for (track = 0; track < info->tracks; ++track) {
        uint32_t track_size;
        if (position + 8u > size || memcmp(data + position, "MTrk", 4) != 0) {
            return false;
        }
        track_size = ww_read_be32(data + position + 4);
        position += 8;
        if (track_size > size - position ||
            !ww_midi_track(data + position, track_size, &info->events)) {
            return false;
        }
        position += track_size;
    }
    return position == size;
}
