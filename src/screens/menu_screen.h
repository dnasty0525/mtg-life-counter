/**
 * menu_screen.h — settings/utility screen: New Game, d20 roller, random
 * first-player picker, brightness/theme/LED settings, and the switches
 * that turn on poison/energy/storm/commander-damage tracking.
 *
 * Reached by swiping left on the counter screen; a Back button (or
 * encoder long-press) returns. It's a normal vertically-scrolling screen
 * (there's more content than fits one 240x240 view), so touch is the
 * primary way to use it; on the rotary board the knob scrolls the list
 * and you tap to interact with whatever's in view.
 */

#pragma once

void menu_screen_create();
void menu_screen_destroy();
