/**
 * arduino_ide_display_test.ino — minimal display bring-up test using YOUR
 * OWN vendor example code verbatim (the LGFX class you pasted earlier,
 * straight from wherever you found the Meshnology/CrowPanel S3 example),
 * built in Arduino IDE instead of PlatformIO. This removes PlatformIO
 * board-profile/flash-config guessing entirely from the picture — if this
 * doesn't show color either, the problem is almost certainly hardware
 * (wiring/FPC/panel), not anything in the PlatformIO project.
 *
 * SETUP (Arduino IDE):
 * 1. Tools > Board > Boards Manager: install "esp32 by Espressif Systems"
 *    if you haven't already (any recent 2.x or 3.x version is fine).
 * 2. Sketch > Include Library > Manage Libraries: install "LovyanGFX"
 *    (by lovyan03).
 * 3. Tools > Board: select "ESP32S3 Dev Module".
 * 4. Tools > USB CDC On Boot: "Enabled"  <-- IMPORTANT, this board has no
 *    separate USB-serial chip, so without this you get no serial output.
 * 5. Tools > Port: select the board's COM port.
 * 6. Upload, then open Serial Monitor at 115200 baud.
 *
 * Expected result if the panel is alive: screen cycles red/green/blue/
 * white/black once a second, serial prints which color each time.
 */

#include <LovyanGFX.hpp>

/* ---- Pins, taken from the Elecrow CrowPanel 1.28" wiki (confirmed) ---- */
#define PIN_TFT_SCLK 10
#define PIN_TFT_MOSI 11
#define PIN_TFT_CS   9
#define PIN_TFT_DC   3
#define PIN_TFT_RST  14
#define PIN_TFT_BL   46

/* ---- This is your own vendor snippet, reproduced verbatim ---- */
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read = 20000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = PIN_TFT_SCLK;
      cfg.pin_mosi = PIN_TFT_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = PIN_TFT_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = PIN_TFT_CS;
      cfg.pin_rst = PIN_TFT_RST;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 240;
      cfg.panel_width = 240;
      cfg.panel_height = 240;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};
LGFX gfx;
/* NOTE: the vendor snippet's touch line
 *   CST816D touch(TP_I2C_SDA_PIN, TP_I2C_SCL_PIN, TP_RST, TP_INT);
 * is deliberately left out here — CST816D is a separate vendor library
 * this test doesn't need, and we're isolating the DISPLAY only. */

void setup() {
  Serial.begin(115200);
  delay(2500);
  Serial.println();
  Serial.println("=== Arduino IDE display test starting ===");

  /* THE MISSING PIECE: per the factory source code, GPIO1 and GPIO2 are
   * power-enable rails that must be driven HIGH before the display will
   * do anything -- likely a level-shifter/LDO enable for the panel's
   * logic supply. Without this the GC9A01 controller itself has no
   * power, even though the backlight (a separate line) can still glow. */
  pinMode(1, OUTPUT);
  digitalWrite(1, HIGH);
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  Serial.println("Display power rails (GPIO1, GPIO2) driven HIGH.");

  Serial.println("Calling gfx.init() ...");
  bool ok = gfx.init();
  Serial.printf("gfx.init() returned: %s\n", ok ? "true" : "false");

  /* This snippet never explicitly sets up a backlight object (no
   * Light_PWM instance) -- on this board the panel driver may handle it
   * internally, or the backlight may need to be driven manually. Try
   * both: an explicit HIGH on the backlight pin, in case it's a simple
   * on/off line rather than PWM. */
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);
  Serial.println("Backlight pin driven HIGH directly (in addition to whatever the panel driver did).");
}

void loop() {
  static int step = 0;
  const char *names[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};
  uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000};

  Serial.printf("[test] filling screen: %s\n", names[step % 5]);
  gfx.fillScreen(colors[step % 5]);
  step++;

  delay(1000);
}
