#include "player_detail_overlay.h"
#include "counter_screen.h"
#include "../ui/theme.h"
#include "../app_state.h"
#include <stdio.h>

static lv_obj_t *s_overlay = nullptr;
static uint8_t s_player_idx = 0;
/* Overlay can be opened from the counter screen; remember what to return
 * to, same pattern as dice_overlay. */
static app_screen_t s_return_screen = SCREEN_COUNTER;

static lv_obj_t *s_poison_lbl = nullptr;
static lv_obj_t *s_energy_lbl = nullptr;
static lv_obj_t *s_cmd_lbls[MAX_PLAYERS];
static uint8_t s_cmd_lbl_owner[MAX_PLAYERS]; /* which opponent index each s_cmd_lbls slot belongs to */

static void close_btn_cb(lv_event_t *e) {
    (void)e;
    player_detail_overlay_close();
}

static void refresh_poison_label() {
    if (!s_poison_lbl) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", g_game.players[s_player_idx].poison);
    lv_label_set_text(s_poison_lbl, buf);
}
static void refresh_energy_label() {
    if (!s_energy_lbl) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", g_game.players[s_player_idx].energy);
    lv_label_set_text(s_energy_lbl, buf);
}
static void refresh_cmd_label(uint8_t slot) {
    if (!s_cmd_lbls[slot]) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", g_game.players[s_player_idx].commander_damage[s_cmd_lbl_owner[slot]]);
    lv_label_set_text(s_cmd_lbls[slot], buf);
}

static void poison_minus_cb(lv_event_t *e) { (void)e; app_state_adjust_poison(s_player_idx, -1); refresh_poison_label(); }
static void poison_plus_cb(lv_event_t *e)  { (void)e; app_state_adjust_poison(s_player_idx, +1); refresh_poison_label(); }
static void energy_minus_cb(lv_event_t *e) { (void)e; app_state_adjust_energy(s_player_idx, -1); refresh_energy_label(); }
static void energy_plus_cb(lv_event_t *e)  { (void)e; app_state_adjust_energy(s_player_idx, +1); refresh_energy_label(); }

static void cmd_minus_cb(lv_event_t *e) {
    uint8_t slot = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    app_state_adjust_commander_damage(s_player_idx, s_cmd_lbl_owner[slot], -1);
    refresh_cmd_label(slot);
    /* Commander damage moves life too now (app_state.cpp) — refresh the
     * counter screen underneath so its life total reflects that live,
     * instead of only catching up the next time it's rebuilt. This
     * overlay only ever opens on top of the counter screen, so its
     * widgets are still valid to touch here. */
    counter_screen_refresh();
}
static void cmd_plus_cb(lv_event_t *e) {
    uint8_t slot = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    app_state_adjust_commander_damage(s_player_idx, s_cmd_lbl_owner[slot], +1);
    refresh_cmd_label(slot);
    counter_screen_refresh();
}

/* A "label — value — [-][+]" row, 190px wide, used for poison/energy/
 * commander-damage lines. Returns the value label so callers can stash it
 * for refreshing later. */
static lv_obj_t *stepper_row(lv_obj_t *parent, const char *text, int16_t initial,
                              lv_event_cb_t minus_cb, lv_event_cb_t plus_cb, void *user_data) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 190, 30);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, THEME_LIFE_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *minus = lv_btn_create(row);
    lv_obj_set_size(minus, 28, 26);
    lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -66, 0);
    lv_obj_set_style_bg_color(minus, THEME_BTN_MINUS, 0);
    lv_obj_add_event_cb(minus, minus_cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *minus_lbl = lv_label_create(minus);
    lv_label_set_text(minus_lbl, "-");
    lv_obj_set_style_text_font(minus_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(minus_lbl);

    lv_obj_t *value = lv_label_create(row);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", initial);
    lv_label_set_text(value, buf);
    lv_obj_set_style_text_color(value, theme_accent(), 0);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_16, 0);
    lv_obj_align(value, LV_ALIGN_RIGHT_MID, -34, 0);

    lv_obj_t *plus = lv_btn_create(row);
    lv_obj_set_size(plus, 28, 26);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(plus, THEME_BTN_PLUS, 0);
    lv_obj_add_event_cb(plus, plus_cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *plus_lbl = lv_label_create(plus);
    lv_label_set_text(plus_lbl, "+");
    lv_obj_set_style_text_font(plus_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(plus_lbl);

    return value;
}

void player_detail_overlay_open(uint8_t player_idx) {
    if (s_overlay) return;
    s_player_idx = player_idx;
    s_return_screen = g_game.current_screen;
    g_game.current_screen = SCREEN_PLAYER_DETAIL_OVERLAY;

    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        s_cmd_lbls[i] = nullptr;
    }

    s_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_overlay, 240, 240);
    lv_obj_center(s_overlay);
    lv_obj_set_style_bg_color(s_overlay, THEME_BG, 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_radius(s_overlay, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_flex_flow(s_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_overlay, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(s_overlay, 32, 0);
    lv_obj_set_style_pad_bottom(s_overlay, 28, 0);
    lv_obj_set_style_pad_row(s_overlay, 8, 0);
    lv_obj_set_scroll_dir(s_overlay, LV_DIR_VER);

    lv_obj_t *title = lv_label_create(s_overlay);
    char title_buf[16];
    snprintf(title_buf, sizeof(title_buf), "Player %d", player_idx + 1);
    lv_label_set_text(title, title_buf);
    lv_obj_set_style_text_color(title, theme_accent(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    if (g_game.track_poison) {
        s_poison_lbl = stepper_row(s_overlay, "Poison", g_game.players[player_idx].poison,
                                    poison_minus_cb, poison_plus_cb, nullptr);
    } else {
        s_poison_lbl = nullptr;
    }

    if (g_game.track_energy) {
        s_energy_lbl = stepper_row(s_overlay, "Energy", g_game.players[player_idx].energy,
                                    energy_minus_cb, energy_plus_cb, nullptr);
    } else {
        s_energy_lbl = nullptr;
    }

    if (g_game.track_commander_damage) {
        lv_obj_t *section = lv_label_create(s_overlay);
        lv_label_set_text(section, "COMMANDER DAMAGE TAKEN");
        lv_obj_set_style_text_color(section, theme_accent(), 0);
        lv_obj_set_style_text_font(section, &lv_font_montserrat_12, 0);

        uint8_t slot = 0;
        for (uint8_t j = 0; j < g_game.player_count; j++) {
            if (j == player_idx) continue;
            char row_text[16];
            snprintf(row_text, sizeof(row_text), "From P%d", j + 1);
            s_cmd_lbl_owner[slot] = j;
            s_cmd_lbls[slot] = stepper_row(s_overlay, row_text,
                                            g_game.players[player_idx].commander_damage[j],
                                            cmd_minus_cb, cmd_plus_cb,
                                            (void *)(uintptr_t)slot);
            slot++;
        }
    }

    lv_obj_t *close_btn = lv_btn_create(s_overlay);
    lv_obj_set_size(close_btn, 100, 32);
    lv_obj_set_style_bg_color(close_btn, THEME_PANEL, 0);
    lv_obj_add_event_cb(close_btn, close_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "Close");
    lv_obj_set_style_text_font(close_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(close_lbl);
}

void player_detail_overlay_close() {
    if (!s_overlay) return;
    lv_obj_del_async(s_overlay);
    s_overlay = nullptr;
    s_poison_lbl = nullptr;
    s_energy_lbl = nullptr;
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) s_cmd_lbls[i] = nullptr;
    g_game.current_screen = s_return_screen;
}

bool player_detail_overlay_is_open() {
    return s_overlay != nullptr;
}

void player_detail_overlay_discard() {
    if (!s_overlay) return;
    /* No lv_obj_del_async() here — see the matching comment in
     * dice_overlay_discard(). The screen tearing itself down owns this
     * object as a child and will free it; we just stop pointing at it. */
    s_overlay = nullptr;
    s_poison_lbl = nullptr;
    s_energy_lbl = nullptr;
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) s_cmd_lbls[i] = nullptr;
}
