/**
 * table_sync.h — table sync over ESP-NOW: one device hosts, others join as
 * relays/mirrors. See sync_protocol.h for the wire format and why ESP-NOW
 * (not classic Bluetooth) carries it.
 *
 * Usage from the rest of the firmware:
 *   - main.cpp calls table_sync_init() once at boot and table_sync_poll()
 *     every loop() iteration (cheap no-op when idle).
 *   - menu_screen.cpp's Multiplayer section drives table_sync_host_start()/
 *     _start_scan()/_join()/_leave()/_host_stop() from button callbacks, and
 *     reads table_sync_role()/_status()/discovered list to draw itself.
 *   - app_state.cpp's mutators call table_sync_send_action() instead of
 *     mutating local state directly when this device is a joiner (see the
 *     guard added at the top of each of those functions).
 *   - table_sync.cpp itself calls the normal app_state_* functions to apply
 *     both local host actions and incoming host state/actions, and pokes
 *     the UI (counter_screen_refresh() / counter_screen_rebuild_if_active()
 *     / led_driver_refresh()) after doing so.
 */

#pragma once

#include <stdint.h>

typedef enum {
    SYNC_ROLE_NONE = 0,
    SYNC_ROLE_HOST,
    SYNC_ROLE_JOINER,
} sync_role_t;

typedef enum {
    SYNC_STATUS_IDLE = 0,     /* not hosting, not joined, not scanning */
    SYNC_STATUS_HOSTING,      /* role HOST, beaconing + accepting joiners */
    SYNC_STATUS_SCANNING,     /* role NONE, listening for host beacons */
    SYNC_STATUS_JOINING,      /* role NONE, join request sent, awaiting ack */
    SYNC_STATUS_JOINED,       /* role JOINER, actively synced to a host */
    SYNC_STATUS_JOIN_LOST,    /* role JOINER, host stopped beaconing/acking (timed out) */
} sync_status_t;

#define SYNC_MAX_DISCOVERED 6

typedef struct {
    uint8_t mac[6];
    char host_name[17]; /* SYNC_MAX_NAME_LEN + 1, always null-terminated here */
    uint8_t player_count;
    uint32_t table_id;
    uint32_t last_seen_ms;
} sync_discovered_table_t;

void table_sync_init();
/* Call every loop() iteration. Drains received ESP-NOW packets, runs the
 * periodic host beacon/state broadcast and joiner scan-prune/join-timeout
 * timers. Cheap no-op when idle (role NONE, not scanning). */
void table_sync_poll();

sync_role_t table_sync_role();
sync_status_t table_sync_status();

/* --- Host side --- */
/* Starts hosting under the given display name (truncated to 16 bytes).
 * Picks a random table_id, switches role to HOST, starts beaconing. No-op
 * if already hosting or joined. */
void table_sync_host_start(const char *name);
void table_sync_host_stop();
uint8_t table_sync_host_joiner_count();

/* --- Joiner side --- */
void table_sync_start_scan();
void table_sync_stop_scan();
uint8_t table_sync_discovered_count();
/* Returns nullptr if idx is out of range. Pointer is only valid until the
 * next table_sync_poll() call — copy out what you need. */
const sync_discovered_table_t *table_sync_discovered_get(uint8_t idx);
/* Sends a join request to the idx'th discovered table. device_name is this
 * device's own display name, truncated to 16 bytes. */
void table_sync_join(uint8_t idx, const char *device_name);
/* Leaves a joined table (or cancels an in-flight join), returns to IDLE. */
void table_sync_leave();

/* Called by app_state.cpp mutators when table_sync_role() == SYNC_ROLE_JOINER,
 * instead of mutating g_game locally — sends the request to the host, which
 * applies it and rebroadcasts the resulting state to everyone (including
 * back to this device). No-op if not currently joined. from_player_idx and
 * delta are only meaningful for some action types; pass 0 when unused. */
void table_sync_send_action(uint8_t action, uint8_t player_idx, uint8_t from_player_idx, int16_t delta);
