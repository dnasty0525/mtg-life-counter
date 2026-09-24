#include "dice_overlay.h"
#include "../ui/theme.h"
#include "../app_state.h"
#include <stdlib.h>
#include <stdio.h>

static lv_obj_t *s_overlay = nullptr;
static lv_obj_t *s_result_label = nullptr;
static lv_obj_t *s_roll_btn = nullptr;
static lv_anim_t s_spin_anim;
static bool s_rolling = false;
/* The dice roller can be opened from either the counter screen or the
 * menu screen — remember which so close() returns to the right one
 * instead of always assuming SCREEN_COUNTER. */
static app_screen_t s_return_screen = SCREEN_COUNTER;

static void close_btn_cb(lv_event_t *e) {
    (void)e;
    dice_overlay_close();
}

static void roll_btn_cb(lv_event_t *e) {
    (void)e;
    dice_overlay_handle_roll();
}

/* Quick "tumbling" animation: cycle random numbers for a few frames, then
 * land on the real roll. Keeps it lightweight — no sprite art needed. */
static void spin_anim_exec_cb(void *var, int32_t v) {
    (void)var;
    char buf[4];
    if (v < 100) {
        snprintf(buf, sizeof(buf), "%d", (rand() % 20) + 1);
    } else {
        int result = (rand() % 20) + 1;
        snprintf(buf, sizeof(buf), "%d", result);
        s_rolling = false;
    }
    if (s_result_label) lv_label_set_text(s_result_label, buf);
}

void dice_overlay_open() {
    if (s_overlay) return;
    s_return_screen = g_game.current_screen;
    g_game.current_screen = SCREEN_DICE_OVERLAY;

    s_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_overlay, 240, 240);
    lv_obj_center(s_overlay);
    lv_obj_set_style_bg_color(s_overlay, THEME_BG, 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_radius(s_overlay, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_overlay);
    lv_label_set_text(title, "d20");
    lv_obj_set_style_text_color(title, theme_accent(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    s_result_label = lv_label_create(s_overlay);
    lv_label_set_text(s_result_label, "--");
    lv_obj_set_style_text_color(s_result_label, THEME_LIFE_TEXT, 0);
    lv_obj_set_style_text_font(s_result_label, &lv_font_montserrat_48, 0);
    lv_obj_center(s_result_label);

    s_roll_btn = lv_btn_create(s_overlay);
    lv_obj_set_size(s_roll_btn, 90, 40);
    lv_obj_align(s_roll_btn, LV_ALIGN_BOTTOM_MID, 0, -46);
    lv_obj_set_style_bg_color(s_roll_btn, THEME_BTN_PLUS, 0);
    lv_obj_add_event_cb(s_roll_btn, roll_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *roll_lbl = lv_label_create(s_roll_btn);
    lv_label_set_text(roll_lbl, "Roll");
    lv_obj_set_style_text_font(roll_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(roll_lbl);

    lv_obj_t *close_btn = lv_btn_create(s_overlay);
    lv_obj_set_size(close_btn, 60, 28);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(close_btn, THEME_PANEL, 0);
    lv_obj_add_event_cb(close_btn, close_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "Close");
    lv_obj_set_style_text_font(close_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(close_lbl);
}

void dice_overlay_close() {
    if (!s_overlay) return;
    /* Async delete — this often runs from inside the Close button's own
     * click event (the button is a child of s_overlay), which is the same
     * unsafe-immediate-delete situation as the screen transitions. */
    lv_obj_del_async(s_overlay);
    s_overlay = nullptr;
    s_result_label = nullptr;
    s_roll_btn = nullptr;
    g_game.current_screen = s_return_screen;
}

bool dice_overlay_is_open() {
    return s_overlay != nullptr;
}

void dice_overlay_discard() {
    if (!s_overlay) return;
    /* No lv_obj_del_async() here on purpose — the caller's own screen
     * teardown (already in progress) will free this object as one of its
     * children. Queuing a second, separate delete for the same pointer
     * would race it: whichever delete runs first frees the object, and
     * the other one then calls lv_obj_del() on already-freed memory. */
    s_overlay = nullptr;
    s_result_label = nullptr;
    s_roll_btn = nullptr;
    s_rolling = false;
    /* Don't touch g_game.current_screen — the caller sets that itself via
     * its own *_create(). */
}

void dice_overlay_handle_roll() {
    if (!s_overlay || s_rolling) return;
    s_rolling = true;

    lv_anim_init(&s_spin_anim);
    lv_anim_set_var(&s_spin_anim, s_result_label);
    lv_anim_set_values(&s_spin_anim, 0, 100);
    lv_anim_set_time(&s_spin_anim, 500);
    lv_anim_set_exec_cb(&s_spin_anim, spin_anim_exec_cb);
    lv_anim_set_path_cb(&s_spin_anim, lv_anim_path_ease_out);
    lv_anim_start(&s_spin_anim);
}
