#include "ww_voc.h"

#include "ww_common.h"

#include <stdlib.h>
#include <string.h>

static bool ww_voc_append(WwVocPcm *pcm, const uint8_t *samples, size_t count)
{
    uint8_t *grown;
    if (count == 0) {
        return true;
    }
    if (pcm->sample_count > SIZE_MAX - count) {
        return false;
    }
    grown = (uint8_t *)realloc(pcm->samples, pcm->sample_count + count);
    if (grown == NULL) {
        return false;
    }
    pcm->samples = grown;
    memcpy(pcm->samples + pcm->sample_count, samples, count);
    pcm->sample_count += count;
    return true;
}

static bool ww_voc_append_silence(WwVocPcm *pcm, size_t count)
{
    uint8_t *grown;
    if (pcm->sample_count > SIZE_MAX - count) {
        return false;
    }
    grown = (uint8_t *)realloc(pcm->samples, pcm->sample_count + count);
    if (grown == NULL) {
        return false;
    }
    pcm->samples = grown;
    memset(pcm->samples + pcm->sample_count, 0x80, count);
    pcm->sample_count += count;
    return true;
}

bool ww_voc_decode(const uint8_t *data, size_t size, WwVocPcm *pcm)
{
    static const char signature[] = "Creative Voice File";
    size_t position;
    uint32_t current_rate = 0;
    uint32_t extended_rate = 0;
    unsigned extended_channels = 1;

    if (data == NULL || pcm == NULL || size < 26 ||
        memcmp(data, signature, sizeof(signature) - 1) != 0 || data[19] != 0x1a) {
        return false;
    }
    memset(pcm, 0, sizeof(*pcm));
    position = ww_read_le16(data + 20);
    if (position < 26 || position > size) {
        return false;
    }

    while (position < size) {
        uint8_t type = data[position++];
        uint32_t length;
        const uint8_t *block;
        if (type == 0) {
            break;
        }
        if (position + 3 > size) {
            ww_voc_free(pcm);
            return false;
        }
        length = ww_read_le24(data + position);
        position += 3;
        if (length > size - position) {
            ww_voc_free(pcm);
            return false;
        }
        block = data + position;

        switch (type) {
        case 1:
            if (length < 2 || block[1] != 0) {
                ww_voc_free(pcm);
                return false;
            }
            current_rate = extended_rate != 0
                               ? extended_rate
                               : 1000000u / (256u - block[0]);
            if (extended_channels != 1 || !ww_voc_append(pcm, block + 2, length - 2)) {
                ww_voc_free(pcm);
                return false;
            }
            if (pcm->sample_rate == 0) {
                pcm->sample_rate = current_rate;
            } else if (pcm->sample_rate != current_rate) {
                ww_voc_free(pcm);
                return false;
            }
            extended_rate = 0;
            extended_channels = 1;
            break;
        case 2:
            if (current_rate == 0 || !ww_voc_append(pcm, block, length)) {
                ww_voc_free(pcm);
                return false;
            }
            break;
        case 3:
            if (length < 3 || block[2] == 0xff) {
                ww_voc_free(pcm);
                return false;
            }
            current_rate = 1000000u / (256u - block[2]);
            if (pcm->sample_rate == 0) {
                pcm->sample_rate = current_rate;
            }
            if (pcm->sample_rate != current_rate ||
                !ww_voc_append_silence(pcm, (size_t)ww_read_le16(block) + 1u)) {
                ww_voc_free(pcm);
                return false;
            }
            break;
        case 6:
            pcm->loop = true;
            break;
        case 7:
            break;
        case 8:
            if (length < 4 || block[2] != 0) {
                ww_voc_free(pcm);
                return false;
            }
            extended_channels = block[3] + 1u;
            extended_rate = 256000000u /
                            (65536u - ww_read_le16(block)) /
                            extended_channels;
            break;
        case 9:
            if (length < 12 || block[4] != 8 || block[5] != 1 ||
                ww_read_le16(block + 6) != 0) {
                ww_voc_free(pcm);
                return false;
            }
            current_rate = ww_read_le32(block);
            if (pcm->sample_rate == 0) {
                pcm->sample_rate = current_rate;
            }
            if (pcm->sample_rate != current_rate ||
                !ww_voc_append(pcm, block + 12, length - 12)) {
                ww_voc_free(pcm);
                return false;
            }
            break;
        default:
            /* Text, marker and metadata blocks do not affect PCM output. */
            break;
        }
        position += length;
    }
    if (pcm->sample_rate == 0 || pcm->sample_count == 0) {
        ww_voc_free(pcm);
        return false;
    }
    return true;
}

void ww_voc_free(WwVocPcm *pcm)
{
    if (pcm == NULL) {
        return;
    }
    free(pcm->samples);
    memset(pcm, 0, sizeof(*pcm));
}

