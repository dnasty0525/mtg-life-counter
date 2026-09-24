#include "table_sync.h"
#include "sync_protocol.h"
#include "../app_state.h"
#include "../screens/counter_screen.h"
#include "../screens/menu_screen.h"
#include "../screens/setup_screen.h"
#include "../led_driver.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* Max bytes any packet this protocol sends actually needs: header + the
 * largest payload (sync_state_payload_t, wrapped generously by
 * SYNC_MAX_PAYLOAD_SIZE in sync_protocol.h). */
#define SYNC_WIRE_MAX (sizeof(sync_header_t) + SYNC_MAX_PAYLOAD_SIZE)

/* One received ESP-NOW packet, copied out of the callback (which runs in
 * the WiFi driver's own task, not loop()'s) so it can be handled safely
 * from table_sync_poll() instead. */
typedef struct {
    uint8_t mac[6];
    uint8_t data[SYNC_WIRE_MAX];
    uint8_t len;
} sync_rx_packet_t;

static QueueHandle_t s_rx_queue = nullptr;

static sync_role_t s_role = SYNC_ROLE_NONE;
static sync_status_t s_status = SYNC_STATUS_IDLE;
static uint32_t s_table_id = 0;
static char s_host_name[SYNC_MAX_NAME_LEN] = {0};

/* Joiner-side */
static uint8_t s_host_mac[6] = {0};
static uint32_t s_last_state_rx_ms = 0;
static uint32_t s_join_request_sent_ms = 0;
static sync_discovered_table_t s_discovered[SYNC_MAX_DISCOVERED];
static uint8_t s_discovered_count = 0;

/* Host-side */
static uint8_t s_host_joiners[SYNC_MAX_DISCOVERED][6];
static uint8_t s_host_joiner_count = 0;
static uint32_t s_last_beacon_ms = 0;
static uint32_t s_last_state_broadcast_ms = 0;

static void ensure_peer(const uint8_t *mac) {
    if (esp_now_is_peer_exist(mac)) return;
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0; /* current channel */
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

static void send_packet(const uint8_t *mac, sync_msg_type_t type, uint32_t table_id,
                         const void *payload, uint8_t payload_len) {
    uint8_t buf[SYNC_WIRE_MAX];
    sync_header_t hdr;
    hdr.magic = SYNC_PROTOCOL_MAGIC;
    hdr.version = SYNC_PROTOCOL_VERSION;
    hdr.type = (uint8_t)type;
    hdr.table_id = table_id;
    memcpy(buf, &hdr, sizeof(hdr));
    if (payload && payload_len) {
        memcpy(buf + sizeof(hdr), payload, payload_len);
    }
    ensure_peer(mac);
    esp_now_send(mac, buf, sizeof(hdr) + payload_len);
}

/* Snapshots the shared parts of g_game into the wire-safe struct every
 * device agrees on. See sync_protocol.h for why this isn't just
 * game_state_t sent raw. */
static void build_state_payload(sync_state_payload_t *out) {
    memset(out, 0, sizeof(*out));
    uint8_t pc = g_game.player_count;
    if (pc > SYNC_MAX_PLAYERS) pc = SYNC_MAX_PLAYERS;

    out->player_count = pc;
    out->starting_life = g_game.starting_life;
    for (uint8_t i = 0; i < pc; i++) {
        out->life[i] = g_game.players[i].life;
        out->poison[i] = g_game.players[i].poison;
        out->energy[i] = g_game.players[i].energy;
        for (uint8_t j = 0; j < pc; j++) {
            out->commander_damage[i][j] = g_game.players[i].commander_damage[j];
        }
        out->player_led_color_idx[i] = g_game.player_led_color_idx[i];
    }
    out->selected_player = g_game.selected_player;
    out->track_flags = 0;
    if (g_game.track_poison) out->track_flags |= SYNC_TRACK_FLAG_POISON;
    if (g_game.track_energy) out->track_flags |= SYNC_TRACK_FLAG_ENERGY;
    if (g_game.track_storm) out->track_flags |= SYNC_TRACK_FLAG_STORM;
    if (g_game.track_commander_damage) out->track_flags |= SYNC_TRACK_FLAG_COMMANDER_DAMAGE;
    out->storm_count = g_game.storm_count;
}

/* Applies an incoming state snapshot (JOIN_ACK or STATE) directly to
 * g_game — used only on joiner devices, which treat the host as the sole
 * source of truth and never locally mutate life totals/counters
 * themselves (see the SYNC_ROLE_JOINER guards added to app_state.cpp). */
static void apply_state_payload(const sync_state_payload_t *in) {
    uint8_t pc = in->player_count;
    if (pc < 1) pc = 1;
    if (pc > MAX_PLAYERS) pc = MAX_PLAYERS;

    g_game.player_count = pc;
    g_game.starting_life = in->starting_life;
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        if (i >= pc) {
            g_game.players[i].life = 0;
            g_game.players[i].active = false;
            continue;
        }
        g_game.players[i].life = in->life[i];
        g_game.players[i].active = (in->life[i] > 0);
        g_game.players[i].poison = in->poison[i];
        g_game.players[i].energy = in->energy[i];
        for (uint8_t j = 0; j < MAX_PLAYERS; j++) {
            g_game.players[i].commander_damage[j] = (j < pc) ? in->commander_damage[i][j] : 0;
        }
        g_game.player_led_color_idx[i] = in->player_led_color_idx[i];
    }
    g_game.selected_player = (in->selected_player < pc) ? in->selected_player : 0;
    g_game.track_poison = (in->track_flags & SYNC_TRACK_FLAG_POISON) != 0;
    g_game.track_energy = (in->track_flags & SYNC_TRACK_FLAG_ENERGY) != 0;
    g_game.track_storm = (in->track_flags & SYNC_TRACK_FLAG_STORM) != 0;
    g_game.track_commander_damage = (in->track_flags & SYNC_TRACK_FLAG_COMMANDER_DAMAGE) != 0;
    g_game.storm_count = in->storm_count;
}

/* Pokes whichever UI is currently showing so a remotely-caused change
 * (host applying a joiner's action, a joiner receiving a state broadcast)
 * shows up immediately instead of waiting for the next local interaction.
 * Safe to call regardless of which screen is active or whether table_sync
 * is even in use — every function here already no-ops when its screen
 * isn't the one on display. */
static void refresh_local_ui() {
    counter_screen_rebuild_if_active();
    led_driver_refresh();
}

static void send_beacon() {
    sync_beacon_payload_t b;
    memset(&b, 0, sizeof(b));
    memcpy(b.host_name, s_host_name, SYNC_MAX_NAME_LEN);
    b.player_count = g_game.player_count;
    send_packet(BROADCAST_MAC, SYNC_MSG_HOST_BEACON, s_table_id, &b, sizeof(b));
}

static void broadcast_state_now() {
    sync_state_payload_t state;
    build_state_payload(&state);
    send_packet(BROADCAST_MAC, SYNC_MSG_STATE, s_table_id, &state, sizeof(state));
}

static void handle_beacon(const uint8_t *mac, uint32_t table_id, const uint8_t *payload, uint8_t len) {
    if (s_status != SYNC_STATUS_SCANNING) return;
    if (len < sizeof(sync_beacon_payload_t)) return;
    sync_beacon_payload_t b;
    memcpy(&b, payload, sizeof(b));

    for (uint8_t i = 0; i < s_discovered_count; i++) {
        if (memcmp(s_discovered[i].mac, mac, 6) == 0) {
            s_discovered[i].table_id = table_id;
            s_discovered[i].player_count = b.player_count;
            s_discovered[i].last_seen_ms = millis();
            return;
        }
    }
    if (s_discovered_count >= SYNC_MAX_DISCOVERED) return;

    sync_discovered_table_t *d = &s_discovered[s_discovered_count++];
    memcpy(d->mac, mac, 6);
    memcpy(d->host_name, b.host_name, SYNC_MAX_NAME_LEN);
    d->host_name[SYNC_MAX_NAME_LEN] = '\0'; /* host_name[] is 17 bytes, see table_sync.h */
    d->player_count = b.player_count;
    d->table_id = table_id;
    d->last_seen_ms = millis();
}

/* Host: a joiner wants in. Register them and hand back the current state —
 * joining is just "here's the table as it stands right now" (sync_protocol.h). */
static void handle_join_request(const uint8_t *mac, const uint8_t *payload, uint8_t len) {
    if (len < sizeof(sync_join_request_payload_t)) return;
    ensure_peer(mac);

    bool known = false;
    for (uint8_t i = 0; i < s_host_joiner_count; i++) {
        if (memcmp(s_host_joiners[i], mac, 6) == 0) { known = true; break; }
    }
    if (!known && s_host_joiner_count < SYNC_MAX_DISCOVERED) {
        memcpy(s_host_joiners[s_host_joiner_count++], mac, 6);
    }

    sync_state_payload_t state;
    build_state_payload(&state);
    send_packet(mac, SYNC_MSG_JOIN_ACK, s_table_id, &state, sizeof(state));
}

/* Host: apply a joiner's requested change the same way a local button
 * press would, then tell everyone (including that joiner) the resulting
 * state — the host never trusts a joiner's own math, only its intent. */
static void handle_action(const uint8_t *payload, uint8_t len) {
    if (len < sizeof(sync_action_payload_t)) return;
    sync_action_payload_t a;
    memcpy(&a, payload, sizeof(a));

    switch ((sync_action_t)a.action) {
        case SYNC_ACTION_ADJUST_LIFE:
            app_state_adjust_life(a.player_idx, a.delta);
            break;
        case SYNC_ACTION_SELECT_NEXT_PLAYER:
            app_state_select_next_player();
            break;
        case SYNC_ACTION_ADJUST_POISON:
            app_state_adjust_poison(a.player_idx, a.delta);
            break;
        case SYNC_ACTION_ADJUST_ENERGY:
            app_state_adjust_energy(a.player_idx, a.delta);
            break;
        case SYNC_ACTION_ADJUST_COMMANDER_DAMAGE:
            app_state_adjust_commander_damage(a.player_idx, a.from_player_idx, a.delta);
            break;
        case SYNC_ACTION_ADJUST_STORM:
            app_state_adjust_storm(a.delta);
            break;
        case SYNC_ACTION_RESET_STORM:
            app_state_reset_storm();
            break;
    }

    refresh_local_ui();
    broadcast_state_now();
}

static void handle_leave(const uint8_t *mac) {
    for (uint8_t i = 0; i < s_host_joiner_count; i++) {
        if (memcmp(s_host_joiners[i], mac, 6) == 0) {
            for (uint8_t k = i; k < s_host_joiner_count - 1; k++) {
                memcpy(s_host_joiners[k], s_host_joiners[k + 1], 6);
            }
            s_host_joiner_count--;
            return;
        }
    }
}

/* Joiner: the host just said yes. This is the one place in the firmware
 * that reliably knows "we just joined a table right now" (as opposed to
 * "we're receiving a routine state update"), so it's also where we jump
 * straight to the counter screen — the same create-then-destroy ordering
 * every other screen transition in this codebase uses, just triggered by
 * a network event instead of a button. */
static void handle_join_ack(const uint8_t *mac, uint32_t table_id, const uint8_t *payload, uint8_t len) {
    if (len < sizeof(sync_join_ack_payload_t)) return;

    sync_join_ack_payload_t state;
    memcpy(&state, payload, sizeof(state));

    memcpy(s_host_mac, mac, 6);
    ensure_peer(mac);
    s_table_id = table_id;
    s_role = SYNC_ROLE_JOINER;
    s_status = SYNC_STATUS_JOINED;
    s_last_state_rx_ms = millis();

    apply_state_payload(&state);

    app_screen_t prev = g_game.current_screen;
    counter_screen_create();
    if (prev == SCREEN_MENU) {
        menu_screen_destroy();
    } else if (prev == SCREEN_SETUP) {
        setup_screen_destroy();
    }
    /* Any other prev (already on the counter screen, an overlay) needs no
     * teardown call here — counter_screen_create() above already loaded
     * the new screen, and an overlay's parent will get cleaned up the next
     * time it's dismissed or replaced, same as any other navigation. */
}

/* Joiner: periodic/on-change full-state push from the host while already
 * joined — apply it and refresh whatever's on screen. */
static void handle_state(const uint8_t *payload, uint8_t len) {
    if (len < sizeof(sync_state_payload_t)) return;
    sync_state_payload_t state;
    memcpy(&state, payload, sizeof(state));
    apply_state_payload(&state);
    s_last_state_rx_ms = millis();
    if (s_status == SYNC_STATUS_JOIN_LOST) s_status = SYNC_STATUS_JOINED;
    refresh_local_ui();
}

static void handle_packet(const uint8_t *mac, const uint8_t *data, uint8_t len) {
    if (len < sizeof(sync_header_t)) return;
    sync_header_t hdr;
    memcpy(&hdr, data, sizeof(hdr));
    if (hdr.magic != SYNC_PROTOCOL_MAGIC) return;
    if (hdr.version != SYNC_PROTOCOL_VERSION) return;

    const uint8_t *payload = data + sizeof(hdr);
    uint8_t payload_len = len - sizeof(hdr);

    switch ((sync_msg_type_t)hdr.type) {
        case SYNC_MSG_HOST_BEACON:
            if (s_role != SYNC_ROLE_HOST) handle_beacon(mac, hdr.table_id, payload, payload_len);
            break;
        case SYNC_MSG_JOIN_REQUEST:
            if (s_role == SYNC_ROLE_HOST && hdr.table_id == s_table_id) handle_join_request(mac, payload, payload_len);
            break;
        case SYNC_MSG_JOIN_ACK:
            if (s_role != SYNC_ROLE_HOST && s_status == SYNC_STATUS_JOINING) handle_join_ack(mac, hdr.table_id, payload, payload_len);
            break;
        case SYNC_MSG_ACTION:
            if (s_role == SYNC_ROLE_HOST && hdr.table_id == s_table_id) handle_action(payload, payload_len);
            break;
        case SYNC_MSG_STATE:
            if (s_role == SYNC_ROLE_JOINER && hdr.table_id == s_table_id) handle_state(payload, payload_len);
            break;
        case SYNC_MSG_LEAVE:
            if (s_role == SYNC_ROLE_HOST && hdr.table_id == s_table_id) handle_leave(mac);
            break;
    }
}

/* ESP-NOW's receive callback runs in the WiFi driver's own task, not
 * loop()'s — touching g_game or any LVGL object directly from here would
 * race the main task. Copy the packet out and hand it to table_sync_poll()
 * via a queue instead. The callback signature itself changed between
 * arduino-esp32 core 2.x (esp-idf 4.x: raw uint8_t* mac) and 3.x (esp-idf
 * 5.x: esp_now_recv_info_t* wrapping it) — branch on the IDF major version
 * so this file builds against either core. */
#if ESP_IDF_VERSION_MAJOR >= 5
static void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const uint8_t *mac = info->src_addr;
#else
static void on_data_recv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
    if (!s_rx_queue) return;
    if (len <= 0 || (size_t)len > SYNC_WIRE_MAX) return;

    sync_rx_packet_t pkt;
    memcpy(pkt.mac, mac, 6);
    memcpy(pkt.data, data, len);
    pkt.len = (uint8_t)len;
    /* Non-blocking send: if the queue is somehow full (poll() stalled),
     * drop the packet rather than blocking the WiFi task. Both a lost
     * beacon and a lost state broadcast self-heal via the next one a
     * second later; a lost action just means the button press appears to
     * do nothing and the player tries again. */
    xQueueSend(s_rx_queue, &pkt, 0);
}

void table_sync_init() {
    s_rx_queue = xQueueCreate(16, sizeof(sync_rx_packet_t));

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[table_sync] esp_now_init() failed");
        return;
    }
    esp_now_register_recv_cb(on_data_recv);
    ensure_peer(BROADCAST_MAC);

    Serial.println("[table_sync] ready");
}

void table_sync_poll() {
    if (!s_rx_queue) return;

    sync_rx_packet_t pkt;
    while (xQueueReceive(s_rx_queue, &pkt, 0) == pdTRUE) {
        handle_packet(pkt.mac, pkt.data, pkt.len);
    }

    uint32_t now = millis();

    if (s_role == SYNC_ROLE_HOST) {
        if (now - s_last_beacon_ms >= 1000) {
            s_last_beacon_ms = now;
            send_beacon();
        }
        if (now - s_last_state_broadcast_ms >= 1000) {
            s_last_state_broadcast_ms = now;
            broadcast_state_now();
        }
        return;
    }

    switch (s_status) {
        case SYNC_STATUS_SCANNING:
            for (uint8_t i = 0; i < s_discovered_count;) {
                if (now - s_discovered[i].last_seen_ms > 5000) {
                    for (uint8_t k = i; k < (uint8_t)(s_discovered_count - 1); k++) {
                        s_discovered[k] = s_discovered[k + 1];
                    }
                    s_discovered_count--;
                } else {
                    i++;
                }
            }
            break;
        case SYNC_STATUS_JOINING:
            if (now - s_join_request_sent_ms > 3000) {
                /* No JOIN_ACK in time — fall back to scanning so the
                 * device's own discovered-table list stays live rather
                 * than getting stuck on a dead attempt. */
                s_status = SYNC_STATUS_SCANNING;
            }
            break;
        case SYNC_STATUS_JOINED:
            if (now - s_last_state_rx_ms > 5000) {
                /* Host stopped beaconing/broadcasting (out of range,
                 * powered off, stopped hosting) — say so rather than
                 * silently showing stale numbers forever. */
                s_status = SYNC_STATUS_JOIN_LOST;
            }
            break;
        default:
            break;
    }
}

sync_role_t table_sync_role() { return s_role; }
sync_status_t table_sync_status() { return s_status; }

void table_sync_host_start(const char *name) {
    if (s_role != SYNC_ROLE_NONE) return;

    memset(s_host_name, 0, sizeof(s_host_name));
    strncpy(s_host_name, name, SYNC_MAX_NAME_LEN);

    s_table_id = (uint32_t)esp_random();
    s_host_joiner_count = 0;
    s_role = SYNC_ROLE_HOST;
    s_status = SYNC_STATUS_HOSTING;
    s_last_beacon_ms = 0; /* fire on the very next poll() */
    s_last_state_broadcast_ms = 0;
}

void table_sync_host_stop() {
    if (s_role != SYNC_ROLE_HOST) return;
    /* No explicit "table closing" message — joiners self-detect via the
     * same 5s JOIN_LOST timeout used for any other disconnect. Keeping
     * one code path for "host is gone" (rather than a special-cased
     * message plus the timeout) keeps this simpler, at the cost of a
     * joiner taking up to 5s to notice a deliberate stop. */
    s_role = SYNC_ROLE_NONE;
    s_status = SYNC_STATUS_IDLE;
    s_host_joiner_count = 0;
}

uint8_t table_sync_host_joiner_count() {
    return s_host_joiner_count;
}

void table_sync_start_scan() {
    if (s_role != SYNC_ROLE_NONE) return;
    s_discovered_count = 0;
    s_status = SYNC_STATUS_SCANNING;
}

void table_sync_stop_scan() {
    if (s_status != SYNC_STATUS_SCANNING) return;
    s_status = SYNC_STATUS_IDLE;
}

uint8_t table_sync_discovered_count() {
    return s_discovered_count;
}

const sync_discovered_table_t *table_sync_discovered_get(uint8_t idx) {
    if (idx >= s_discovered_count) return nullptr;
    return &s_discovered[idx];
}

void table_sync_join(uint8_t idx, const char *device_name) {
    if (idx >= s_discovered_count) return;
    if (s_role != SYNC_ROLE_NONE) return;

    const sync_discovered_table_t *d = &s_discovered[idx];
    memcpy(s_host_mac, d->mac, 6);
    s_table_id = d->table_id;
    ensure_peer(d->mac);

    sync_join_request_payload_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.device_name, device_name, SYNC_MAX_NAME_LEN);
    send_packet(d->mac, SYNC_MSG_JOIN_REQUEST, s_table_id, &req, sizeof(req));

    s_status = SYNC_STATUS_JOINING;
    s_join_request_sent_ms = millis();
}

void table_sync_leave() {
    if (s_role == SYNC_ROLE_JOINER) {
        send_packet(s_host_mac, SYNC_MSG_LEAVE, s_table_id, nullptr, 0);
    }
    s_role = SYNC_ROLE_NONE;
    s_status = SYNC_STATUS_IDLE;
    s_discovered_count = 0;
}

void table_sync_send_action(uint8_t action, uint8_t player_idx, uint8_t from_player_idx, int16_t delta) {
    if (s_role != SYNC_ROLE_JOINER || s_status != SYNC_STATUS_JOINED) return;

    sync_action_payload_t a;
    a.action = action;
    a.player_idx = player_idx;
    a.from_player_idx = from_player_idx;
    a.delta = delta;
    send_packet(s_host_mac, SYNC_MSG_ACTION, s_table_id, &a, sizeof(a));
}
