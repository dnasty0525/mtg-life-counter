/**
 * dice_overlay.h — modal d20 roller, opened from the counter screen
 * (tap the dice icon on touch, or long-press the knob on the rotary board).
 */

#pragma once

#include <lvgl.h>

void dice_overlay_open();
void dice_overlay_close();
bool dice_overlay_is_open();

/* Called by the shared encoder handler when the overlay is on screen. */
void dice_overlay_handle_roll();

/* Called by whichever screen owns the overlay's parent object, right
 * before that screen tears itself down (counter_screen_destroy() /
 * menu_screen_destroy()). The overlay is a *child* of that screen, so
 * the screen's own delete already frees the overlay's widgets — this
 * just clears dice_overlay's own static pointers so nothing keeps
 * treating the (now-freed) overlay as still open. Unlike
 * dice_overlay_close(), this does NOT schedule another delete of its
 * own — doing that too would double-free once the parent's delete runs. */
void dice_overlay_discard();
