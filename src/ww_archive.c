#include "ww_archive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    WW_ARCHIVE_DIRECTORY_ENTRY_BYTES = 22,
    WW_ARCHIVE_PAYLOAD_SKIP_BYTES = 2
};

static bool ww_read_file(const char *path, uint8_t **bytes, size_t *size)
{
    FILE *file;
    long length;
    uint8_t *result;

    *bytes = NULL;
    *size = 0;
    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    result = (uint8_t *)malloc((size_t)length);
    if (result == NULL || fread(result, 1, (size_t)length, file) != (size_t)length) {
        free(result);
        fclose(file);
        return false;
    }
    fclose(file);
    *bytes = result;
    *size = (size_t)length;
    return true;
}

bool ww_archive_open(WwArchive *archive, const char *path)
{
    uint16_t count;
    size_t directory_end;
    uint32_t previous_end = 0;
    uint16_t i;

    if (archive == NULL || path == NULL) {
        return false;
    }
    memset(archive, 0, sizeof(*archive));
    if (!ww_read_file(path, &archive->bytes, &archive->size)) {
        ww_error("cannot read archive %s", path);
        return false;
    }
    if (archive->size < 2) {
        ww_error("archive is shorter than its entry count");
        ww_archive_close(archive);
        return false;
    }

    count = ww_read_le16(archive->bytes);
    directory_end = 2u + (size_t)count * WW_ARCHIVE_DIRECTORY_ENTRY_BYTES;
    if (count == 0 || count > 4096 || directory_end > archive->size) {
        ww_error("invalid archive directory count %u", (unsigned)count);
        ww_archive_close(archive);
        return false;
    }

    archive->entries = (WwArchiveEntry *)calloc(count, sizeof(*archive->entries));
    if (archive->entries == NULL) {
        ww_archive_close(archive);
        return false;
    }
    archive->entry_count = count;
    strncpy(archive->path, path, sizeof(archive->path) - 1);

    for (i = 0; i < count; ++i) {
        const uint8_t *record = archive->bytes + 2u +
                                (size_t)i * WW_ARCHIVE_DIRECTORY_ENTRY_BYTES;
        WwArchiveEntry *entry = &archive->entries[i];
        size_t name_length = 0;
        uint64_t end;

        while (name_length < WW_ARCHIVE_NAME_BYTES && record[name_length] != 0) {
            entry->name[name_length] = (char)record[name_length];
            ++name_length;
        }
        entry->name[name_length] = '\0';
        entry->stored_size = ww_read_le32(record + 14);
        entry->offset = ww_read_le32(record + 18);
        end = (uint64_t)entry->offset + WW_ARCHIVE_PAYLOAD_SKIP_BYTES +
              entry->stored_size;

        if (entry->name[0] == '\0' || entry->stored_size == 0 ||
            entry->offset < directory_end || end > archive->size ||
            (i > 0 && entry->offset != previous_end)) {
            ww_error("invalid archive entry %u (%s)", (unsigned)i, entry->name);
            ww_archive_close(archive);
            return false;
        }
        entry->prefix = ww_read_le16(archive->bytes + entry->offset);
        /* Directory offsets advance by stored_size even though each logical
         * payload begins at offset+2.  The final two bytes therefore overlap
         * the next entry's skipped bytes.  sub_1418C seeks offset+2 and
         * sub_14238 returns the unmodified stored_size. */
        previous_end = entry->offset + entry->stored_size;
    }

    ww_log("opened %s: %u entries, %zu bytes", path, (unsigned)count, archive->size);
    return true;
}

void ww_archive_close(WwArchive *archive)
{
    if (archive == NULL) {
        return;
    }
    free(archive->entries);
    free(archive->bytes);
    memset(archive, 0, sizeof(*archive));
}

const WwArchiveEntry *ww_archive_find(const WwArchive *archive, const char *name)
{
    uint16_t i;
    if (archive == NULL || name == NULL) {
        return NULL;
    }
    for (i = 0; i < archive->entry_count; ++i) {
        if (ww_ascii_iequals(archive->entries[i].name, name)) {
            return &archive->entries[i];
        }
    }
    return NULL;
}

bool ww_archive_view_entry(const WwArchive *archive, const WwArchiveEntry *entry,
                           WwArchiveView *view)
{
    if (archive == NULL || entry == NULL || view == NULL ||
        (uint64_t)entry->offset + WW_ARCHIVE_PAYLOAD_SKIP_BYTES +
            entry->stored_size > archive->size) {
        return false;
    }
    view->data = archive->bytes + entry->offset + WW_ARCHIVE_PAYLOAD_SKIP_BYTES;
    view->size = entry->stored_size;
    view->entry = entry;
    return true;
}

bool ww_archive_view(const WwArchive *archive, const char *name, WwArchiveView *view)
{
    return ww_archive_view_entry(archive, ww_archive_find(archive, name), view);
}
