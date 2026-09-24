#include "setup_screen.h"
#include "counter_screen.h"
#include "../ui/theme.h"
#include "../app_state.h"
#include "../input_driver.h"
#include "board_config.h"
#include <stdio.h>

static lv_obj_t *s_screen = nullptr;
static lv_obj_t *s_count_label = nullptr;
static lv_obj_t *s_life_label = nullptr;
static lv_obj_t *s_count_field_box = nullptr;
static lv_obj_t *s_life_field_box = nullptr;

static uint8_t s_pending_count = 2;
static int16_t s_pending_life = DEFAULT_START_LIFE;

#if BOARD_HAS_ENCODER
static uint8_t s_focused_field = 0; /* 0 = player count, 1 = starting life */
#endif

static void refresh_labels() {
    if (!s_screen) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", s_pending_count);
    lv_label_set_text(s_count_label, buf);
    snprintf(buf, sizeof(buf), "%d", s_pending_life);
    lv_label_set_text(s_life_label, buf);

#if BOARD_HAS_ENCODER
    lv_obj_set_style_border_width(s_count_field_box, s_focused_field == 0 ? 2 : 0, 0);
    lv_obj_set_style_border_color(s_count_field_box, THEME_SELECTED_RING, 0);
    lv_obj_set_style_border_width(s_life_field_box, s_focused_field == 1 ? 2 : 0, 0);
    lv_obj_set_style_border_color(s_life_field_box, THEME_SELECTED_RING, 0);
#endif
}

static void start_game() {
    app_state_reset_game(s_pending_count, s_pending_life);
    /* Load the counter screen BEFORE tearing down the setup screen — the
     * setup screen is still the active screen at this point, and deleting
     * the active screen out from under LVGL leaves a dangling internal
     * pointer that crashes on the next redraw/input event. */
    counter_screen_create();
    setup_screen_destroy();
}

static void count_minus_cb(lv_event_t *e) {
    (void)e;
    if (s_pending_count > 1) s_pending_count--;
    refresh_labels();
}
static void count_plus_cb(lv_event_t *e) {
    (void)e;
    if (s_pending_count < MAX_PLAYERS) s_pending_count++;
    refresh_labels();
}
static void life_minus_cb(lv_event_t *e) {
    (void)e;
    if (s_pending_life > 5) s_pending_life -= 5;
    refresh_labels();
}
static void life_plus_cb(lv_event_t *e) {
    (void)e;
    if (s_pending_life < 999) s_pending_life += 5;
    refresh_labels();
}
static void start_btn_cb(lv_event_t *e) {
    (void)e;
    start_game();
}

static lv_obj_t *build_stepper_row(lv_obj_t *parent, const char *title, int16_t y_off,
                                    lv_obj_t **out_value_label, lv_obj_t **out_box,
                                    lv_event_cb_t minus_cb, lv_event_cb_t plus_cb) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 190, 46);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, y_off);
    lv_obj_set_style_radius(box, 8, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    *out_box = box;

    lv_obj_t *title_lbl = lv_label_create(box);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, theme_accent(), 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, -4);

    lv_obj_t *minus_btn = lv_btn_create(box);
    lv_obj_set_size(minus_btn, 34, 28);
    lv_obj_align(minus_btn, LV_ALIGN_LEFT_MID, 0, 6);
    lv_obj_set_style_bg_color(minus_btn, THEME_BTN_MINUS, 0);
    lv_obj_add_event_cb(minus_btn, minus_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *minus_lbl = lv_label_create(minus_btn);
    lv_label_set_text(minus_lbl, "-");
    lv_obj_set_style_text_font(minus_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(minus_lbl);

    lv_obj_t *value_lbl = lv_label_create(box);
    lv_label_set_text(value_lbl, "--");
    lv_obj_set_style_text_font(value_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(value_lbl, THEME_LIFE_TEXT, 0);
    lv_obj_align(value_lbl, LV_ALIGN_CENTER, 0, 6);
    *out_value_label = value_lbl;

    lv_obj_t *plus_btn = lv_btn_create(box);
    lv_obj_set_size(plus_btn, 34, 28);
    lv_obj_align(plus_btn, LV_ALIGN_RIGHT_MID, 0, 6);
    lv_obj_set_style_bg_color(plus_btn, THEME_BTN_PLUS, 0);
    lv_obj_add_event_cb(plus_btn, plus_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *plus_lbl = lv_label_create(plus_btn);
    lv_label_set_text(plus_lbl, "+");
    lv_obj_set_style_text_font(plus_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(plus_lbl);

    return box;
}

#if BOARD_HAS_ENCODER
static void encoder_handler(input_encoder_event_t evt, int16_t data) {
    if (g_game.current_screen != SCREEN_SETUP) return;
    if (!s_screen) return; /* belt-and-suspenders: see setup_screen_destroy() */
    switch (evt) {
        case INPUT_ENC_TURN:
            if (s_focused_field == 0) {
                int16_t v = s_pending_count + data;
                if (v < 1) v = 1;
                if (v > MAX_PLAYERS) v = MAX_PLAYERS;
                s_pending_count = (uint8_t)v;
            } else {
                int16_t v = s_pending_life + data * 5;
                if (v < 5) v = 5;
                if (v > 999) v = 999;
                s_pending_life = v;
            }
            refresh_labels();
            break;
        case INPUT_ENC_CLICK:
            s_focused_field = s_focused_field ? 0 : 1;
            refresh_labels();
            break;
        case INPUT_ENC_LONG_PRESS:
            start_game();
            break;
    }
}
#endif

void setup_screen_create() {
    g_game.current_screen = SCREEN_SETUP;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, THEME_BG, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "MTG Life Counter");
    lv_obj_set_style_text_color(title, theme_accent(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    build_stepper_row(s_screen, "PLAYERS", -20, &s_count_label, &s_count_field_box,
                       count_minus_cb, count_plus_cb);
    build_stepper_row(s_screen, "STARTING LIFE", 40, &s_life_label, &s_life_field_box,
                       life_minus_cb, life_plus_cb);

    lv_obj_t *start_btn = lv_btn_create(s_screen);
    lv_obj_set_size(start_btn, 120, 34);
    lv_obj_align(start_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(start_btn, theme_accent(), 0);
    lv_obj_add_event_cb(start_btn, start_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *start_lbl = lv_label_create(start_btn);
    lv_label_set_text(start_lbl, "Start Game");
    lv_obj_set_style_text_color(start_lbl, lv_color_hex(0x101014), 0);
    /* Explicit font — without this it inherits LV_FONT_DEFAULT (32px),
     * which barely fits inside a 120x34 button. */
    lv_obj_set_style_text_font(start_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(start_lbl);

#if BOARD_HAS_ENCODER
    s_focused_field = 0;
    input_driver_set_encoder_handler(encoder_handler);
#endif

    refresh_labels();
    lv_scr_load(s_screen);
}

void setup_screen_destroy() {
    if (s_screen) {
        /* Async delete: safe to call even though this can run from inside
         * a widget event on this very screen (the Start Game button, or
         * the encoder handler's own call stack) — it defers the actual
         * free until LVGL is done with the current event/redraw pass. */
        lv_obj_del_async(s_screen);
        s_screen = nullptr;
    }
    /* Don't leave stale pointers into the about-to-be-freed widget tree
     * around — see the matching comment in counter_screen_destroy(). This
     * screen can now be revisited (menu screen's New Game button), so a
     * late-firing encoder/touch event finding these still set is a real
     * dangling-pointer risk, not just a hypothetical one. */
    s_count_label = nullptr;
    s_life_label = nullptr;
    s_count_field_box = nullptr;
    s_life_field_box = nullptr;
}
