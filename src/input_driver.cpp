#include "input_driver.h"
#include "display_driver.h"

#if BOARD_HAS_TOUCH
static lv_indev_drv_t touch_indev_drv;

static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    lgfx::touch_point_t tp;
    if (gfx.getTouch(&tp)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = tp.x;
        data->point.y = tp.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
#endif

#if BOARD_HAS_ENCODER
static volatile int32_t s_enc_raw_count = 0;
static int32_t s_enc_last_reported = 0;

static void IRAM_ATTR encoder_isr() {
    /* Simple 2-bit quadrature decode. Not glitch-filtered in hardware, so
     * treat single-step noise as acceptable; a hardware RC filter on A/B
     * is recommended once the real pinout is confirmed. */
    static uint8_t last_state = 0;
    uint8_t a = digitalRead(PIN_ENCODER_A);
    uint8_t b = digitalRead(PIN_ENCODER_B);
    uint8_t state = (a << 1) | b;
    static const int8_t transition_table[16] = {
        0, -1, 1, 0,
        1, 0, 0, -1,
        -1, 0, 0, 1,
        0, 1, -1, 0
    };
    uint8_t idx = (last_state << 2) | state;
    s_enc_raw_count += transition_table[idx & 0x0F];
    last_state = state;
}

static input_encoder_cb_t s_enc_cb = nullptr;
static bool s_btn_down = false;
static uint32_t s_btn_down_at = 0;
static bool s_long_press_fired = false;

/* Debounce: a mechanical/tactile encoder button's contacts chatter for a
 * few ms on every press and release. Without filtering that out, a single
 * physical click can be read as press-release-press-release in the same
 * loop() burst, firing several INPUT_ENC_CLICK events for what the user
 * felt as one click — most noticeable right after a long-press (e.g.
 * going back from the menu screen), where a bounce on release can look
 * like an extra quick click immediately afterward. Require the raw
 * reading to hold steady for BTN_DEBOUNCE_MS before it's trusted. */
#define BTN_DEBOUNCE_MS 25
static bool s_btn_raw_candidate = false;
static uint32_t s_btn_candidate_since = 0;
#endif

void input_driver_init() {
#if BOARD_HAS_TOUCH
    lv_indev_drv_init(&touch_indev_drv);
    touch_indev_drv.type = LV_INDEV_TYPE_POINTER;
    touch_indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&touch_indev_drv);
#endif

#if BOARD_HAS_ENCODER
    pinMode(PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PIN_ENCODER_B, INPUT_PULLUP);
    pinMode(PIN_ENCODER_BTN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B), encoder_isr, CHANGE);
#endif
}

void input_driver_set_encoder_handler(input_encoder_cb_t cb) {
#if BOARD_HAS_ENCODER
    s_enc_cb = cb;
#else
    (void)cb;
#endif
}

void input_driver_poll() {
#if BOARD_HAS_ENCODER
    /* Quadrature ISR increments by 4 per physical detent on most
     * encoders — divide down so one detent == one step. Adjust
     * ENC_STEPS_PER_DETENT if your knob feels 2x/4x too sensitive. */
    const int32_t ENC_STEPS_PER_DETENT = 4;
    int32_t raw = s_enc_raw_count;
    int32_t detents = (raw - s_enc_last_reported) / ENC_STEPS_PER_DETENT;
    if (detents != 0) {
        s_enc_last_reported += detents * ENC_STEPS_PER_DETENT;
        if (s_enc_cb) s_enc_cb(INPUT_ENC_TURN, (int16_t)detents);
    }

    uint32_t now = millis();
    bool raw_pressed = (digitalRead(PIN_ENCODER_BTN) == LOW);

    /* Debounce stage: only let a raw transition through once it's been
     * stable for BTN_DEBOUNCE_MS. */
    if (raw_pressed != s_btn_raw_candidate) {
        s_btn_raw_candidate = raw_pressed;
        s_btn_candidate_since = now;
    }
    bool pressed = s_btn_down; /* debounced state carried across calls */
    if (s_btn_raw_candidate != s_btn_down &&
        (now - s_btn_candidate_since) >= BTN_DEBOUNCE_MS) {
        pressed = s_btn_raw_candidate;
    }

    if (pressed && !s_btn_down) {
        s_btn_down = true;
        s_btn_down_at = now;
        s_long_press_fired = false;
    } else if (pressed && s_btn_down) {
        if (!s_long_press_fired && (now - s_btn_down_at) >= INPUT_LONG_PRESS_MS) {
            s_long_press_fired = true;
            if (s_enc_cb) s_enc_cb(INPUT_ENC_LONG_PRESS, 0);
        }
    } else if (!pressed && s_btn_down) {
        s_btn_down = false;
        if (!s_long_press_fired) {
            if (s_enc_cb) s_enc_cb(INPUT_ENC_CLICK, 0);
        }
    }
#endif
}
