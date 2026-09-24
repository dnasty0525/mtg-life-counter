/**
 * display_driver.h — LovyanGFX panel/bus setup per board, plus the LVGL
 * display flush callback that bridges LVGL's draw buffer to the panel.
 *
 * Both boards use a GC9A01-class 240x240 round SPI panel, so the two
 * LGFX subclasses differ only in pin assignment (pulled from board_config.h).
 */

#pragma once

#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "board_config.h"

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;
#if BOARD_HAS_TOUCH
    /* Both boards' touch chips (DIYmalls: CST816S, Meshnology: CST816D) are
     * the same CST816-family register layout; LovyanGFX's CST816S driver
     * talks to both in practice. If touch misbehaves on the Meshnology
     * board specifically, that's the first thing to suspect. */
    lgfx::Touch_CST816S _touch_instance;
#endif

public:
    LGFX() {
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
            cfg.readable   = false;
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
            cfg.pwm_channel = TFT_BL_PWM_CHANNEL;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
#if BOARD_HAS_TOUCH
        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;
            cfg.x_max = DISPLAY_HOR_RES - 1;
            cfg.y_min = 0;
            cfg.y_max = DISPLAY_VER_RES - 1;
            cfg.pin_int = PIN_TOUCH_INT;
            cfg.pin_rst = PIN_TOUCH_RST;
            cfg.pin_sda = PIN_TOUCH_SDA;
            cfg.pin_scl = PIN_TOUCH_SCL;
            cfg.i2c_addr = TOUCH_I2C_ADDR;
            cfg.i2c_port = 0;
            cfg.freq = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
#endif
        setPanel(&_panel_instance);
    }
};

/* Global panel instance + LVGL draw buffer, initialized in display_driver.cpp */
extern LGFX gfx;

void display_driver_init(lv_disp_drv_t *disp_drv);
void display_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
