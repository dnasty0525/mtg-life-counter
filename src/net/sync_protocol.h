/**
 * sync_protocol.h — wire format for table sync between devices, sent as
 * raw ESP-NOW payloads.
 *
 * ESP-NOW (not classic Bluetooth/BLE) is what actually carries this: it's
 * Espressif's own low-latency broadcast/unicast protocol that rides on
 * the WiFi radio both boards already have, and needs no pairing screen or
 * GATT server/client setup — a device just sends a packet to a MAC
 * address (or the broadcast address) and whoever's listening gets it.
 * That's a much better fit for "a few ESP32s on the same table" than
 * real Bluetooth would be, at the cost of not being controllable from a
 * phone (a future phone companion would need actual BLE or WiFi+a
 * server, which is a different project).
 *
 * The Meshnology board is Xtensa (S3) and the DIYmalls board is RISC-V
 * (C3) — different architectures compiling this same header. Struct
 * layout/padding rules aren't guaranteed identical across them, so every
 * wire struct here is explicitly `packed` and built only from
 * fixed-width stdint types. Never send a raw, unpacked C++ struct
 * (game_state_t, etc.) over the wire — always go through one of these.
 */

#pragma once

#include <stdint.h>

/* Bumped whenever a struct below changes shape. A device ignores any
 * packet whose version doesn't match its own, rather than trying to
 * partially interpret a layout it doesn't understand. */
#define SYNC_PROTOCOL_VERSION 1

/* Rejects stray ESP-NOW traffic from anything that isn't this firmware
 * (someone else's ESP-NOW project in range, corrupted noise, etc.)
 * before trusting a single other byte of the packet. Spells "MTGC" in
 * ASCII, no meaning beyond being a recognizable, unlikely-to-collide
 * constant. */
#define SYNC_PROTOCOL_MAGIC 0x4D544743UL

#define SYNC_MAX_NAME_LEN 16

typedef enum : uint8_t {
    SYNC_MSG_HOST_BEACON = 1, /* host -> broadcast, periodic "a table exists" announcement */
    SYNC_MSG_JOIN_REQUEST = 2, /* joiner -> host (unicast), "let me in" */
    SYNC_MSG_JOIN_ACK = 3,     /* host -> joiner (unicast), "you're in" + full state */
    SYNC_MSG_ACTION = 4,       /* joiner -> host (unicast), "please apply this" */
    SYNC_MSG_STATE = 5,        /* host -> broadcast, authoritative full state */
    SYNC_MSG_LEAVE = 6,        /* joiner -> host (unicast), "I'm leaving" */
} sync_msg_type_t;

/* Every packet starts with this. */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;    /* SYNC_PROTOCOL_MAGIC */
    uint8_t version;   /* SYNC_PROTOCOL_VERSION */
    uint8_t type;      /* sync_msg_type_t */
    /* Random ID the host picks when it starts hosting. Ties every packet
     * to one specific hosting session, so a joiner that hears two
     * different tables' beacons (two tables in ESP-NOW range at once)
     * can tell them apart, and so a STATE broadcast that arrives after
     * you've already left doesn't get applied. */
    uint32_t table_id;
} sync_header_t;
#pragma pack(pop)

/* SYNC_MSG_HOST_BEACON payload. */
#pragma pack(push, 1)
typedef struct {
    char host_name[SYNC_MAX_NAME_LEN]; /* not necessarily null-terminated if it fills the buffer */
    uint8_t player_count;              /* so a joiner can see how full the table already looks */
} sync_beacon_payload_t;
#pragma pack(pop)

/* SYNC_MSG_JOIN_REQUEST payload. */
#pragma pack(push, 1)
typedef struct {
    char device_name[SYNC_MAX_NAME_LEN];
} sync_join_request_payload_t;
#pragma pack(pop)

/* Packed, wire-safe mirror of the parts of game_state_t that matter to
 * every device at the table. Deliberately NOT the same struct as
 * app_state.h's game_state_t — that one's layout isn't guaranteed to
 * match byte-for-byte between Xtensa and RISC-V, and it also carries
 * fields (current_screen, etc.) that are local to each device, not
 * shared table state. */
#define SYNC_MAX_PLAYERS 4
#pragma pack(push, 1)
typedef struct {
    uint8_t player_count;
    int16_t starting_life;
    int16_t life[SYNC_MAX_PLAYERS];
    int16_t poison[SYNC_MAX_PLAYERS];
    int16_t energy[SYNC_MAX_PLAYERS];
    int16_t commander_damage[SYNC_MAX_PLAYERS][SYNC_MAX_PLAYERS];
    uint8_t selected_player;
    uint8_t track_flags; /* bit0=poison bit1=energy bit2=storm bit3=commander_damage */
    int16_t storm_count;
    uint8_t player_led_color_idx[SYNC_MAX_PLAYERS];
} sync_state_payload_t;
#pragma pack(pop)

#define SYNC_TRACK_FLAG_POISON (1 << 0)
#define SYNC_TRACK_FLAG_ENERGY (1 << 1)
#define SYNC_TRACK_FLAG_STORM (1 << 2)
#define SYNC_TRACK_FLAG_COMMANDER_DAMAGE (1 << 3)

/* SYNC_MSG_JOIN_ACK payload: same as a state broadcast, nothing extra to
 * add — joining just means "here's the table as it stands right now". */
typedef sync_state_payload_t sync_join_ack_payload_t;

typedef enum : uint8_t {
    SYNC_ACTION_ADJUST_LIFE = 1,
    SYNC_ACTION_SELECT_NEXT_PLAYER = 2,
    SYNC_ACTION_ADJUST_POISON = 3,
    SYNC_ACTION_ADJUST_ENERGY = 4,
    SYNC_ACTION_ADJUST_COMMANDER_DAMAGE = 5,
    SYNC_ACTION_ADJUST_STORM = 6,
    SYNC_ACTION_RESET_STORM = 7,
} sync_action_t;

/* SYNC_MSG_ACTION payload — a joiner asking the host to apply one change.
 * Not every field is used by every action type; unused fields are just
 * ignored by the handler for that action. */
#pragma pack(push, 1)
typedef struct {
    uint8_t action;          /* sync_action_t */
    uint8_t player_idx;
    uint8_t from_player_idx; /* only for ADJUST_COMMANDER_DAMAGE */
    int16_t delta;
} sync_action_payload_t;
#pragma pack(pop)

/* Largest payload any message carries — sync_state_payload_t, at roughly
 * 1+2+(2*4*3)+(2*16)+1+1+2+4 = ~67 bytes. Header (10 bytes) + this is
 * comfortably under ESP-NOW's 250-byte-per-packet limit. */
#define SYNC_MAX_PAYLOAD_SIZE 128
