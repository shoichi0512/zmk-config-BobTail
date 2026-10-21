/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdint.h>

#define BOBTAIL_SCROLL_LEVEL_COUNT 5
#define BOBTAIL_SCROLL_DEFAULT_LEVEL 2

/* More sensor counts per wheel step means slower scrolling. */
static inline int bobtail_scroll_level_tick(uint8_t level) {
    static const int ticks[BOBTAIL_SCROLL_LEVEL_COUNT] = {64, 32, 16, 8, 4};
    return ticks[level < BOBTAIL_SCROLL_LEVEL_COUNT ? level : BOBTAIL_SCROLL_DEFAULT_LEVEL];
}

int bobtail_scroll_tick_get(void);
