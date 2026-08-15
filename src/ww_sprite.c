#include "ww_sprite.h"

#include "ww_common.h"

#include <string.h>

/* Parser for the ten records consumed by sub_28038.  Every destination
 * column owns height+1 signed source-pointer increments. */
bool ww_sprite_scale_set_load(WwSpriteScaleSet *set,
                              const WwArchive *archive,
                              const char *asset_name)
{
    WwArchiveView view;
    size_t cursor = 0;
    unsigned i;
    if (set == NULL || archive == NULL || asset_name == NULL ||
        !ww_archive_view(archive, asset_name, &view)) {
        return false;
    }
    memset(set, 0, sizeof(*set));
    for (i = 0; i < WW_SPRITE_SCALE_LEVELS; ++i) {
        WwSpriteScale *scale = &set->level[i];
        size_t offset_bytes;
        if (cursor + 4u > view.size) return false;
        scale->width = ww_read_le16(view.data + cursor);
        scale->height = ww_read_le16(view.data + cursor + 2u);
        cursor += 4u;
        if (scale->width == 0 || scale->height == 0 ||
            (size_t)scale->width > SIZE_MAX / ((size_t)scale->height + 1u)) {
            return false;
        }
        scale->offset_count =
            (size_t)scale->width * ((size_t)scale->height + 1u);
        if (scale->offset_count > SIZE_MAX / 4u) return false;
        offset_bytes = scale->offset_count * 4u;
        if (offset_bytes > view.size - cursor) return false;
        scale->offset_data = view.data + cursor;
        cursor += offset_bytes;
    }
    /* sub_28038 consumes exactly ten records and does not require EOF.  This
     * matters for 18X13.INF: its ten records occupy 4,900 bytes and the
     * archive entry intentionally has a further 92 trailing bytes. */
    return true;
}

/* Chunky equivalent of sub_37487's unclipped branch.  Its Mode X plane walk
 * becomes an ordinary left-to-right destination-column walk.  The original
 * source pointer is not bounded to one 0x428-byte CARS.SP frame; the scaler's
 * signed increments may intentionally reach bytes in the following frame. */
bool ww_sprite_draw_scaled_column_major(uint8_t *pixels, size_t pitch,
                                        unsigned screen_width,
                                        unsigned screen_height,
                                        int x, int y,
                                        const uint8_t *source,
                                        size_t source_size,
                                        const WwSpriteScale *scale,
                                        uint8_t transparent_color)
{
    size_t offset_index = 0;
    unsigned column;
    if (pixels == NULL || source == NULL || scale == NULL ||
        scale->offset_data == NULL || source_size == 0 ||
        pitch < screen_width || scale->width == 0 || scale->height == 0 ||
        scale->offset_count !=
            (size_t)scale->width * ((size_t)scale->height + 1u)) {
        return false;
    }

    for (column = 0; column < scale->width; ++column) {
        int64_t source_offset = 0;
        unsigned row;
        source_offset += (int32_t)ww_read_le32(
            scale->offset_data + offset_index++ * 4u);
        for (row = 0; row < scale->height; ++row) {
            int destination_x = x + (int)column;
            int destination_y = y + (int)row;
            uint8_t color;
            if (source_offset < 0 || (uint64_t)source_offset >= source_size) {
                return false;
            }
            color = source[source_offset];
            if (color != transparent_color && destination_x >= 0 &&
                destination_y >= 0 &&
                (unsigned)destination_x < screen_width &&
                (unsigned)destination_y < screen_height) {
                pixels[(size_t)destination_y * pitch +
                       (unsigned)destination_x] = color;
            }
            source_offset += (int32_t)ww_read_le32(
                scale->offset_data + offset_index++ * 4u);
        }
    }
    return offset_index == scale->offset_count;
}
