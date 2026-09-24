#include "counter_screen.h"
#include "dice_overlay.h"
#include "menu_screen.h"
#include "player_detail_overlay.h"
#include "../ui/theme.h"
#include "../app_state.h"
#include "../input_driver.h"
#include "../led_driver.h"
#include "board_config.h"
#include <stdio.h>

typedef struct { int16_t dx, dy; } player_pos_t;

/* Offsets from screen center (120,120), tuned so labels + their +/- row
 * stay inside the round visible area (corners of the square panel are
 * hidden under the bezel). Indexed [player_count-1][player_idx]. */
static const player_pos_t POSITIONS[MAX_PLAYERS][MAX_PLAYERS] = {
    /* 1 player */  { {0, 0} },
    /* 2 players */ { {0, -55}, {0, 55} },
    /* 3 players */ { {0, -70}, {61, 35}, {-61, 35} },
    /* 4 players */ { {44, -44}, {44, 44}, {-44, 44}, {-44, -44} },
};

static const lv_font_t *LIFE_FONTS[MAX_PLAYERS] = {
    &lv_font_montserrat_48,
    &lv_font_montserrat_40,
    &lv_font_montserrat_28,
    &lv_font_montserrat_28,
};

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_containers[MAX_PLAYERS];
static lv_obj_t *s_life_labels[MAX_PLAYERS];
static lv_obj_t *s_status_labels[MAX_PLAYERS]; /* small poison/energy badge, blank when nothing to show */
/* player_count the currently-built widget tree was laid out for — used by
 * counter_screen_rebuild_if_active() (table sync) to tell "just refresh
 * the numbers" apart from "the player count itself changed under us,
 * rebuild the layout". */
static uint8_t s_built_for_player_count = 0;

static void update_container_style(uint8_t i) {
    if (!s_containers[i]) return;
    bool eliminated = (g_game.players[i].life <= 0);
    lv_obj_set_style_text_color(s_life_labels[i],
        eliminated ? THEME_DANGER : THEME_LIFE_TEXT, 0);

#if BOARD_HAS_ENCODER
    bool selected = (i == g_game.selected_player);
    lv_obj_set_style_border_width(s_containers[i], selected ? 2 : 0, 0);
    lv_obj_set_style_border_color(s_containers[i], THEME_SELECTED_RING, 0);
#endif
}

static void update_status_badge(uint8_t i) {
    if (!s_status_labels[i]) return;
    char buf[24];
    buf[0] = '\0';
    size_t len = 0;
    if (g_game.track_poison && g_game.players[i].poison > 0) {
        len += snprintf(buf + len, sizeof(buf) - len, "P%d ", g_game.players[i].poison);
    }
    if (g_game.track_energy && g_game.players[i].energy > 0) {
        len += snprintf(buf + len, sizeof(buf) - len, "E%d", g_game.players[i].energy);
    }
    lv_label_set_text(s_status_labels[i], buf);
}

void counter_screen_refresh() {
    /* Guards against a dangling call: if this screen isn't the one on
     * display right now (e.g. a queued event fires after we've already
     * navigated away), s_life_labels[]/s_containers[] may still be
     * holding pointers into a just-deleted widget tree. Touching those
     * is exactly what produces LVGL's "No screen found" warning followed
     * by a hard crash — bail out instead. */
    if (!s_screen) return;
    for (uint8_t i = 0; i < g_game.player_count; i++) {
        if (!s_life_labels[i]) continue;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", g_game.players[i].life);
        lv_label_set_text(s_life_labels[i], buf);
        update_container_style(i);
        update_status_badge(i);
    }
}

static void plus_btn_cb(lv_event_t *e) {
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    app_state_adjust_life(idx, +1);
    counter_screen_refresh();
}

static void minus_btn_cb(lv_event_t *e) {
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    app_state_adjust_life(idx, -1);
    counter_screen_refresh();
}

static void player_long_press_cb(lv_event_t *e) {
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    /* Opens the poison/energy/commander-damage detail view for this
     * player. Touch-only gesture (both boards have touch), so it works
     * the same on the rotary board too. */
    player_detail_overlay_open(idx);
}

/* Swiping left off the counter screen (empty background, not one of the
 * player boxes) opens the settings/utility menu — the d20 roller, extra
 * counter toggles, theme/brightness, etc. now live there instead of a
 * button in the middle of this screen. */
static void screen_gesture_cb(lv_event_t *e) {
    (void)e;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    if (lv_indev_get_gesture_dir(indev) == LV_DIR_LEFT) {
        menu_screen_create();
        counter_screen_destroy();
    }
}

static void build_player_widget(uint8_t idx) {
    player_pos_t pos = POSITIONS[g_game.player_count - 1][idx];

    lv_obj_t *cont = lv_obj_create(s_screen);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, 96, 72);
    lv_obj_align(cont, LV_ALIGN_CENTER, pos.dx, pos.dy);
    lv_obj_set_style_radius(cont, 8, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    /* A swipe that starts on a player box (not just empty background)
     * should still reach the screen's own gesture handler and open the
     * menu, instead of being swallowed here. */
    lv_obj_add_flag(cont, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(cont, player_long_press_cb, LV_EVENT_LONG_PRESSED, (void *)(uintptr_t)idx);
    s_containers[idx] = cont;

    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, "--");
    lv_obj_set_style_text_font(label, LIFE_FONTS[g_game.player_count - 1], 0);
    lv_obj_set_style_text_color(label, THEME_LIFE_TEXT, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
    s_life_labels[idx] = label;

    lv_obj_t *status = lv_label_create(cont);
    lv_label_set_text(status, "");
    lv_obj_set_style_text_color(status, theme_accent(), 0);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_12, 0);
    lv_obj_align(status, LV_ALIGN_TOP_RIGHT, 2, -4);
    s_status_labels[idx] = status;

    lv_obj_t *minus_btn = lv_btn_create(cont);
    lv_obj_set_size(minus_btn, 34, 26);
    lv_obj_align(minus_btn, LV_ALIGN_BOTTOM_LEFT, 4, 0);
    lv_obj_set_style_bg_color(minus_btn, THEME_BTN_MINUS, 0);
    lv_obj_add_event_cb(minus_btn, minus_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);
    lv_obj_t *minus_lbl = lv_label_create(minus_btn);
    lv_label_set_text(minus_lbl, "-");
    lv_obj_set_style_text_font(minus_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(minus_lbl);

    lv_obj_t *plus_btn = lv_btn_create(cont);
    lv_obj_set_size(plus_btn, 34, 26);
    lv_obj_align(plus_btn, LV_ALIGN_BOTTOM_RIGHT, -4, 0);
    lv_obj_set_style_bg_color(plus_btn, THEME_BTN_PLUS, 0);
    lv_obj_add_event_cb(plus_btn, plus_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);
    lv_obj_t *plus_lbl = lv_label_create(plus_btn);
    lv_label_set_text(plus_lbl, "+");
    lv_obj_set_style_text_font(plus_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(plus_lbl);
}

#if BOARD_HAS_ENCODER
static void encoder_handler(input_encoder_event_t evt, int16_t data) {
    if (dice_overlay_is_open()) {
        /* While the dice overlay is up, the knob controls it instead. */
        if (evt == INPUT_ENC_CLICK) dice_overlay_handle_roll();
        if (evt == INPUT_ENC_LONG_PRESS) dice_overlay_close();
        return;
    }
    if (g_game.current_screen != SCREEN_COUNTER) return;
    if (!s_screen) return; /* belt-and-suspenders: see counter_screen_refresh() */

    switch (evt) {
        case INPUT_ENC_TURN:
            app_state_adjust_life(g_game.selected_player, data);
            counter_screen_refresh();
            break;
        case INPUT_ENC_CLICK:
            app_state_select_next_player();
            counter_screen_refresh();
            /* If "Turn Colors" is on (menu screen), the ambient LEDs
             * switch to the new player's assigned color. A harmless
             * no-op call otherwise (led_driver_refresh() checks both
             * led_driver_available() and whether turn colors are on). */
            led_driver_refresh();
            break;
        case INPUT_ENC_LONG_PRESS:
            dice_overlay_open();
            break;
    }
}
#endif

void counter_screen_create() {
    g_game.current_screen = SCREEN_COUNTER;
    s_built_for_player_count = g_game.player_count;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, THEME_BG, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        s_containers[i] = nullptr;
        s_life_labels[i] = nullptr;
        s_status_labels[i] = nullptr;
    }

    for (uint8_t i = 0; i < g_game.player_count; i++) {
        build_player_widget(i);
    }

    /* Swipe left anywhere on the background to reach the menu screen (d20
     * roller, settings, extra-counter toggles now live there). */
    lv_obj_add_event_cb(s_screen, screen_gesture_cb, LV_EVENT_GESTURE, nullptr);

#if BOARD_HAS_ENCODER
    input_driver_set_encoder_handler(encoder_handler);
#endif

    counter_screen_refresh();
    /* Make sure the ambient LEDs match whoever's turn it is (or the
     * chosen ambient color) as soon as this screen becomes active —
     * covers New Game resetting selected_player back to 0, and coming
     * back from the menu after changing LED settings there. */
    led_driver_refresh();
    lv_scr_load(s_screen);
}

void counter_screen_destroy() {
    /* The dice roller and the player-detail view are both created as
     * children of this screen (lv_obj_create(lv_scr_act())). If either is
     * still open when this screen gets torn down (e.g. the user swiped to
     * the menu without closing it first), deleting s_screen below deletes
     * their widgets right along with it — but their own modules don't
     * know that unless we tell them. Without this, dice_overlay_is_open()
     * / player_detail_overlay_is_open() would keep reporting "open"
     * forever, pointing at freed memory, and the next encoder click or
     * touch routed into them reads through a dangling pointer. This is
     * exactly what was crashing before this fix. */
    dice_overlay_discard();
    player_detail_overlay_discard();

    if (s_screen) {
        /* Async delete — see the matching comment in setup_screen.cpp. Call
         * this only after a replacement screen has already been loaded. */
        lv_obj_del_async(s_screen);
        s_screen = nullptr;
    }
    /* Clear every cached child-widget pointer along with it. These were
     * only ever valid for the lifetime of the s_screen tree they belong
     * to; leaving them set after that tree is gone means any late-firing
     * event (a queued encoder click, an in-flight touch release, an
     * async callback that hasn't run yet) reads dangling pointers into
     * freed heap memory instead of getting a clean "nothing to do here". */
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        s_containers[i] = nullptr;
        s_life_labels[i] = nullptr;
        s_status_labels[i] = nullptr;
    }
}

void counter_screen_rebuild_if_active() {
    if (!s_screen) return;

    if (s_built_for_player_count == g_game.player_count) {
        counter_screen_refresh();
        return;
    }

    /* Player count changed out from under the built layout (a remote New
     * Game with a different count, applied via table sync) — the existing
     * boxes are positioned/sized for the old count, so rebuild instead of
     * just re-labeling. Same create-before-destroy ordering as every other
     * screen transition here: build + load the replacement first, then
     * async-delete the old tree. */
    lv_obj_t *old_screen = s_screen;
    s_screen = nullptr; /* so counter_screen_create() below starts clean */
    dice_overlay_discard();
    player_detail_overlay_discard();

    counter_screen_create();

    lv_obj_del_async(old_screen);
}
