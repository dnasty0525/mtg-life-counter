/**
 * led_colors.h — small named RGB palette shared by every LED color
 * picker in the UI (the ambient/base color, and each player's turn
 * color), so they all cycle through the same list instead of each screen
 * inventing its own.
 *
 * The first four entries are the defaults main.cpp assigns to players
 * 1-4 at boot — chosen to be easy to tell apart at a glance.
 */

#pragma once

#include <stdint.h>

typedef struct {
    const char *name;
    uint8_t r, g, b;
} led_color_t;

#define LED_COLOR_COUNT 8
extern const led_color_t LED_COLORS[LED_COLOR_COUNT];
