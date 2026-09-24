/**
 * pin_finder.ino — standalone diagnostic sketch to find the Meshnology
 * ESP32-S3 board's rotary encoder (A/B/button) pinout by observation.
 *
 * This is a separate Arduino sketch, NOT part of the PlatformIO build —
 * flash it on its own (Arduino IDE, or `arduino-cli compile --upload`)
 * before you rely on boards/board_config.h's BOARD_MESHNOLOGY_S3 pins.
 *
 * HOW TO USE
 * 1. Flash this sketch to the Meshnology ESP32-S3 board.
 * 2. Open the Serial Monitor at 115200 baud.
 * 3. Slowly turn the knob one detent at a time, then press the button.
 * 4. Watch which GPIO numbers print "CHANGED" and in what pattern:
 *      - Two pins that flip back and forth together as you turn        -> encoder A/B
 *      - One pin that goes LOW once per press, HIGH on release          -> button
 * 5. Update PIN_ENCODER_A / PIN_ENCODER_B / PIN_ENCODER_BTN in
 *    boards/board_config.h (under BOARD_MESHNOLOGY_S3) with what you find.
 *
 * CANDIDATE_PINS below covers the commonly broken-out GPIOs on ESP32-S3
 * dev boards. If your board exposes different pins (check the silkscreen
 * or a schematic if one came with it), edit the list.
 */

const int CANDIDATE_PINS[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18,
    21, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 45, 46, 47, 48
};
const int NUM_PINS = sizeof(CANDIDATE_PINS) / sizeof(CANDIDATE_PINS[0]);

int last_state[64];

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("=== pin_finder: encoder/button pinout scanner ===");
    Serial.println("Turn the knob slowly, then press its button. Watching for changes...");
    Serial.println();

    for (int i = 0; i < NUM_PINS; i++) {
        pinMode(CANDIDATE_PINS[i], INPUT_PULLUP);
        last_state[i] = digitalRead(CANDIDATE_PINS[i]);
    }
}

void loop() {
    for (int i = 0; i < NUM_PINS; i++) {
        int v = digitalRead(CANDIDATE_PINS[i]);
        if (v != last_state[i]) {
            Serial.printf("GPIO %2d CHANGED -> %s  (t=%lums)\n",
                          CANDIDATE_PINS[i], v ? "HIGH" : "LOW", millis());
            last_state[i] = v;
        }
    }
    delay(2);
}
