#include "menu_screen.h"
#include "counter_screen.h"
#include "setup_screen.h"
#include "dice_overlay.h"
#include "../ui/theme.h"
#include "../app_state.h"
#include "../input_driver.h"
#include "../led_driver.h"
#include "../led_colors.h"
#include "../display_driver.h"
#include "../net/table_sync.h"
#include "board_config.h"
#include <stdio.h>

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_random_result_lbl = nullptr;
static lv_obj_t *s_theme_btn_lbl = nullptr;
static lv_obj_t *s_storm_lbl = nullptr;
static lv_obj_t *s_led_color_btn_lbl = nullptr;

static void go_back() {
    /* Load counter screen first (rebuilds it fresh from current game
     * state), then tear down the menu screen — never delete the screen
     * that's still active. Same pattern as every other screen swap here. */
    counter_screen_create();
    menu_screen_destroy();
}

/* Rebuilds the menu screen in place, for the Multiplayer buttons — those
 * change table_sync's internal state (start scanning, join, leave, ...)
 * and need the screen redrawn with fresh status/button/discovered-table
 * content afterward. Same create-before-destroy ordering as every other
 * screen swap here, just with "old" and "new" both being a menu screen. */
static void rebuild_self() {
    lv_obj_t *old_screen = s_screen;
    s_screen = nullptr; /* menu_screen_create() below must start clean */
    dice_overlay_discard();
    menu_screen_create();
    if (old_screen) lv_obj_del_async(old_screen);
}

static void back_btn_cb(lv_event_t *e) { (void)e; go_back(); }

/* Mirror of counter_screen's swipe-left: swiping right on the menu screen
 * returns to the counter screen, same as the Back button. */
static void screen_gesture_cb(lv_event_t *e) {
    (void)e;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    if (lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT) {
        go_back();
    }
}

static void new_game_btn_cb(lv_event_t *e) {
    (void)e;
    setup_screen_create();
    menu_screen_destroy();
}

static void dice_btn_cb(lv_event_t *e) { (void)e; dice_overlay_open(); }

static void random_first_btn_cb(lv_event_t *e) {
    (void)e;
    uint8_t p = app_state_pick_random_player();
    if (!s_random_result_lbl) return;
    char buf[24];
    snprintf(buf, sizeof(buf), "Player %d goes first!", p + 1);
    lv_label_set_text(s_random_result_lbl, buf);
}

static void brightness_slider_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t v = lv_slider_get_value(slider);
    gfx.setBrightness((uint8_t)v);
}

static void theme_btn_cb(lv_event_t *e) {
    (void)e;
    const char *name = theme_cycle();
    if (!s_theme_btn_lbl) return;
    lv_label_set_text(s_theme_btn_lbl, name);
    /* Re-apply so already-built screens elsewhere pick it up the next
     * time they're rebuilt (setup/counter screens are rebuilt on nearly
     * every navigation anyway, so this doesn't need a broadcast). */
}

static void led_switch_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    led_driver_set_enabled(lv_obj_has_state(sw, LV_STATE_CHECKED));
    led_driver_refresh();
}

static void led_brightness_slider_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    led_driver_set_brightness((uint8_t)lv_slider_get_value(slider));
    led_driver_refresh();
}

/* Cycles the strip's base/ambient color — what it shows when turn colors
 * are off. ("an option in the menu to change the color of the leds") */
static void led_color_btn_cb(lv_event_t *e) {
    (void)e;
    uint8_t next = (led_driver_get_ambient_color_idx() + 1) % LED_COLOR_COUNT;
    led_driver_set_ambient_color_idx(next);
    led_driver_refresh();
    if (s_led_color_btn_lbl) lv_label_set_text(s_led_color_btn_lbl, LED_COLORS[next].name);
}

static void led_turn_colors_switch_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    led_driver_set_turn_colors_enabled(lv_obj_has_state(sw, LV_STATE_CHECKED));
    led_driver_refresh();
}

/* One of these per active player — cycles that player's LED turn color.
 * The player index is packed into the button's user data. Reads the
 * label back off the button itself (its only child) instead of keeping
 * a separate array of label pointers to track and null out. */
static void player_color_btn_cb(lv_event_t *e) {
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    uint8_t next = (g_game.player_led_color_idx[idx] + 1) % LED_COLOR_COUNT;
    g_game.player_led_color_idx[idx] = next;

    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl) {
        char buf[24];
        snprintf(buf, sizeof(buf), "P%d: %s", idx + 1, LED_COLORS[next].name);
        lv_label_set_text(lbl, buf);
    }
    /* Only visibly changes the strip if it's currently this player's
     * turn and turn colors are on — harmless no-op call otherwise. */
    led_driver_refresh();
}

static void poison_switch_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    g_game.track_poison = lv_obj_has_state(sw, LV_STATE_CHECKED);
}
static void energy_switch_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    g_game.track_energy = lv_obj_has_state(sw, LV_STATE_CHECKED);
}
static void storm_switch_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    g_game.track_storm = lv_obj_has_state(sw, LV_STATE_CHECKED);
}
static void commander_switch_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    g_game.track_commander_damage = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void refresh_storm_label() {
    if (!s_storm_lbl) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "Storm: %d", g_game.storm_count);
    lv_label_set_text(s_storm_lbl, buf);
}
static void storm_minus_cb(lv_event_t *e) { (void)e; app_state_adjust_storm(-1); refresh_storm_label(); }
static void storm_plus_cb(lv_event_t *e)  { (void)e; app_state_adjust_storm(+1); refresh_storm_label(); }
static void storm_reset_cb(lv_event_t *e) { (void)e; app_state_reset_storm(); refresh_storm_label(); }

/* ---- Multiplayer (table sync) ----
 * No on-device text entry in v1 (LVGL keyboard input isn't wired up), so
 * table/device names are fixed strings rather than something you type —
 * good enough to tell devices apart at a glance in the discovered list,
 * see the README for this and table sync's other v1 limitations. Every
 * one of these buttons just pokes table_sync's own state machine and then
 * rebuild_self()s to show the result — table_sync.cpp never reaches into
 * this screen directly except for the one auto-navigation on a successful
 * join (see handle_join_ack() there). */
static void host_btn_cb(lv_event_t *e) {
    (void)e;
    table_sync_host_start("MTG Table");
    rebuild_self();
}
static void stop_hosting_btn_cb(lv_event_t *e) {
    (void)e;
    table_sync_host_stop();
    rebuild_self();
}
static void scan_btn_cb(lv_event_t *e) {
    (void)e;
    table_sync_start_scan();
    rebuild_self();
}
static void cancel_scan_btn_cb(lv_event_t *e) {
    (void)e;
    table_sync_stop_scan();
    rebuild_self();
}
static void refresh_scan_btn_cb(lv_event_t *e) {
    (void)e;
    /* Discovered tables accumulate in the background (table_sync_poll()
     * runs from loop() regardless of which screen is showing) — this just
     * redraws the list with whatever's arrived since it was last drawn. */
    rebuild_self();
}
static void join_btn_cb(lv_event_t *e) {
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    table_sync_join(idx, "Player");
    rebuild_self();
    /* If the JOIN_ACK is already sitting in the queue by the time
     * table_sync_poll() next runs, handle_join_ack() will navigate away to
     * the counter screen on its own — this rebuild just shows "Joining..."
     * in the meantime for the (much more common) case where it hasn't. */
}
static void leave_btn_cb(lv_event_t *e) {
    (void)e;
    table_sync_leave();
    rebuild_self();
}

/* ---- small layout helpers ---- */

static lv_obj_t *section_label(lv_obj_t *parent, const char *text) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, theme_accent(), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    return l;
}

static lv_obj_t *action_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 170, 34);
    lv_obj_set_style_bg_color(btn, THEME_PANEL, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);
    return btn;
}

static lv_obj_t *labeled_switch(lv_obj_t *parent, const char *text, bool initial, lv_event_cb_t cb) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 170, 28);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, THEME_LIFE_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    if (initial) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return row;
}

#if BOARD_HAS_ENCODER
static void encoder_handler(input_encoder_event_t evt, int16_t data) {
    if (g_game.current_screen != SCREEN_MENU) return;
    if (!s_screen) return; /* belt-and-suspenders: see menu_screen_destroy() */
    switch (evt) {
        case INPUT_ENC_TURN:
            /* Encoder scrolls the settings list; tap to actually interact
             * with whatever's in view (this board has touch too). */
            lv_obj_scroll_by(s_screen, 0, -data * 40, LV_ANIM_ON);
            break;
        case INPUT_ENC_CLICK:
            break;
        case INPUT_ENC_LONG_PRESS:
            go_back();
            break;
    }
}
#endif

void menu_screen_create() {
    g_game.current_screen = SCREEN_MENU;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, THEME_BG, 0);
    lv_obj_set_flex_flow(s_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(s_screen, 30, 0);
    lv_obj_set_style_pad_bottom(s_screen, 30, 0);
    lv_obj_set_style_pad_row(s_screen, 10, 0);
    lv_obj_set_scroll_dir(s_screen, LV_DIR_VER);
    lv_obj_add_event_cb(s_screen, screen_gesture_cb, LV_EVENT_GESTURE, nullptr);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Menu");
    lv_obj_set_style_text_color(title, theme_accent(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    /* Only the host is allowed to start a new game while synced — a
     * joiner's g_game is a mirror of the host's, and app_state_reset_game()
     * has no table-sync guard of its own (unlike the other mutators),
     * since only this button ever calls it. Hidden rather than disabled so
     * there's nothing to explain in-UI about why it doesn't work. */
    if (table_sync_role() != SYNC_ROLE_JOINER) {
        action_button(s_screen, "New Game", new_game_btn_cb);
    }
    action_button(s_screen, "Roll d20", dice_btn_cb);

    action_button(s_screen, "Random First Player", random_first_btn_cb);
    s_random_result_lbl = lv_label_create(s_screen);
    lv_label_set_text(s_random_result_lbl, "");
    lv_obj_set_style_text_color(s_random_result_lbl, THEME_LIFE_TEXT, 0);
    lv_obj_set_style_text_font(s_random_result_lbl, &lv_font_montserrat_14, 0);

    section_label(s_screen, "BRIGHTNESS");
    lv_obj_t *brightness_slider = lv_slider_create(s_screen);
    lv_obj_set_size(brightness_slider, 170, 14);
    lv_slider_set_range(brightness_slider, 10, 255);
    lv_slider_set_value(brightness_slider, 255, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    section_label(s_screen, "THEME");
    lv_obj_t *theme_btn = lv_btn_create(s_screen);
    lv_obj_set_size(theme_btn, 170, 34);
    lv_obj_set_style_bg_color(theme_btn, THEME_PANEL, 0);
    lv_obj_add_event_cb(theme_btn, theme_btn_cb, LV_EVENT_CLICKED, nullptr);
    s_theme_btn_lbl = lv_label_create(theme_btn);
    lv_label_set_text(s_theme_btn_lbl, theme_name());
    lv_obj_set_style_text_font(s_theme_btn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(s_theme_btn_lbl);

    if (led_driver_available()) {
        section_label(s_screen, "AMBIENT LED");
        labeled_switch(s_screen, "LED On", led_driver_is_enabled(), led_switch_cb);
        lv_obj_t *led_slider = lv_slider_create(s_screen);
        lv_obj_set_size(led_slider, 170, 14);
        lv_slider_set_range(led_slider, 0, 255);
        lv_slider_set_value(led_slider, led_driver_get_brightness(), LV_ANIM_OFF);
        lv_obj_add_event_cb(led_slider, led_brightness_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

        lv_obj_t *led_color_btn = lv_btn_create(s_screen);
        lv_obj_set_size(led_color_btn, 170, 34);
        lv_obj_set_style_bg_color(led_color_btn, THEME_PANEL, 0);
        lv_obj_add_event_cb(led_color_btn, led_color_btn_cb, LV_EVENT_CLICKED, nullptr);
        s_led_color_btn_lbl = lv_label_create(led_color_btn);
        lv_label_set_text(s_led_color_btn_lbl, LED_COLORS[led_driver_get_ambient_color_idx()].name);
        lv_obj_set_style_text_font(s_led_color_btn_lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(s_led_color_btn_lbl);

        labeled_switch(s_screen, "Turn Colors", led_driver_is_turn_colors_enabled(), led_turn_colors_switch_cb);

        /* One color-cycle button per active player — only meaningful
         * with "Turn Colors" on, but left visible either way so the
         * assignment can be set up in advance. */
        section_label(s_screen, "PLAYER COLORS");
        for (uint8_t i = 0; i < g_game.player_count; i++) {
            lv_obj_t *pc_btn = lv_btn_create(s_screen);
            lv_obj_set_size(pc_btn, 170, 30);
            lv_obj_set_style_bg_color(pc_btn, THEME_PANEL, 0);
            lv_obj_add_event_cb(pc_btn, player_color_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
            lv_obj_t *pc_lbl = lv_label_create(pc_btn);
            char buf[24];
            snprintf(buf, sizeof(buf), "P%d: %s", i + 1, LED_COLORS[g_game.player_led_color_idx[i]].name);
            lv_label_set_text(pc_lbl, buf);
            lv_obj_set_style_text_font(pc_lbl, &lv_font_montserrat_14, 0);
            lv_obj_center(pc_lbl);
        }
    }

    section_label(s_screen, "EXTRA COUNTERS");
    labeled_switch(s_screen, "Poison", g_game.track_poison, poison_switch_cb);
    labeled_switch(s_screen, "Energy", g_game.track_energy, energy_switch_cb);
    labeled_switch(s_screen, "Storm", g_game.track_storm, storm_switch_cb);
    labeled_switch(s_screen, "Commander Dmg", g_game.track_commander_damage, commander_switch_cb);

    /* Storm is a single shared count (not per player), so it's managed
     * right here rather than on the counter screen. */
    lv_obj_t *storm_row = lv_obj_create(s_screen);
    lv_obj_remove_style_all(storm_row);
    lv_obj_set_size(storm_row, 170, 34);
    lv_obj_clear_flag(storm_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *storm_minus = lv_btn_create(storm_row);
    lv_obj_set_size(storm_minus, 34, 28);
    lv_obj_align(storm_minus, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(storm_minus, THEME_BTN_MINUS, 0);
    lv_obj_add_event_cb(storm_minus, storm_minus_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *storm_minus_lbl = lv_label_create(storm_minus);
    lv_label_set_text(storm_minus_lbl, "-");
    lv_obj_set_style_text_font(storm_minus_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(storm_minus_lbl);

    s_storm_lbl = lv_label_create(storm_row);
    lv_obj_set_style_text_color(s_storm_lbl, THEME_LIFE_TEXT, 0);
    lv_obj_set_style_text_font(s_storm_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(s_storm_lbl);
    refresh_storm_label();

    lv_obj_t *storm_plus = lv_btn_create(storm_row);
    lv_obj_set_size(storm_plus, 34, 28);
    lv_obj_align(storm_plus, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(storm_plus, THEME_BTN_PLUS, 0);
    lv_obj_add_event_cb(storm_plus, storm_plus_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *storm_plus_lbl = lv_label_create(storm_plus);
    lv_label_set_text(storm_plus_lbl, "+");
    lv_obj_set_style_text_font(storm_plus_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(storm_plus_lbl);

    lv_obj_t *storm_reset_btn = lv_btn_create(s_screen);
    lv_obj_set_size(storm_reset_btn, 100, 26);
    lv_obj_set_style_bg_color(storm_reset_btn, THEME_PANEL, 0);
    lv_obj_add_event_cb(storm_reset_btn, storm_reset_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *storm_reset_lbl = lv_label_create(storm_reset_btn);
    lv_label_set_text(storm_reset_lbl, "Reset Storm");
    lv_obj_set_style_text_font(storm_reset_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(storm_reset_lbl);

    section_label(s_screen, "MULTIPLAYER (SYNC)");
    {
        sync_status_t status = table_sync_status();

        lv_obj_t *status_lbl = lv_label_create(s_screen);
        char status_buf[32];
        switch (status) {
            case SYNC_STATUS_HOSTING:
                snprintf(status_buf, sizeof(status_buf), "Hosting (%d joined)", table_sync_host_joiner_count());
                break;
            case SYNC_STATUS_SCANNING:
                snprintf(status_buf, sizeof(status_buf), "Scanning...");
                break;
            case SYNC_STATUS_JOINING:
                snprintf(status_buf, sizeof(status_buf), "Joining...");
                break;
            case SYNC_STATUS_JOINED:
                snprintf(status_buf, sizeof(status_buf), "Joined table");
                break;
            case SYNC_STATUS_JOIN_LOST:
                snprintf(status_buf, sizeof(status_buf), "Lost connection to host");
                break;
            case SYNC_STATUS_IDLE:
            default:
                snprintf(status_buf, sizeof(status_buf), "Not connected");
                break;
        }
        lv_label_set_text(status_lbl, status_buf);
        lv_obj_set_style_text_color(status_lbl, THEME_LIFE_TEXT, 0);
        lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_14, 0);

        switch (status) {
            case SYNC_STATUS_IDLE:
                action_button(s_screen, "Host Table", host_btn_cb);
                action_button(s_screen, "Join Table (Scan)", scan_btn_cb);
                break;
            case SYNC_STATUS_HOSTING:
                action_button(s_screen, "Stop Hosting", stop_hosting_btn_cb);
                break;
            case SYNC_STATUS_SCANNING: {
                action_button(s_screen, "Refresh List", refresh_scan_btn_cb);
                uint8_t n = table_sync_discovered_count();
                for (uint8_t i = 0; i < n; i++) {
                    const sync_discovered_table_t *d = table_sync_discovered_get(i);
                    if (!d) continue;
                    lv_obj_t *btn = lv_btn_create(s_screen);
                    lv_obj_set_size(btn, 170, 30);
                    lv_obj_set_style_bg_color(btn, THEME_PANEL, 0);
                    lv_obj_add_event_cb(btn, join_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
                    lv_obj_t *lbl = lv_label_create(btn);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%s (%d)", d->host_name, d->player_count);
                    lv_label_set_text(lbl, buf);
                    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
                    lv_obj_center(lbl);
                }
                action_button(s_screen, "Cancel Scan", cancel_scan_btn_cb);
                break;
            }
            case SYNC_STATUS_JOINING:
                action_button(s_screen, "Cancel", leave_btn_cb);
                break;
            case SYNC_STATUS_JOINED:
            case SYNC_STATUS_JOIN_LOST:
                action_button(s_screen, "Leave Table", leave_btn_cb);
                break;
        }
    }

    action_button(s_screen, "Back", back_btn_cb);

#if BOARD_HAS_ENCODER
    input_driver_set_encoder_handler(encoder_handler);
#endif

    lv_scr_load(s_screen);
}

void menu_screen_destroy() {
    /* Same reasoning as counter_screen_destroy(): the dice overlay can be
     * opened from here too ("Roll d20"), and it's parented to this screen
     * — if it's still open when this screen tears down, tell it before
     * deleting s_screen so it doesn't keep reporting itself as open and
     * pointing at freed memory afterward. */
    dice_overlay_discard();

    if (s_screen) {
        lv_obj_del_async(s_screen);
        s_screen = nullptr;
    }
    /* Same reasoning as counter_screen_destroy(): don't leave pointers to
     * a widget tree that's about to be freed lying around in statics. */
    s_random_result_lbl = nullptr;
    s_theme_btn_lbl = nullptr;
    s_storm_lbl = nullptr;
    s_led_color_btn_lbl = nullptr;
}
