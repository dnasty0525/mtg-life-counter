#include "led_driver.h"
#include "led_colors.h"
#include "app_state.h"
#include "board_config.h"

#ifdef PIN_RGB_LED
#include <Adafruit_NeoPixel.h>
static Adafruit_NeoPixel s_strip(RGB_LED_NUM, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);
#endif

static bool s_enabled = false;
static uint8_t s_brightness = 128;
static uint8_t s_ambient_color_idx = 4; /* "Amber" — matches the old hardcoded default */
static bool s_turn_colors_enabled = false;

void led_driver_init() {
#ifdef PIN_RGB_LED
    s_strip.begin();
    s_strip.clear();
    s_strip.show();
#endif
}

void led_driver_set(bool on, uint8_t brightness, uint8_t r, uint8_t g, uint8_t b) {
#ifdef PIN_RGB_LED
    if (!on) {
        s_strip.clear();
        s_strip.show();
        return;
    }
    s_strip.setBrightness(brightness);
    for (uint16_t i = 0; i < RGB_LED_NUM; i++) {
        s_strip.setPixelColor(i, s_strip.Color(r, g, b));
    }
    s_strip.show();
#else
    (void)on; (void)brightness; (void)r; (void)g; (void)b;
#endif
}

bool led_driver_available() {
#ifdef PIN_RGB_LED
    return true;
#else
    return false;
#endif
}

void led_driver_set_enabled(bool on) { s_enabled = on; }
bool led_driver_is_enabled() { return s_enabled; }

void led_driver_set_brightness(uint8_t brightness) { s_brightness = brightness; }
uint8_t led_driver_get_brightness() { return s_brightness; }

void led_driver_set_ambient_color_idx(uint8_t idx) {
    if (idx >= LED_COLOR_COUNT) idx = 0;
    s_ambient_color_idx = idx;
}
uint8_t led_driver_get_ambient_color_idx() { return s_ambient_color_idx; }

void led_driver_set_turn_colors_enabled(bool enabled) { s_turn_colors_enabled = enabled; }
bool led_driver_is_turn_colors_enabled() { return s_turn_colors_enabled; }

void led_driver_refresh() {
    if (!led_driver_available()) return;

    if (!s_enabled) {
        led_driver_set(false, 0, 0, 0, 0);
        return;
    }

    uint8_t color_idx = s_ambient_color_idx;
    if (s_turn_colors_enabled) {
        color_idx = g_game.player_led_color_idx[g_game.selected_player];
        if (color_idx >= LED_COLOR_COUNT) color_idx = 0;
    }
    const led_color_t &c = LED_COLORS[color_idx];
    led_driver_set(true, s_brightness, c.r, c.g, c.b);
}
