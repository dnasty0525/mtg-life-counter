/**
 * main.cpp — entry point. Brings up the display + input drivers, LVGL,
 * app state, and shows the setup screen.
 */

#include <Arduino.h>
#include <lvgl.h>
#include "display_driver.h"
#include "input_driver.h"
#include "app_state.h"
#include "led_driver.h"
#include "net/table_sync.h"
#include "screens/setup_screen.h"
#include "board_config.h"

static lv_disp_drv_t disp_drv;
static uint32_t s_last_heartbeat_ms = 0;

void setup() {
    Serial.begin(115200);
    /* Native-USB boards (no separate USB-serial chip) need a beat for the
     * host OS to enumerate the CDC port and for you to get the monitor
     * open — without this, early prints are dropped on the floor before
     * anyone's listening, which looks identical to "nothing ran at all". */
    delay(2000);
    Serial.println();
    Serial.println("=================================");
    Serial.printf("MTG Life Counter booting on %s\n", BOARD_NAME);
    Serial.println("=================================");

    Serial.println("[boot] lv_init()...");
    lv_init();

    Serial.println("[boot] display_driver_init()...");
    display_driver_init(&disp_drv);
    lv_disp_drv_register(&disp_drv);
    Serial.println("[boot] display driver registered OK");

    Serial.println("[boot] input_driver_init()...");
    input_driver_init();

    Serial.println("[boot] led_driver_init()...");
    led_driver_init();

    Serial.println("[boot] table_sync_init()...");
    table_sync_init();

    /* Seed Arduino's random() from the hardware RNG (esp_random() draws
     * from the ESP32's true hardware entropy source) so "random first
     * player" isn't the same pick every boot. */
    randomSeed(esp_random());

    Serial.println("[boot] app_state_reset_game()...");
    app_state_reset_game(2, DEFAULT_START_LIFE);

    /* One-time default LED turn-color assignment (Red/Blue/Green/Purple
     * for P1-P4) — g_game is zero-initialized, which would otherwise
     * leave every player on the same color (index 0). This is a
     * preference like track_poison, so app_state_reset_game() itself
     * deliberately leaves it alone on every later New Game; it only
     * needs seeding once, here, at boot. */
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        g_game.player_led_color_idx[i] = i;
    }

    Serial.println("[boot] setup_screen_create()...");
    setup_screen_create();

    Serial.println("[boot] setup() complete, entering loop()");
}

void loop() {
    lv_timer_handler();
    input_driver_poll();
    table_sync_poll();

    /* Once-a-second heartbeat so a monitor opened late still confirms the
     * firmware is alive and looping, not just silently crashed. */
    uint32_t now = millis();
    if (now - s_last_heartbeat_ms >= 1000) {
        s_last_heartbeat_ms = now;
        Serial.printf("[heartbeat] uptime=%lus\n", now / 1000);
    }

    delay(5);
}
