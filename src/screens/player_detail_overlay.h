/**
 * player_detail_overlay.h — modal detail view for one player: poison,
 * energy, and commander damage taken from each opponent. Only the
 * sections that are turned on (g_game.track_poison / track_energy /
 * track_commander_damage, set from the menu screen) are shown.
 *
 * Opened by long-pressing a player's life total on the counter screen
 * (touch only — both boards have touch, so this doesn't need an encoder
 * path). Closed with its Close button, which returns to whichever screen
 * was active before it opened.
 */

#pragma once

#include <stdint.h>

void player_detail_overlay_open(uint8_t player_idx);
void player_detail_overlay_close();
bool player_detail_overlay_is_open();

/* Same purpose as dice_overlay_discard() — call this from a screen's own
 * *_destroy() right before it deletes itself, if this overlay might still
 * be open on top of it. Clears the dangling static pointers without
 * scheduling a second delete of an object the screen's own teardown is
 * about to free anyway. */
void player_detail_overlay_discard();
