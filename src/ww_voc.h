#ifndef WW_VOC_H
#define WW_VOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct WwVocPcm {
    uint8_t *samples;
    size_t sample_count;
    uint32_t sample_rate;
    bool loop;
} WwVocPcm;

bool ww_voc_decode(const uint8_t *data, size_t size, WwVocPcm *pcm);
void ww_voc_free(WwVocPcm *pcm);

#endif

