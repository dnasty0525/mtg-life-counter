/**
 * dice_overlay.h — modal dice roller, opened from the counter screen
 * (long-press the knob on the rotary board) or from the menu screen's
 * "Roll d20" / "Roll 2d6" buttons.
 *
 * Two modes, switchable while the overlay is open: a single d20, or two
 * six-sided dice (shown as a total plus the two individual faces).
 */

#pragma once

#include <lvgl.h>

typedef enum {
    DICE_MODE_D20 = 0,
    DICE_MODE_2D6 = 1,
} dice_mode_t;

/* Opens the overlay in the given mode. With no argument it reopens in
 * whichever mode was used last (so a rotary-board user who switched to
 * 2d6 once doesn't have to switch again every long-press). */
void dice_overlay_open();
void dice_overlay_open(dice_mode_t mode);
void dice_overlay_close();
bool dice_overlay_is_open();

/* Called by the shared encoder handler when the overlay is on screen. */
void dice_overlay_handle_roll();

/* Switches d20 <-> 2d6 (the encoder's turn action while the overlay is
 * open). Ignored mid-roll. */
void dice_overlay_toggle_mode();

/* Called by whichever screen owns the overlay's parent object, right
 * before that screen tears itself down (counter_screen_destroy() /
 * menu_screen_destroy()). The overlay is a *child* of that screen, so
 * the screen's own delete already frees the overlay's widgets — this
 * just clears dice_overlay's own static pointers so nothing keeps
 * treating the (now-freed) overlay as still open. Unlike
 * dice_overlay_close(), this does NOT schedule another delete of its
 * own — doing that too would double-free once the parent's delete runs. */
void dice_overlay_discard();
