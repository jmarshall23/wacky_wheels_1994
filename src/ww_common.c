#include "ww_common.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

uint16_t ww_read_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

uint32_t ww_read_le24(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

uint32_t ww_read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint16_t ww_read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

uint32_t ww_read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

bool ww_ascii_iequals(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }
    while (*a != '\0' && *b != '\0') {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == *b;
}

bool ww_ascii_ends_with(const char *text, const char *suffix)
{
    size_t text_length;
    size_t suffix_length;
    if (text == NULL || suffix == NULL) {
        return false;
    }
    text_length = strlen(text);
    suffix_length = strlen(suffix);
    if (suffix_length > text_length) {
        return false;
    }
    return ww_ascii_iequals(text + text_length - suffix_length, suffix);
}

static void ww_vlog(FILE *stream, const char *prefix, const char *format, va_list args)
{
    fputs(prefix, stream);
    vfprintf(stream, format, args);
    fputc('\n', stream);
    fflush(stream);
}

void ww_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    ww_vlog(stdout, "ww95: ", format, args);
    va_end(args);
}

void ww_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    ww_vlog(stderr, "ww95 error: ", format, args);
    va_end(args);
}

