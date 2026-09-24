/**
 * app_state.h — game state shared by every screen: player count/life
 * totals, which screen is active, the currently "selected" player (used
 * by the rotary board, where turning the knob must know which player's
 * life it's adjusting), and the optional extra counters (poison, energy,
 * storm, commander damage) toggled on from the menu screen.
 */

#pragma once

#include <stdint.h>

#define MAX_PLAYERS 4
#define DEFAULT_START_LIFE 40   /* Commander default; setup screen can change it */
#define POISON_DEATH_THRESHOLD 10

typedef enum {
    SCREEN_SETUP = 0,
    SCREEN_COUNTER,
    SCREEN_MENU,
    SCREEN_DICE_OVERLAY,
    SCREEN_PLAYER_DETAIL_OVERLAY,
} app_screen_t;

typedef struct {
    int16_t life;
    bool active;              /* false once a player is eliminated (life <= 0), still shown greyed out */
    int16_t poison;
    int16_t energy;
    /* commander_damage[from] = damage this player has taken from
     * players[from]'s commander. Self-index unused. 21+ is commander-kill
     * in the paper rules; we just display the number and let players
     * apply the rule themselves. */
    int16_t commander_damage[MAX_PLAYERS];
} player_state_t;

typedef struct {
    uint8_t player_count;         /* 1..MAX_PLAYERS, chosen on the setup screen */
    int16_t starting_life;        /* chosen on the setup screen */
    player_state_t players[MAX_PLAYERS];
    uint8_t selected_player;      /* index into players[]; drives encoder-based adjustment */
    app_screen_t current_screen;

    /* Which extra counters are turned on, set from the menu screen. */
    bool track_poison;
    bool track_energy;
    bool track_storm;
    bool track_commander_damage;

    /* Storm count is a single shared count for the turn (not per player),
     * matching how the rule actually works — manually reset each turn. */
    int16_t storm_count;

    /* Per-player accent color for the ambient LED strip (Meshnology board
     * only), shown on that player's turn when LED turn-colors are turned
     * on from the menu screen. Index into LED_COLORS (led_colors.h). A
     * settings preference like track_poison, so app_state_reset_game()
     * leaves it alone; main.cpp seeds distinct defaults once at boot. */
    uint8_t player_led_color_idx[MAX_PLAYERS];
} game_state_t;

extern game_state_t g_game;

void app_state_reset_game(uint8_t player_count, int16_t starting_life);
void app_state_adjust_life(uint8_t player_idx, int16_t delta);
void app_state_select_next_player();

void app_state_adjust_poison(uint8_t player_idx, int16_t delta);
void app_state_adjust_energy(uint8_t player_idx, int16_t delta);
/* Also moves player_idx's life total by -delta — commander damage is
 * still damage, so taking more of it costs life the same way any other
 * hit would, and undoing a misclick (delta<0) gives that life back. */
void app_state_adjust_commander_damage(uint8_t player_idx, uint8_t from_player_idx, int16_t delta);
void app_state_adjust_storm(int16_t delta);
void app_state_reset_storm();

/* Returns a random valid player index in [0, player_count). Uses the
 * hardware RNG seeded once at boot (see main.cpp). */
uint8_t app_state_pick_random_player();
