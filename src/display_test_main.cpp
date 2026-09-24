/**
 * display_test.cpp — minimal bring-up test for the Meshnology/CrowPanel
 * S3 board. Deliberately has NO dependency on LVGL, touch, or the
 * encoder — just raw LovyanGFX driving the GC9A01 panel. This exists to
 * answer one question in isolation: does the panel itself light up with
 * the pins/settings in boards/board_config.h?
 *
 * Build + flash with:
 *   pio run -e meshnology_s3_display_test -t upload
 *   pio device monitor
 *
 * What to expect if it's working: the screen cycles red / green / blue /
 * white every second, and the serial monitor prints a line each time it
 * switches. If the screen stays blank but you see the serial prints,
 * the MCU/pins for touch+display bus are fine and the bug is somewhere
 * in the LVGL/app layer. If you don't even get serial prints, it's a
 * boot-level problem (flash/PSRAM config, wrong board pins, or power).
 */

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "board_config.h"

class LGFXTest : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFXTest() {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = TFT_SPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = TFT_FREQ_WRITE;
            cfg.freq_read  = TFT_FREQ_READ;
            cfg.spi_3wire  = true;
            cfg.use_lock   = true;
            cfg.dma_channel = TFT_DMA_CHANNEL;
            cfg.pin_sclk = PIN_TFT_SCLK;
            cfg.pin_mosi = PIN_TFT_MOSI;
            cfg.pin_miso = -1;
            cfg.pin_dc   = PIN_TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = PIN_TFT_CS;
            cfg.pin_rst  = PIN_TFT_RST;
            cfg.pin_busy = -1;
            cfg.memory_width  = TFT_MEMORY_WIDTH;
            cfg.memory_height = TFT_MEMORY_HEIGHT;
            cfg.panel_width  = DISPLAY_HOR_RES;
            cfg.panel_height = DISPLAY_VER_RES;
            cfg.offset_rotation = DISPLAY_ROTATION;
            cfg.dummy_read_pixel = TFT_DUMMY_READ_PIXEL;
            cfg.dummy_read_bits  = TFT_DUMMY_READ_BITS;
            cfg.readable   = true;  /* enabled here (only in this test) so readPanelID() can work */
            cfg.invert     = true;
            cfg.rgb_order  = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = PIN_TFT_BL;
            cfg.invert = false;
            cfg.freq   = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        setPanel(&_panel_instance);
    }
};

static LGFXTest gfx;

static bool s_init_ok = false;

void setup() {
    Serial.begin(115200);
    delay(2500); /* give the USB CDC port time to enumerate + you time to open the monitor */
    Serial.println();
    Serial.println("=== display_test starting ===");
    Serial.printf("Board: %s\n", BOARD_NAME);
    Serial.printf("Pins -> SCLK=%d MOSI=%d CS=%d DC=%d RST=%d BL=%d\n",
                   PIN_TFT_SCLK, PIN_TFT_MOSI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST, PIN_TFT_BL);

    /* Per the factory source code: GPIO1/GPIO2 are power-enable rails for
     * the display circuit and must be HIGH before the panel will respond
     * to anything, regardless of backlight state. */
    pinMode(PIN_DISPLAY_PWR_EN1, OUTPUT);
    digitalWrite(PIN_DISPLAY_PWR_EN1, HIGH);
    pinMode(PIN_DISPLAY_PWR_EN2, OUTPUT);
    digitalWrite(PIN_DISPLAY_PWR_EN2, HIGH);
    Serial.println("Display power rails enabled.");

    Serial.println("Calling gfx.init() ...");
    s_init_ok = gfx.init();
    Serial.printf("gfx.init() returned: %s\n", s_init_ok ? "true" : "false");

    gfx.setRotation(DISPLAY_ROTATION);
    gfx.setBrightness(255);
    Serial.println("Brightness set to 255. If the panel is alive, colors should start cycling now.");
}

void loop() {
    static int step = 0;
    const char *names[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};
    uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000};

    /* Repeat the init result every cycle too, so it's visible no matter
     * when you happen to open the serial monitor. Also flip invertDisplay
     * each full 5-color lap — wrong invert is a common cause of a GC9A01
     * clone appearing to do nothing even though it's receiving real data. */
    bool invert = (step / 5) % 2 == 1;
    gfx.invertDisplay(invert);
    Serial.printf("[display_test] init_ok=%s invert=%s filling screen: %s\n",
                  s_init_ok ? "true" : "false", invert ? "true" : "false", names[step % 5]);
    gfx.fillScreen(colors[step % 5]);
    step++;

    delay(1000);
}
