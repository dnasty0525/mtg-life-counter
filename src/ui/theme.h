/**
 * theme.h — shared colors/sizing so every screen looks consistent.
 *
 * Most colors are static #defines — only the accent color is a runtime
 * "theme", cyclable from the menu screen, since a full recolor engine is
 * overkill for a life counter and a single accent swap already gives a
 * meaningfully different look.
 */

#pragma once

#include <lvgl.h>

#define THEME_BG            lv_color_hex(0x101014)
#define THEME_PANEL         lv_color_hex(0x1c1c22)
#define THEME_LIFE_TEXT     lv_color_hex(0xf5f5f0)
#define THEME_DANGER        lv_color_hex(0xb23a2f)
#define THEME_SELECTED_RING lv_color_hex(0xffffff)
#define THEME_POISON        lv_color_hex(0x4caf50)
#define THEME_ENERGY        lv_color_hex(0x29b6f6)

#define THEME_BTN_PLUS      lv_color_hex(0x2f6b3a)
#define THEME_BTN_MINUS     lv_color_hex(0x6b2f2f)

#define THEME_PRESET_COUNT 4

/* Current accent color — replaces the old THEME_ACCENT macro. Every
 * screen that used to write THEME_ACCENT now calls theme_accent(). */
lv_color_t theme_accent();

/* Advances to the next preset (wraps around) and returns its name. */
const char *theme_cycle();

/* Name of the currently active preset, for the settings screen label. */
const char *theme_name();
