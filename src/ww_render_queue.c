#include "ww_render_queue.h"

#include "ww_common.h"

#include <string.h>

void ww_render_queue_init(WwRenderQueue *queue)
{
    if (queue != NULL) {
        memset(queue, 0, sizeof(*queue));
    }
}

bool ww_render_queue_push(WwRenderQueue *queue,
                          const WwRenderQueueItem *item)
{
    if (queue == NULL || item == NULL || item->source == NULL ||
        item->source_size == 0 || item->scale == NULL ||
        item->scale->offset_data == NULL ||
        queue->count >= WW_RENDER_QUEUE_CAPACITY) {
        return false;
    }
    queue->item[queue->count++] = *item;
    return true;
}

/* sub_253D8 sorts its 0x28-byte records by descending distance before
 * calling sub_37487.  Equal-distance entries retain producer order. */
bool ww_render_queue_draw(WwRenderQueue *queue,
                          uint8_t *pixels, size_t pitch)
{
    size_t i;
    if (queue == NULL || pixels == NULL || pitch < WW_SCREEN_WIDTH) {
        return false;
    }
    for (i = 1; i < queue->count; ++i) {
        WwRenderQueueItem item = queue->item[i];
        size_t destination = i;
        while (destination > 0 &&
               queue->item[destination - 1u].distance < item.distance) {
            queue->item[destination] = queue->item[destination - 1u];
            --destination;
        }
        queue->item[destination] = item;
    }
    for (i = 0; i < queue->count; ++i) {
        const WwRenderQueueItem *item = &queue->item[i];
        if (!ww_sprite_draw_scaled_column_major(
                pixels, pitch, WW_SCREEN_WIDTH, WW_SCREEN_HEIGHT,
                item->x, item->y, item->source, item->source_size,
                item->scale, 0)) {
            return false;
        }
    }
    return true;
}
