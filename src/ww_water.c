#include "ww_water.h"

#include "ww_common.h"

#include <string.h>

enum {
    WW_WATER_SDX_HEADER_BYTES = 2,
    WW_WATER_SPLASH_SCREEN_Y = 0xa8,
    WW_WATER_PLAYER_DISTANCE = 0x7c,
    WW_WATER_SHALLOW_LEFT_X = 1,
    WW_WATER_SHALLOW_RIGHT_X = 0x19,
    WW_WATER_SHALLOW_Y = 4
};

static bool ww_water_is_deep(uint32_t surface_class)
{
    return surface_class == WW_WATER_DEEP_CLASS_FIRST ||
           surface_class == WW_WATER_DEEP_CLASS_SECOND;
}

static const WwWaterEffectDescriptor *ww_water_descriptor(
    const WwWaterAssets *assets, uint8_t surface_class)
{
    if (assets == NULL || !assets->loaded) return NULL;
    if (surface_class == WW_WATER_DEEP_CLASS_FIRST) return &assets->deep[0];
    if (surface_class == WW_WATER_DEEP_CLASS_SECOND) return &assets->deep[1];
    return NULL;
}

static bool ww_water_parse_descriptor(WwWaterEffectDescriptor *descriptor,
                                      const uint8_t *record,
                                      size_t effects_size)
{
    uint64_t splash_end;
    uint64_t periscope_end;
    if (descriptor == NULL || record == NULL) return false;
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->splash_offset = ww_read_le32(record + 0x100u);
    descriptor->splash_stride = ww_read_le16(record + 0x108u);
    descriptor->splash_width = ww_read_le16(record + 0x10au);
    descriptor->splash_height = ww_read_le16(record + 0x10cu);
    descriptor->splash_frames = ww_read_le16(record + 0x112u);
    descriptor->periscope_offset = ww_read_le32(record + 0x104u);
    descriptor->periscope_stride = ww_read_le16(record + 0x114u);
    descriptor->periscope_width = ww_read_le16(record + 0x116u);
    descriptor->periscope_height = ww_read_le16(record + 0x118u);
    descriptor->periscope_frames = ww_read_le16(record + 0x11eu);
    splash_end = (uint64_t)descriptor->splash_offset +
                 (uint64_t)descriptor->splash_stride *
                     descriptor->splash_frames;
    periscope_end = (uint64_t)descriptor->periscope_offset +
                    (uint64_t)descriptor->periscope_stride *
                        descriptor->periscope_frames;
    return descriptor->splash_stride ==
               descriptor->splash_width * descriptor->splash_height &&
           descriptor->periscope_stride ==
               descriptor->periscope_width * descriptor->periscope_height &&
           descriptor->splash_frames != 0u &&
           descriptor->periscope_frames != 0u &&
           splash_end <= effects_size && periscope_end <= effects_size;
}

/* sub_28180 loads the 0x12e-byte WACKY.SDX terrain records; sub_2754C then
 * resolves their +100/+104 EFFECTS.SP offsets into the +126/+12a pointers.
 * Record 5 supplies the cyan shallow-water wheel spray.  Classes 6 and 0x0f
 * are the only deep-water records admitted at loc_241E7. */
bool ww_water_assets_load(WwWaterAssets *assets, const WwArchive *archive)
{
    WwWaterAssets loaded;
    WwArchiveView sdx;
    WwArchiveView effects;
    uint16_t count;
    const uint8_t *records;
    if (assets == NULL || archive == NULL ||
        !ww_archive_view(archive, "WACKY.SDX", &sdx) ||
        !ww_archive_view(archive, "EFFECTS.SP", &effects) ||
        sdx.size < WW_WATER_SDX_HEADER_BYTES) {
        return false;
    }
    count = ww_read_le16(sdx.data);
    if (count <= WW_WATER_DEEP_CLASS_SECOND ||
        sdx.size != WW_WATER_SDX_HEADER_BYTES +
                        (size_t)count * WW_WATER_SDX_RECORD_BYTES) {
        return false;
    }
    memset(&loaded, 0, sizeof(loaded));
    loaded.effects = effects.data;
    loaded.effects_size = effects.size;
    records = sdx.data + WW_WATER_SDX_HEADER_BYTES;
    if (!ww_water_parse_descriptor(
            &loaded.shallow,
            records + WW_WATER_SHALLOW_CLASS * WW_WATER_SDX_RECORD_BYTES,
            effects.size) ||
        !ww_water_parse_descriptor(
            &loaded.deep[0],
            records + WW_WATER_DEEP_CLASS_FIRST * WW_WATER_SDX_RECORD_BYTES,
            effects.size) ||
        !ww_water_parse_descriptor(
            &loaded.deep[1],
            records + WW_WATER_DEEP_CLASS_SECOND * WW_WATER_SDX_RECORD_BYTES,
            effects.size)) {
        return false;
    }
    loaded.loaded = true;
    *assets = loaded;
    return true;
}

void ww_water_state_reset(WwWaterState *state)
{
    if (state != NULL) memset(state, 0, sizeof(*state));
}

void ww_water_update(WwWaterState *state, uint32_t surface_class,
                     bool allow_entry)
{
    if (state == NULL) return;
    if (!state->submerged) {
        if (allow_entry && ww_water_is_deep(surface_class)) {
            state->shallow_active = false;
            state->shallow_frame = 0u;
            state->surface_class = (uint8_t)surface_class;
            state->splash_frame = 0u;
            state->periscope_frame = 0u;
            state->submerged = true;
            state->splash_active = true;
            state->splash_sound_pending = true;
        } else if (allow_entry && surface_class == WW_WATER_SHALLOW_CLASS) {
            if (!state->shallow_active) state->shallow_frame = 0u;
            state->shallow_active = true;
        } else {
            state->shallow_active = false;
            state->shallow_frame = 0u;
        }
        return;
    }

    state->shallow_active = false;
    state->shallow_frame = 0u;

    /* loc_26B34 retains the submerged state across class-three bank/wall
     * samples.  Any other class differing from the entry record resurfaces.
     * Shallow water therefore never starts or prolongs the periscope state. */
    if (surface_class != 3u && surface_class != state->surface_class) {
        state->submerged = false;
        state->splash_frame = 0u;
        state->splash_active = true;
        state->splash_sound_pending = true;
        if (allow_entry && surface_class == WW_WATER_SHALLOW_CLASS) {
            state->shallow_active = true;
        }
    }
}

bool ww_water_enqueue_periscope(const WwWaterAssets *assets,
                                const WwWaterState *state,
                                const WwRenderer *renderer,
                                const WwSpriteScaleSet *scale_set,
                                WwRenderQueue *queue)
{
    const WwWaterEffectDescriptor *descriptor;
    const WwSpriteScale *scale;
    WwRenderQueueItem item;
    size_t offset;
    if (assets == NULL || state == NULL || renderer == NULL ||
        scale_set == NULL || queue == NULL || !state->submerged) {
        return assets != NULL && state != NULL && renderer != NULL &&
               scale_set != NULL && queue != NULL;
    }
    descriptor = ww_water_descriptor(assets, state->surface_class);
    if (descriptor == NULL || state->periscope_frame >=
                                  descriptor->periscope_frames) {
        return false;
    }
    scale = &scale_set->level[0];
    if (scale->width != descriptor->periscope_width ||
        scale->height != descriptor->periscope_height) {
        return false;
    }
    offset = descriptor->periscope_offset +
             (size_t)state->periscope_frame * descriptor->periscope_stride;
    if (offset + descriptor->periscope_stride > assets->effects_size) {
        return false;
    }
    memset(&item, 0, sizeof(item));
    item.x = (int16_t)(WW_SCREEN_WIDTH / 2 - scale->width / 2);
    item.y = (int16_t)(
        (int)renderer->projection_right[WW_WATER_PLAYER_DISTANCE] -
        (int)scale->height);
    item.distance = WW_WATER_PLAYER_DISTANCE;
    item.source = assets->effects + offset;
    item.source_size = assets->effects_size - offset;
    item.scale = scale;
    return ww_render_queue_push(queue, &item);
}

bool ww_water_draw_splash(const WwWaterAssets *assets,
                          const WwWaterState *state, WwDisplay *display)
{
    const WwWaterEffectDescriptor *descriptor;
    size_t offset;
    if (assets == NULL || state == NULL || display == NULL) return false;
    if (!state->splash_active) return true;
    descriptor = ww_water_descriptor(assets, state->surface_class);
    if (descriptor == NULL || state->splash_frame >= descriptor->splash_frames) {
        return false;
    }
    offset = descriptor->splash_offset +
             (size_t)state->splash_frame * descriptor->splash_stride;
    if (offset + descriptor->splash_stride > assets->effects_size) {
        return false;
    }
    ww_display_blit_column_major(
        display, (WW_SCREEN_WIDTH - descriptor->splash_width) / 2,
        WW_WATER_SPLASH_SCREEN_Y, descriptor->splash_width,
        descriptor->splash_height, assets->effects + offset,
        descriptor->splash_height, 0);
    return true;
}

/* sub_24BCC loc_2513C identifies the 12x24 alternate terrain image and
 * loc_2518A/loc_251CA draw it at both rear wheels of the fixed 38x28 player
 * car.  The car has already been drawn when these transparent sprays land. */
bool ww_water_draw_shallow_spray(const WwWaterAssets *assets,
                                 const WwWaterState *state,
                                 const WwRenderer *renderer,
                                 const WwSpriteScaleSet *scale_set,
                                 WwDisplay *display)
{
    const WwWaterEffectDescriptor *descriptor;
    const WwSpriteScale *player_scale;
    size_t offset;
    int car_x;
    int car_y;
    if (assets == NULL || state == NULL || renderer == NULL ||
        scale_set == NULL || display == NULL) {
        return false;
    }
    if (!state->shallow_active) return true;
    descriptor = &assets->shallow;
    if (!assets->loaded || state->shallow_frame >=
                               descriptor->periscope_frames) {
        return false;
    }
    player_scale = &scale_set->level[0];
    if (player_scale->width != 38u || player_scale->height != 28u ||
        descriptor->periscope_width != 12u ||
        descriptor->periscope_height != 24u) {
        return false;
    }
    offset = descriptor->periscope_offset +
             (size_t)state->shallow_frame * descriptor->periscope_stride;
    if (offset + descriptor->periscope_stride > assets->effects_size) {
        return false;
    }
    car_x = WW_SCREEN_WIDTH / 2 - (int)player_scale->width / 2;
    car_y = (int)renderer->projection_right[WW_WATER_PLAYER_DISTANCE] -
            (int)player_scale->height;
    ww_display_blit_column_major(
        display, car_x + WW_WATER_SHALLOW_LEFT_X,
        car_y + WW_WATER_SHALLOW_Y, descriptor->periscope_width,
        descriptor->periscope_height, assets->effects + offset,
        descriptor->periscope_height, 0);
    ww_display_blit_column_major(
        display, car_x + WW_WATER_SHALLOW_RIGHT_X,
        car_y + WW_WATER_SHALLOW_Y, descriptor->periscope_width,
        descriptor->periscope_height, assets->effects + offset,
        descriptor->periscope_height, 0);
    return true;
}

void ww_water_render_step(WwWaterState *state,
                          const WwWaterAssets *assets)
{
    const WwWaterEffectDescriptor *descriptor;
    if (state == NULL || assets == NULL) return;
    if (state->shallow_active) {
        ++state->shallow_frame;
        if (state->shallow_frame >= assets->shallow.periscope_frames) {
            state->shallow_frame = 0u;
        }
    }
    descriptor = ww_water_descriptor(assets, state->surface_class);
    if (descriptor == NULL) return;
    if (state->submerged) {
        ++state->periscope_frame;
        if (state->periscope_frame >= descriptor->periscope_frames) {
            state->periscope_frame = 0u;
        }
    }
    if (state->splash_active) {
        ++state->splash_frame;
        if (state->splash_frame >= descriptor->splash_frames) {
            state->splash_frame = 0u;
            state->splash_active = false;
        }
    }
}
