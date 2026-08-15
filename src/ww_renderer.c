#include "ww_renderer.h"

#include <string.h>

/* Translation of sub_274A4.  The original stores only the right edge; the
 * matching left edge is retained here because chunky SDL drawing needs both. */
void ww_renderer_init(WwRenderer *renderer)
{
    unsigned distance;
    memset(renderer, 0, sizeof(*renderer));
    for (distance = 0; distance < WW_PROJECTION_TABLE_COUNT; ++distance) {
        int width;
        int left;
        if (distance == 0) {
            width = 240;
        } else {
            int remainder;
            width = 0x4650 / (int)distance;
            remainder = 0x4650 - width * (int)distance;
            if (remainder > (int)distance / 2) {
                ++width;
            }
            if (width < 21) width = 21;
            if (width > 240) width = 240;
        }
        left = 120 - width / 2;
        renderer->projection_left[distance] = (uint16_t)left;
        renderer->projection_right[distance] = (uint16_t)(left + width);
    }
}

bool ww_renderer_load_assets(WwRenderer *renderer, const WwArchive *archive)
{
    WwArchiveView trig;
    WwArchiveView ndist;
    WwArchiveView view;
    if (renderer == NULL || archive == NULL ||
        !ww_archive_view(archive, "TRIG.DAT", &trig) ||
        !ww_archive_view(archive, "NDIST", &ndist) ||
        !ww_archive_view(archive, "VIEW", &view) ||
        trig.size != WW_TRIG_BYTES || ndist.size != WW_NDIST_BYTES ||
        view.size != WW_VIEW_BYTES) {
        return false;
    }
    renderer->trig_data = trig.data;
    renderer->ndist_data = ndist.data;
    /* loc_36684 loads this 0x500-byte signed 2.14 perspective correction
     * table directly into dword_6B7F8. */
    renderer->view_data = view.data;
    return true;
}

/* MUL followed by SHR EAX,16 in sub_37E94/sub_3C3E9 intentionally keeps
 * only the low 32 bits of the unsigned product. */
static uint16_t ww_renderer_fixed_coordinate(uint32_t trig, uint32_t scale,
                                             uint16_t origin)
{
    uint32_t low_product = trig * scale;
    return (uint16_t)((uint16_t)(low_product >> 16) + origin);
}

static bool ww_renderer_sample_track(const WwTrack *track,
                                     uint16_t x, uint16_t y,
                                     uint8_t *color)
{
    WwTrackSurfaceSample sample;
    if (!ww_track_surface_sample(track, x, y, &sample)) {
        return false;
    }
    *color = sample.color;
    return true;
}

/* Road core shared by sub_37E94 and sub_3C3E9.  Their Mode X plane masks
 * become 1-, 2-, or 4-pixel chunky writes for detail values 1, 2, and 3. */
bool ww_renderer_render_road_split(const WwRenderer *renderer,
                                   const WwTrack *track,
                                   uint8_t *pixels, size_t pitch,
                                   bool lower_view, uint16_t heading,
                                   uint16_t origin_x, uint16_t origin_y,
                                   WwRoadDetail detail)
{
    unsigned x;
    unsigned angle;
    unsigned horizontal_step;
    unsigned bottom = lower_view ? 199u : 99u;
    if (renderer == NULL || track == NULL || pixels == NULL ||
        renderer->trig_data == NULL || renderer->ndist_data == NULL ||
        pitch < WW_SCREEN_WIDTH || heading >= WW_TRIG_ENTRY_COUNT) {
        return false;
    }
    horizontal_step = detail == WW_ROAD_DETAIL_HIGH ? 1u
                      : detail == WW_ROAD_DETAIL_MEDIUM ? 2u : 4u;
    angle = heading < 0xa0u ? heading + WW_TRIG_ENTRY_COUNT - 0xa0u
                            : heading - 0xa0u;
    for (x = 0; x < WW_ROAD_COLUMNS; x += horizontal_step) {
        const uint8_t *trig = renderer->trig_data +
                              (size_t)angle * WW_TRIG_ENTRY_BYTES;
        uint32_t trig_x = ww_read_le32(trig);
        uint32_t trig_y = ww_read_le32(trig + 4);
        unsigned depth;
        for (depth = 0; depth < WW_ROAD_ROWS; ++depth) {
            size_t distance_index = (size_t)x * WW_ROAD_ROWS + depth;
            uint32_t scale = ww_read_le32(renderer->ndist_data +
                                          distance_index * 4u);
            uint16_t sample_x = ww_renderer_fixed_coordinate(trig_x, scale,
                                                              origin_x);
            uint16_t sample_y = ww_renderer_fixed_coordinate(trig_y, scale,
                                                              origin_y);
            uint8_t color;
            if (!ww_renderer_sample_track(track, sample_x, sample_y, &color)) {
                return false;
            }
            {
                unsigned duplicate;
                for (duplicate = 0; duplicate < horizontal_step; ++duplicate) {
                    pixels[(size_t)(bottom - depth) * pitch + x + duplicate] = color;
                }
            }
        }
        angle += horizontal_step;
        if (angle >= WW_TRIG_ENTRY_COUNT) angle -= WW_TRIG_ENTRY_COUNT;
    }
    return true;
}

/* The 20 REP MOVSB blocks at the head of sub_37E94/sub_3C3E9 use VGA write
 * mode 1.  One source address transfers the four latched plane pixels, so 80
 * addresses reproduce 320 distinct pixels.  sub_279E8 loads the track .PAR
 * payload into the A7D00 panorama area: its 25,600 bytes are exactly twenty
 * 1,280-pixel logical rows.  dword_7D860 ranges from A7D00 through A7DF0;
 * its byte offset therefore selects a 320-pixel window in each logical row. */
bool ww_renderer_render_horizon_split(const WwTrack *track,
                                      uint8_t *pixels, size_t pitch,
                                      bool lower_view,
                                      unsigned source_address_offset)
{
    unsigned row;
    unsigned destination_y = lower_view ? 110u : 10u;
    if (track == NULL || pixels == NULL || pitch < WW_SCREEN_WIDTH ||
        source_address_offset > 0xf0u) {
        return false;
    }
    for (row = 0; row < 20u; ++row) {
        unsigned x;
        unsigned panorama_start = source_address_offset * 4u;
        uint8_t *destination =
            pixels + (size_t)(destination_y + row) * pitch;
        for (x = 0; x < WW_SCREEN_WIDTH; ++x) {
            unsigned panorama_x = panorama_start + x;
            destination[x] = track->par_bytes[
                (size_t)row * WW_SCREEN_WIDTH * 4u + panorama_x];
        }
    }
    return true;
}
