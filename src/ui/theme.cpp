#include "theme.h"

struct theme_preset_t {
    const char *name;
    uint32_t accent_hex;
};

static const theme_preset_t PRESETS[THEME_PRESET_COUNT] = {
    {"Gold",   0xc9a227},
    {"Azure",  0x2f8fd6},
    {"Crimson", 0xc9432f},
    {"Forest", 0x3f9e5a},
};

static uint8_t s_theme_index = 0;

lv_color_t theme_accent() {
    return lv_color_hex(PRESETS[s_theme_index].accent_hex);
}

const char *theme_cycle() {
    s_theme_index = (s_theme_index + 1) % THEME_PRESET_COUNT;
    return PRESETS[s_theme_index].name;
}

const char *theme_name() {
    return PRESETS[s_theme_index].name;
}
