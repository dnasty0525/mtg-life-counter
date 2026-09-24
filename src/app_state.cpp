#include "app_state.h"
#include "net/table_sync.h"
#include "net/sync_protocol.h"
#include <Arduino.h>

game_state_t g_game = {}; /* zero-initialized; app_state_reset_game() fills in real values */

/* When this device is a table-sync joiner, the host is the sole source of
 * truth for shared game state — a joiner's own button presses become
 * requests sent to the host (which applies them via these same functions,
 * then rebroadcasts the result back to everyone, including this device)
 * rather than local mutations. Screen code (counter_screen.cpp,
 * menu_screen.cpp, player_detail_overlay.cpp) calls these functions the
 * exact same way either way and doesn't need to know the difference. New
 * Game intentionally has no such guard — only the host ever starts a new
 * game (see menu_screen.cpp hiding that button for joiners). */
static bool relay_if_joiner(sync_action_t action, uint8_t player_idx, uint8_t from_player_idx, int16_t delta) {
    if (table_sync_role() != SYNC_ROLE_JOINER) return false;
    table_sync_send_action((uint8_t)action, player_idx, from_player_idx, delta);
    return true;
}

void app_state_reset_game(uint8_t player_count, int16_t starting_life) {
    if (player_count < 1) player_count = 1;
    if (player_count > MAX_PLAYERS) player_count = MAX_PLAYERS;

    g_game.player_count = player_count;
    g_game.starting_life = starting_life;
    g_game.selected_player = 0;
    g_game.storm_count = 0;
    /* track_poison/energy/storm/commander_damage are NOT reset here —
     * they're a settings-screen preference, not per-game state, so they
     * carry over from one game to the next. */

    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        g_game.players[i].life = starting_life;
        g_game.players[i].active = (i < player_count);
        g_game.players[i].poison = 0;
        g_game.players[i].energy = 0;
        for (uint8_t j = 0; j < MAX_PLAYERS; j++) {
            g_game.players[i].commander_damage[j] = 0;
        }
    }
}

void app_state_adjust_life(uint8_t player_idx, int16_t delta) {
    if (relay_if_joiner(SYNC_ACTION_ADJUST_LIFE, player_idx, 0, delta)) return;
    if (player_idx >= g_game.player_count) return;
    g_game.players[player_idx].life += delta;
}

void app_state_select_next_player() {
    if (relay_if_joiner(SYNC_ACTION_SELECT_NEXT_PLAYER, 0, 0, 0)) return;
    if (g_game.player_count <= 1) return;
    g_game.selected_player = (g_game.selected_player + 1) % g_game.player_count;
}

void app_state_adjust_poison(uint8_t player_idx, int16_t delta) {
    if (relay_if_joiner(SYNC_ACTION_ADJUST_POISON, player_idx, 0, delta)) return;
    if (player_idx >= g_game.player_count) return;
    int16_t v = g_game.players[player_idx].poison + delta;
    if (v < 0) v = 0;
    g_game.players[player_idx].poison = v;
}

void app_state_adjust_energy(uint8_t player_idx, int16_t delta) {
    if (relay_if_joiner(SYNC_ACTION_ADJUST_ENERGY, player_idx, 0, delta)) return;
    if (player_idx >= g_game.player_count) return;
    int16_t v = g_game.players[player_idx].energy + delta;
    if (v < 0) v = 0;
    g_game.players[player_idx].energy = v;
}

void app_state_adjust_commander_damage(uint8_t player_idx, uint8_t from_player_idx, int16_t delta) {
    if (relay_if_joiner(SYNC_ACTION_ADJUST_COMMANDER_DAMAGE, player_idx, from_player_idx, delta)) return;
    if (player_idx >= g_game.player_count) return;
    if (from_player_idx >= g_game.player_count) return;
    if (player_idx == from_player_idx) return;

    int16_t old_v = g_game.players[player_idx].commander_damage[from_player_idx];
    int16_t new_v = old_v + delta;
    if (new_v < 0) new_v = 0;
    g_game.players[player_idx].commander_damage[from_player_idx] = new_v;

    /* Commander damage comes straight off life, like any other hit. Use
     * the CLAMPED change (new_v - old_v), not the raw delta — e.g. delta
     * of -1 against a damage total already at 0 clamps to a no-op, and
     * life shouldn't move either in that case. */
    int16_t applied = new_v - old_v;
    if (applied != 0) {
        g_game.players[player_idx].life -= applied;
    }
}

void app_state_adjust_storm(int16_t delta) {
    if (relay_if_joiner(SYNC_ACTION_ADJUST_STORM, 0, 0, delta)) return;
    int16_t v = g_game.storm_count + delta;
    if (v < 0) v = 0;
    g_game.storm_count = v;
}

void app_state_reset_storm() {
    if (relay_if_joiner(SYNC_ACTION_RESET_STORM, 0, 0, 0)) return;
    g_game.storm_count = 0;
}

uint8_t app_state_pick_random_player() {
    if (g_game.player_count <= 1) return 0;
    /* Arduino's random(), seeded from the hardware RNG in main.cpp's
     * setup() via randomSeed(esp_random()) — not libc rand(), which
     * isn't seeded from anything meaningful on ESP32/Arduino. */
    return (uint8_t)random(g_game.player_count);
}
