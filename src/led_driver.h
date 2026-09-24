/**
 * led_driver.h — ambient WS2812 RGB LED strip control (Meshnology/
 * CrowPanel S3 board only — PIN_RGB_LED is undefined on boards without
 * one, and every function here is a safe no-op / harmless default in
 * that case so callers don't need to #ifdef every call site).
 *
 * Owns the strip's runtime state (on/off, brightness, ambient color, and
 * whether it's following the active player's turn color) so the menu
 * screen (settings UI) and the counter screen (turn changes) can both
 * drive the same strip without reaching into each other's statics —
 * they just call the setters below, then led_driver_refresh().
 */

#pragma once

#include <stdint.h>

void led_driver_init();

/* Low-level: push a color to the whole strip right now. Prefer the
 * setters + led_driver_refresh() below, which remember state across
 * calls and pick the right color automatically; this is what they call
 * internally, and it's still here for anything that wants one-shot
 * direct control. */
void led_driver_set(bool on, uint8_t brightness /* 0-255 */, uint8_t r, uint8_t g, uint8_t b);

/* True only on boards that actually have the strip wired up — lets the
 * menu screen decide whether to show LED controls at all. */
bool led_driver_available();

void led_driver_set_enabled(bool on);
bool led_driver_is_enabled();

void led_driver_set_brightness(uint8_t brightness);
uint8_t led_driver_get_brightness();

/* The strip's color when NOT showing a per-player turn color (turn
 * colors switched off from the menu). Index into LED_COLORS
 * (led_colors.h); out-of-range values are clamped back to 0. */
void led_driver_set_ambient_color_idx(uint8_t idx);
uint8_t led_driver_get_ambient_color_idx();

void led_driver_set_turn_colors_enabled(bool enabled);
bool led_driver_is_turn_colors_enabled();

/* Recomputes and pushes the strip's actual color from the settings
 * above — if turn colors are on, from the active player's assigned color
 * (g_game.player_led_color_idx[g_game.selected_player]) instead of the
 * ambient color. Call this after changing any setting above, and after
 * app_state_select_next_player() changes whose turn it is. */
void led_driver_refresh();
