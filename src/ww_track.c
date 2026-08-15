#include "ww_track.h"

#include "ww_common.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct WwTextCursor {
    const uint8_t *data;
    size_t size;
    size_t position;
} WwTextCursor;

static bool ww_track_name(char output[WW_ARCHIVE_NAME_BYTES + 1],
                          const char *base_name, const char *extension)
{
    int length = snprintf(output, WW_ARCHIVE_NAME_BYTES + 1, "%s%s",
                          base_name, extension);
    return length > 0 && length <= WW_ARCHIVE_NAME_BYTES;
}

static bool ww_track_read_line(WwTextCursor *cursor, char *output, size_t capacity)
{
    size_t length = 0;
    if (capacity == 0 || cursor->position >= cursor->size) {
        return false;
    }
    while (cursor->position < cursor->size &&
           cursor->data[cursor->position] != '\r' &&
           cursor->data[cursor->position] != '\n') {
        if (length + 1 >= capacity) {
            return false;
        }
        output[length++] = (char)cursor->data[cursor->position++];
    }
    output[length] = '\0';
    if (cursor->position < cursor->size && cursor->data[cursor->position] == '\r') {
        ++cursor->position;
    }
    if (cursor->position < cursor->size && cursor->data[cursor->position] == '\n') {
        ++cursor->position;
    }
    return true;
}

static bool ww_track_read_i16(WwTextCursor *cursor, int16_t *value)
{
    char line[64];
    char *end;
    long parsed;
    if (!ww_track_read_line(cursor, line, sizeof(line))) {
        return false;
    }
    errno = 0;
    parsed = strtol(line, &end, 10);
    while (*end == ' ' || *end == '\t') {
        ++end;
    }
    if (errno != 0 || end == line || *end != '\0' ||
        parsed < INT16_MIN || parsed > INT16_MAX) {
        return false;
    }
    *value = (int16_t)parsed;
    return true;
}

/* Direct translation of the line order and adjustments in sub_2717C. */
static bool sub_2717c_parse_game(WwTrackGameDefinition *game,
                                 const uint8_t *data, size_t size)
{
    WwTextCursor cursor = {data, size, 0};
    int16_t raw;
    unsigned i;
    memset(game, 0, sizeof(*game));
    for (i = 0; i < WW_TRACK_IMAGE_NAMES; ++i) {
        if (!ww_track_read_line(&cursor, game->image_name[i],
                                sizeof(game->image_name[i]))) {
            return false;
        }
    }
    if (!ww_track_read_line(&cursor, game->background_name,
                            sizeof(game->background_name)) ||
        !ww_track_read_i16(&cursor, &raw)) {
        return false;
    }
    game->origin[0] = (int16_t)(raw + 0x400);
    if (!ww_track_read_i16(&cursor, &raw)) {
        return false;
    }
    game->origin[1] = (int16_t)(raw + 0x400);
    if (!ww_track_read_line(&cursor, game->sub_2717c_line_7,
                            sizeof(game->sub_2717c_line_7))) {
        return false;
    }
    for (i = 0; i < WW_TRACK_PARAMETER_PAIRS; ++i) {
        if (!ww_track_read_i16(&cursor, &raw)) {
            return false;
        }
        game->parameter_pair[i][0] = (int16_t)(raw + 0x3e0);
        if (!ww_track_read_i16(&cursor, &raw)) {
            return false;
        }
        game->parameter_pair[i][1] = (int16_t)(raw + 0x3e0);
    }
    if (!ww_track_read_i16(&cursor, &raw)) {
        return false;
    }
    game->has_special_parameters = raw != 0;
    if (game->has_special_parameters) {
        for (i = 0; i < 3; ++i) {
            if (!ww_track_read_i16(&cursor, &game->special_parameter[i])) {
                return false;
            }
        }
    }

    for (i = 0; i < WW_TRACK_IMAGE_NAMES; ++i) {
        if (snprintf(game->alternate_image_name[i],
                     sizeof(game->alternate_image_name[i]), "A_%s",
                     game->image_name[i]) >=
            (int)sizeof(game->alternate_image_name[i])) {
            return false;
        }
    }
    return true;
}

static bool ww_track_asset(const WwArchive *archive, const char *base,
                           const char *extension, WwArchiveView *view)
{
    char name[WW_ARCHIVE_NAME_BYTES + 1];
    return ww_track_name(name, base, extension) &&
           ww_archive_view(archive, name, view);
}

static bool ww_track_parse_road(WwTrack *track, const WwArchiveView *view)
{
    uint16_t count;
    uint16_t i;
    size_t total_points = 0;
    if (view->size < 2) {
        return false;
    }
    count = ww_read_le16(view->data);
    if (view->size != 2u + (size_t)count * 14u) {
        return false;
    }
    track->road_segments = (WwRoadSegment *)calloc(count, sizeof(*track->road_segments));
    if (count != 0 && track->road_segments == NULL) {
        return false;
    }
    track->road_segment_count = count;
    for (i = 0; i < count; ++i) {
        const uint8_t *record = view->data + 2u + (size_t)i * 14u;
        WwRoadSegment *segment = &track->road_segments[i];
        segment->x0 = (int16_t)(ww_read_le16(record) + 0x400);
        segment->y0 = (int16_t)(ww_read_le16(record + 2) + 0x400);
        segment->x1 = (int16_t)(ww_read_le16(record + 4) + 0x400);
        segment->y1 = (int16_t)(ww_read_le16(record + 6) + 0x400);
        segment->sub_279e8_value_4 = (int16_t)ww_read_le16(record + 8);
        segment->sub_279e8_value_5 = (int16_t)ww_read_le16(record + 10);
        segment->sub_279e8_value_6 = (int16_t)ww_read_le16(record + 12);
        {
            int dx = abs((int)segment->x1 - segment->x0);
            int dy = abs((int)segment->y1 - segment->y0);
            size_t point_count = (size_t)(dx > dy ? dx : dy) + 1u;
            if (point_count > UINT16_MAX || total_points > UINT32_MAX - point_count) {
                return false;
            }
            segment->point_offset = (uint32_t)total_points;
            segment->point_count = (uint16_t)point_count;
            total_points += point_count;
        }
    }
    track->road_points = (WwTrackPoint *)malloc(total_points *
                                                 sizeof(*track->road_points));
    if (total_points != 0 && track->road_points == NULL) {
        return false;
    }
    track->road_point_count = total_points;
    for (i = 0; i < count; ++i) {
        WwRoadSegment *segment = &track->road_segments[i];
        size_t written = ww_track_rasterize_line(
            segment->x0, segment->y0, segment->x1, segment->y1,
            track->road_points + segment->point_offset, segment->point_count);
        if (written != segment->point_count) {
            return false;
        }
    }
    return true;
}

/* Exact Bresenham-style point order and strict error comparison of sub_21554. */
size_t ww_track_rasterize_line(int16_t x0_value, int16_t y0_value,
                               int16_t x1_value, int16_t y1_value,
                               WwTrackPoint *points, size_t capacity)
{
    int x = x0_value;
    int y = y0_value;
    int x1 = x1_value;
    int y1 = y1_value;
    int dx = x1 - x;
    int dy = y1 - y;
    int step_x = dx < 0 ? -1 : 1;
    int step_y = dy < 0 ? -1 : 1;
    int absolute_x = abs(dx);
    int absolute_y = abs(dy);
    int error;
    size_t count = 0;

    if (points == NULL && capacity != 0) {
        return 0;
    }
    if (absolute_x > absolute_y) {
        error = absolute_x / 2;
        while (x != x1) {
            if (count >= capacity) return 0;
            points[count].x = (int16_t)x;
            points[count++].y = (int16_t)y;
            error += absolute_y;
            if (error > absolute_x) {
                y += step_y;
                error -= absolute_x;
            }
            x += step_x;
        }
    } else {
        error = absolute_y / 2;
        while (y != y1) {
            if (count >= capacity) return 0;
            points[count].x = (int16_t)x;
            points[count++].y = (int16_t)y;
            error += absolute_x;
            if (error > absolute_y) {
                x += step_x;
                error -= absolute_y;
            }
            y += step_y;
        }
    }
    if (count >= capacity) return 0;
    points[count].x = (int16_t)x;
    points[count++].y = (int16_t)y;
    return count;
}

static bool ww_track_parse_spawns(WwTrack *track, const WwArchiveView *view)
{
    uint16_t count;
    uint16_t i;
    if (view->size < 2) {
        return false;
    }
    count = ww_read_le16(view->data);
    if (view->size != 2u + (size_t)count * 12u) {
        return false;
    }
    track->spawn_records = (WwSpawnRecord *)calloc(count, sizeof(*track->spawn_records));
    if (count != 0 && track->spawn_records == NULL) {
        return false;
    }
    track->spawn_record_count = count;
    for (i = 0; i < count; ++i) {
        const uint8_t *record = view->data + 2u + (size_t)i * 12u;
        WwSpawnRecord *spawn = &track->spawn_records[i];
        spawn->sprite_type = (int16_t)ww_read_le16(record);
        spawn->state = (int16_t)ww_read_le16(record + 2u);
        spawn->world_x = (int16_t)ww_read_le16(record + 4u);
        spawn->world_y = (int16_t)ww_read_le16(record + 6u);
        spawn->sub_21490_value_4 = (int16_t)ww_read_le16(record + 8u);
        spawn->animation_frame = (int16_t)ww_read_le16(record + 10u);
        /* sub_21490 clears this state word and converts the two coordinates
         * from track space by adding 0x400. */
        spawn->state = 0;
        spawn->world_x = (int16_t)(spawn->world_x + 0x400);
        spawn->world_y = (int16_t)(spawn->world_y + 0x400);
    }
    return true;
}

/* Direct chunky-memory form of sub_376D7's post-PCX rearrangement.  Each
 * 320x200 source contains 54 useful 32x32 tiles, arranged ten across. */
static bool sub_376d7_rearrange_tiles(uint8_t *destination,
                                      const WwPcxImage *source)
{
    unsigned tile;
    if (destination == NULL || source == NULL || source->pixels == NULL ||
        source->width != WW_SCREEN_WIDTH || source->height != WW_SCREEN_HEIGHT) {
        return false;
    }
    for (tile = 0; tile < WW_TRACK_TILES_PER_IMAGE; ++tile) {
        unsigned source_x = (tile % 10u) * WW_TRACK_TILE_WIDTH;
        unsigned source_y = (tile / 10u) * WW_TRACK_TILE_HEIGHT;
        unsigned row;
        for (row = 0; row < WW_TRACK_TILE_HEIGHT; ++row) {
            memcpy(destination + (size_t)tile * WW_TRACK_TILE_BYTES +
                       (size_t)row * WW_TRACK_TILE_WIDTH,
                   source->pixels + (size_t)(source_y + row) * source->width +
                       source_x,
                   WW_TRACK_TILE_WIDTH);
        }
    }
    return true;
}

/* sub_277B8 interleaves the four names into two 108-tile banks: GAM image
 * lines 0/1 form the visible bank and lines 2/3 form its surface mask. */
static bool sub_277b8_load_tile_banks(WwTrack *track, const WwArchive *archive)
{
    unsigned image_index;
    track->tile_pixels = (uint8_t *)malloc(WW_TRACK_ALL_TILE_BYTES);
    if (track->tile_pixels == NULL) {
        return false;
    }
    for (image_index = 0; image_index < WW_TRACK_ATLAS_IMAGES; ++image_index) {
        WwArchiveView view;
        WwPcxImage image;
        unsigned bank = image_index / 2u;
        unsigned half = image_index % 2u;
        uint8_t *destination = track->tile_pixels +
            (size_t)bank * WW_TRACK_TILE_BANK_BYTES +
            (size_t)half * WW_TRACK_TILES_PER_IMAGE * WW_TRACK_TILE_BYTES;
        if (!ww_archive_view(archive, track->game.alternate_image_name[image_index],
                             &view) ||
            !ww_pcx_decode(view.data, view.size, &image)) {
            return false;
        }
        if (image_index == 0) {
            memcpy(track->tile_palette, image.palette, sizeof(track->tile_palette));
        }
        if (!sub_376d7_rearrange_tiles(destination, &image)) {
            ww_pcx_free(&image);
            return false;
        }
        ww_pcx_free(&image);
    }
    return true;
}

bool ww_track_load(WwTrack *track, const WwArchive *archive, const char *base_name)
{
    WwTrack loaded;
    WwArchiveView view;
    const char *stage = ".GAM";
    unsigned i;
    if (track == NULL || archive == NULL || base_name == NULL ||
        strlen(base_name) > WW_ARCHIVE_NAME_BYTES - 4) {
        return false;
    }
    memset(&loaded, 0, sizeof(loaded));
    strncpy(loaded.base_name, base_name, sizeof(loaded.base_name) - 1);

    if (!ww_track_asset(archive, base_name, ".GAM", &view)) {
        goto failed;
    }
    stage = ".GAM lines";
    if (!sub_2717c_parse_game(&loaded.game, view.data, view.size)) {
        goto failed;
    }
    stage = ".GAM asset references";
    if (!ww_archive_find(archive, loaded.game.background_name)) {
        goto failed;
    }
    for (i = 0; i < WW_TRACK_IMAGE_NAMES; ++i) {
        if (!ww_archive_find(archive, loaded.game.alternate_image_name[i])) {
            goto failed;
        }
    }
    stage = "background PCX";
    if (!ww_archive_view(archive, loaded.game.background_name, &view) ||
        !ww_pcx_decode(view.data, view.size, &loaded.background) ||
        loaded.background.width != WW_SCREEN_WIDTH ||
        loaded.background.height != WW_SCREEN_HEIGHT) {
        goto failed;
    }
    stage = "sub_277B8/sub_376D7 tile banks";
    if (!sub_277b8_load_tile_banks(&loaded, archive)) {
        goto failed;
    }

    stage = ".RD/.SPW";
    if (!ww_track_asset(archive, base_name, ".RD", &view) ||
        !ww_track_parse_road(&loaded, &view) ||
        !ww_track_asset(archive, base_name, ".SPW", &view) ||
        !ww_track_parse_spawns(&loaded, &view) ||
        !ww_track_asset(archive, base_name, ".M", &view) ||
        view.size != WW_TRACK_TILE_COUNT) {
        goto failed;
    }
    memcpy(loaded.tile_index, view.data, WW_TRACK_TILE_COUNT);
    for (i = 0; i < WW_TRACK_TILE_COUNT; ++i) {
        loaded.tile_offset[i] = (uint32_t)loaded.tile_index[i] << 10;
    }

    stage = ".PAR";
    if (!ww_track_asset(archive, base_name, ".PAR", &view) ||
        view.size != WW_TRACK_PAR_BYTES) {
        goto failed;
    }
    memcpy(loaded.par_bytes, view.data, WW_TRACK_PAR_BYTES);

    stage = ".POS";
    if (!ww_track_asset(archive, base_name, ".POS", &view) ||
        view.size != WW_TRACK_POSITION_BYTES + 2u) {
        goto failed;
    }
    memcpy(loaded.position_map, view.data, WW_TRACK_POSITION_BYTES);
    loaded.position_count = (int16_t)ww_read_le16(view.data + WW_TRACK_POSITION_BYTES);
    loaded.position_quarter_count = (int16_t)(loaded.position_count / 4);

    stage = ".SIN";
    if (!ww_track_asset(archive, base_name, ".SIN", &view) ||
        view.size != WW_TRACK_SIN_RECORDS * 12u) {
        goto failed;
    }
    for (i = 0; i < WW_TRACK_SIN_RECORDS; ++i) {
        const uint8_t *record = view.data + i * 12u;
        loaded.sin_records[i].sub_279e8_discarded = ww_read_le32(record);
        loaded.sin_records[i].value[0] = ww_read_le32(record + 4);
        loaded.sin_records[i].value[1] = ww_read_le32(record + 8);
    }

    stage = "WACKY.SDX terrain drag";
    if (!ww_archive_view(archive, "WACKY.SDX", &view) || view.size < 2u) {
        goto failed;
    }
    loaded.surface_class_count = ww_read_le16(view.data);
    if (loaded.surface_class_count > WW_TRACK_SURFACE_CLASSES ||
        view.size != 2u + (size_t)loaded.surface_class_count *
                            WW_TRACK_SDX_RECORD_BYTES) {
        goto failed;
    }
    for (i = 0; i < loaded.surface_class_count; ++i) {
        loaded.surface_drag[i] = ww_read_le32(
            view.data + 2u + (size_t)i * WW_TRACK_SDX_RECORD_BYTES + 0x120u);
    }

    *track = loaded;
    return true;

failed:
    ww_error("track %s failed while translating %s", base_name, stage);
    ww_track_close(&loaded);
    return false;
}

void ww_track_close(WwTrack *track)
{
    if (track == NULL) {
        return;
    }
    free(track->road_segments);
    free(track->road_points);
    free(track->spawn_records);
    ww_pcx_free(&track->background);
    free(track->tile_pixels);
    memset(track, 0, sizeof(*track));
}

/* Exact sub_1260C lookup.  .POS is a 64-by-64 word map covering the central
 * 0x800-by-0x800 world square, with one gate value per 32 world units. */
bool ww_track_position_sample(const WwTrack *track, uint16_t x, uint16_t y,
                              int16_t *position)
{
    unsigned map_x;
    unsigned map_y;
    size_t offset;
    if (track == NULL || position == NULL || x < 0x400u || x > 0xbffu ||
        y < 0x400u || y > 0xbffu) {
        return false;
    }
    map_x = ((unsigned)x - 0x400u) >> 5;
    map_y = ((unsigned)y - 0x400u) >> 5;
    offset = ((size_t)map_y * 64u + map_x) * 2u;
    *position = (int16_t)ww_read_le16(track->position_map + offset);
    return true;
}

/* Chunky equivalent of the common lookup body in sub_37Axx.  The second
 * 0x1B000-byte bank decides which of the two .SIN values is selected. */
bool ww_track_surface_sample(const WwTrack *track, uint16_t x, uint16_t y,
                             WwTrackSurfaceSample *sample)
{
    unsigned tile_x = x >> 5;
    unsigned tile_y = y >> 5;
    unsigned tile_id = 0;
    size_t within_tile;
    size_t pixel_offset;
    if (track == NULL || sample == NULL || track->tile_pixels == NULL ||
        tile_x >= 128u || tile_y >= 128u) {
        return false;
    }
    /* sub_37A06 initializes the padded map to tile zero; sub_279E8 writes
     * the actual 64x64 map at tile coordinate 32,32. */
    if (tile_x >= 32u && tile_x < 96u && tile_y >= 32u && tile_y < 96u) {
        tile_id = track->tile_index[(tile_y - 32u) * 64u + (tile_x - 32u)];
    }
    if (tile_id >= WW_TRACK_TILES_PER_BANK) {
        return false;
    }
    within_tile = (size_t)(y & 31u) * WW_TRACK_TILE_WIDTH + (x & 31u);
    pixel_offset = (size_t)tile_id * WW_TRACK_TILE_BYTES + within_tile;
    sample->tile_id = (uint8_t)tile_id;
    sample->color = track->tile_pixels[pixel_offset];
    sample->mask = track->tile_pixels[WW_TRACK_TILE_BANK_BYTES + pixel_offset];
    sample->sub_37afc_value = track->sin_records[tile_id].value[
        sample->mask == 0 ? 0 : 1];
    return true;
}
