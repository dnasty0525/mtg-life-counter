/**
 * counter_screen.h — the main life-total screen. Layout adapts to
 * g_game.player_count (1-4), set on the setup screen.
 */

#pragma once

void counter_screen_create();
void counter_screen_destroy();

/* Refreshes all life labels from g_game — call after any adjustment made
 * outside a widget event (e.g. from the encoder handler). */
void counter_screen_refresh();

/* Call after table-sync applies a remote state/action update. No-op if
 * this screen isn't currently active. If g_game.player_count still
 * matches what's built, this is just counter_screen_refresh(); if a
 * remote New Game changed the player count out from under the built
 * layout, it does a full safe rebuild (new screen created and loaded,
 * then the old one async-deleted) instead. */
void counter_screen_rebuild_if_active();
