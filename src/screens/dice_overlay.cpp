#include "dice_overlay.h"
#include "../ui/theme.h"
#include "../app_state.h"
#include "counter_screen.h"
#include <Arduino.h>
#include <stdio.h>

static lv_obj_t *s_overlay = nullptr;
static lv_obj_t *s_result_label = nullptr;
static lv_obj_t *s_detail_label = nullptr; /* "3 + 4" under the 2d6 total; blank for d20 */
static lv_obj_t *s_roll_btn = nullptr;
static lv_obj_t *s_tab_btns[2] = { nullptr, nullptr };
static lv_anim_t s_spin_anim;
static bool s_rolling = false;
static dice_mode_t s_mode = DICE_MODE_D20;
/* The dice roller can be opened from either the counter screen or the
 * menu screen — remember which so close() returns to the right one
 * instead of always assuming SCREEN_COUNTER. */
static app_screen_t s_return_screen = SCREEN_COUNTER;

/* Uniform 1..sides from the ESP32's hardware RNG. (The old code used libc
 * rand(), which nothing seeds on ESP32/Arduino — so every boot rolled the
 * same sequence.) */
static int roll_die(int sides) {
    return (int)random(1, sides + 1);
}

static void close_btn_cb(lv_event_t *e) {
    (void)e;
    dice_overlay_close();
}

static void roll_btn_cb(lv_event_t *e) {
    (void)e;
    dice_overlay_handle_roll();
}

static void set_result_text(const char *main_text, const char *detail_text) {
    if (s_result_label) lv_label_set_text(s_result_label, main_text);
    if (s_detail_label) lv_label_set_text(s_detail_label, detail_text);
}

static void show_placeholder() {
    set_result_text("--", "");
}

static void style_tabs() {
    for (int i = 0; i < 2; i++) {
        if (!s_tab_btns[i]) continue;
        bool sel = (i == (int)s_mode);
        lv_obj_set_style_bg_color(s_tab_btns[i], sel ? theme_accent() : THEME_PANEL, 0);
    }
}

/* Quick "tumbling" animation: cycle random faces for a few frames, then
 * land on the real roll. Keeps it lightweight — no sprite art needed. */
static void spin_anim_exec_cb(void *var, int32_t v) {
    (void)var;
    bool final_frame = (v >= 100);
    char main_buf[8];
    char detail_buf[16];
    detail_buf[0] = '\0';

    if (s_mode == DICE_MODE_D20) {
        snprintf(main_buf, sizeof(main_buf), "%d", roll_die(20));
    } else {
        int a = roll_die(6);
        int b = roll_die(6);
        snprintf(main_buf, sizeof(main_buf), "%d", a + b);
        snprintf(detail_buf, sizeof(detail_buf), "%d + %d", a, b);
    }
    set_result_text(main_buf, detail_buf);
    if (final_frame) s_rolling = false;
}

static void tab_btn_cb(lv_event_t *e) {
    dice_mode_t m = (dice_mode_t)(uintptr_t)lv_event_get_user_data(e);
    if (s_rolling || m == s_mode) return;
    s_mode = m;
    style_tabs();
    show_placeholder();
}

static lv_obj_t *make_tab(lv_obj_t *parent, const char *text, dice_mode_t m, int x_off) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 56, 28);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, x_off, 26);
    lv_obj_add_event_cb(btn, tab_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)m);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl);
    return btn;
}

void dice_overlay_open() {
    dice_overlay_open(s_mode);
}

void dice_overlay_open(dice_mode_t mode) {
    if (s_overlay) return;
    s_mode = mode;
    s_rolling = false;
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

    /* Mode tabs double as the title. */
    s_tab_btns[DICE_MODE_D20] = make_tab(s_overlay, "d20", DICE_MODE_D20, -31);
    s_tab_btns[DICE_MODE_2D6] = make_tab(s_overlay, "2d6", DICE_MODE_2D6, 31);
    style_tabs();

    s_result_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_result_label, THEME_LIFE_TEXT, 0);
    lv_obj_set_style_text_font(s_result_label, &lv_font_montserrat_48, 0);
    lv_obj_align(s_result_label, LV_ALIGN_CENTER, 0, -18);

    s_detail_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_detail_label, theme_accent(), 0);
    lv_obj_set_style_text_font(s_detail_label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_detail_label, LV_ALIGN_CENTER, 0, 14);

    show_placeholder();

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

static void clear_pointers() {
    s_overlay = nullptr;
    s_result_label = nullptr;
    s_detail_label = nullptr;
    s_roll_btn = nullptr;
    s_tab_btns[0] = s_tab_btns[1] = nullptr;
    s_rolling = false;
}

void dice_overlay_close() {
    if (!s_overlay) return;
    /* Async delete — this often runs from inside the Close button's own
     * click event (the button is a child of s_overlay), which is the same
     * unsafe-immediate-delete situation as the screen transitions. */
    lv_obj_del_async(s_overlay);
    clear_pointers();
    g_game.current_screen = s_return_screen;
    /* A remote (table-sync) elimination may have landed while this overlay
     * covered the counter screen; let it play now. No-op if the overlay was
     * opened from the menu (counter screen doesn't exist then). */
    counter_screen_refresh();
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
    clear_pointers();
    /* Don't touch g_game.current_screen — the caller sets that itself via
     * its own *_create(). */
}

void dice_overlay_toggle_mode() {
    if (!s_overlay || s_rolling) return;
    s_mode = (s_mode == DICE_MODE_D20) ? DICE_MODE_2D6 : DICE_MODE_D20;
    style_tabs();
    show_placeholder();
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
