#ifndef WW_ARCHIVE_H
#define WW_ARCHIVE_H

#include "ww_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WW_ARCHIVE_NAME_BYTES 14

typedef struct WwArchiveEntry {
    char name[WW_ARCHIVE_NAME_BYTES + 1];
    uint32_t stored_size;
    uint32_t offset;
    uint16_t prefix;
} WwArchiveEntry;

typedef struct WwArchiveView {
    const uint8_t *data;
    size_t size;
    const WwArchiveEntry *entry;
} WwArchiveView;

typedef struct WwArchive {
    uint8_t *bytes;
    size_t size;
    WwArchiveEntry *entries;
    uint16_t entry_count;
    char path[1024];
} WwArchive;

bool ww_archive_open(WwArchive *archive, const char *path);
void ww_archive_close(WwArchive *archive);
const WwArchiveEntry *ww_archive_find(const WwArchive *archive, const char *name);
bool ww_archive_view(const WwArchive *archive, const char *name, WwArchiveView *view);
bool ww_archive_view_entry(const WwArchive *archive, const WwArchiveEntry *entry,
                           WwArchiveView *view);

#endif

