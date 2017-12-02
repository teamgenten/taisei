/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2017, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2017, Andrei Alexeyev <akari@alienslab.net>.
 */

#include "taisei.h"

#include <immintrin.h>
#include "util_sse42.h"

uint32_t crc32str_sse42(uint32_t crc, const char *str) {
    const uint8_t *s = (const uint8_t*)str;

    while(*s) {
        crc = _mm_crc32_u8(crc, *s++);
    }

    return crc;
}
