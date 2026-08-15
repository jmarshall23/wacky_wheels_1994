#ifndef WW_RENDER_QUEUE_H
#define WW_RENDER_QUEUE_H

#include "ww_sprite.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WW_RENDER_QUEUE_CAPACITY = 128
};

typedef struct WwRenderQueueItem {
    int16_t x;
    int16_t y;
    uint16_t distance;
    const uint8_t *source;
    size_t source_size;
    const WwSpriteScale *scale;
} WwRenderQueueItem;

typedef struct WwRenderQueue {
    WwRenderQueueItem item[WW_RENDER_QUEUE_CAPACITY];
    size_t count;
} WwRenderQueue;

void ww_render_queue_init(WwRenderQueue *queue);
bool ww_render_queue_push(WwRenderQueue *queue,
                          const WwRenderQueueItem *item);
bool ww_render_queue_draw(WwRenderQueue *queue,
                          uint8_t *pixels, size_t pitch);

#endif
