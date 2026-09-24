/**
 * input_driver.h — input abstraction for the two boards.
 *
 * Touch (DIYmalls C3): registered as a normal LVGL POINTER indev, so
 * ordinary lv_btn / LV_EVENT_CLICKED widgets work with no special-casing —
 * screens just build normal LVGL UIs and touch "just works".
 *
 * Rotary encoder (Meshnology S3): a knob has no x/y position, so it can't
 * drive a pointer. Instead it's read as quadrature pulses + a push-button
 * and turned into the same small set of semantic events a life-counter UI
 * actually needs (adjust value, move selection, confirm, long-press).
 * Screens that want to support both boards from one code path listen for
 * these events via input_driver_set_encoder_handler() *in addition to*
 * building normal touch-clickable widgets — see screens/counter_screen.cpp
 * for the pattern.
 */

#pragma once

#include <lvgl.h>
#include "board_config.h"

typedef enum {
    INPUT_ENC_TURN,        /* data = signed step count (+CW / -CCW) */
    INPUT_ENC_CLICK,       /* short press of the knob button */
    INPUT_ENC_LONG_PRESS,  /* held past INPUT_LONG_PRESS_MS */
} input_encoder_event_t;

typedef void (*input_encoder_cb_t)(input_encoder_event_t evt, int16_t data);

#define INPUT_LONG_PRESS_MS 600

/* Call once from main setup(), after display_driver_init(). Registers the
 * touch pointer indev (if BOARD_HAS_TOUCH) and arms encoder GPIOs/ISRs
 * (if BOARD_HAS_ENCODER). Safe to call unconditionally on either board. */
void input_driver_init();

/* Call from main loop()/lv_timer at least every ~20ms so button debounce
 * and long-press timing stay accurate. No-op on touch-only boards. */
void input_driver_poll();

/* Screens register one handler to receive encoder semantic events. Only
 * meaningful on boards with BOARD_HAS_ENCODER; harmless no-op otherwise. */
void input_driver_set_encoder_handler(input_encoder_cb_t cb);
