#ifndef WW_COMMON_H
#define WW_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WW_SCREEN_WIDTH 320
#define WW_SCREEN_HEIGHT 200
#define WW_PALETTE_COLORS 256
#define WW_PALETTE_BYTES (WW_PALETTE_COLORS * 3)
#define WW_DISPLAY_PAGES 4
#define WW_GAME_TICK_HZ 136
#define WW_GAME_FRAME_TICKS 12
#define WW_ARCHIVE_EXPECTED_ENTRIES 577

typedef struct WwBuffer {
    uint8_t *data;
    size_t size;
} WwBuffer;

uint16_t ww_read_le16(const uint8_t *p);
uint32_t ww_read_le24(const uint8_t *p);
uint32_t ww_read_le32(const uint8_t *p);
uint16_t ww_read_be16(const uint8_t *p);
uint32_t ww_read_be32(const uint8_t *p);

bool ww_ascii_iequals(const char *a, const char *b);
bool ww_ascii_ends_with(const char *text, const char *suffix);
void ww_log(const char *format, ...);
void ww_error(const char *format, ...);

#endif
