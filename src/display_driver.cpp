#include <Arduino.h>
#include "display_driver.h"

LGFX gfx;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISPLAY_HOR_RES * 40];
static lv_color_t buf2[DISPLAY_HOR_RES * 40];

void display_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    gfx.writePixels((lgfx::rgb565_t *)color_p, w * h);
    gfx.endWrite();

    lv_disp_flush_ready(disp);
}

void display_driver_init(lv_disp_drv_t *disp_drv) {
#ifdef PIN_DISPLAY_PWR_EN1
    /* Power-enable rails the display circuit needs before it will respond
     * to anything — see the comment in board_config.h. Only defined on
     * boards that need it. */
    pinMode(PIN_DISPLAY_PWR_EN1, OUTPUT);
    digitalWrite(PIN_DISPLAY_PWR_EN1, HIGH);
#endif
#ifdef PIN_DISPLAY_PWR_EN2
    pinMode(PIN_DISPLAY_PWR_EN2, OUTPUT);
    digitalWrite(PIN_DISPLAY_PWR_EN2, HIGH);
#endif

    gfx.init();
    gfx.setRotation(DISPLAY_ROTATION);
    gfx.setBrightness(255);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISPLAY_HOR_RES * 40);

    lv_disp_drv_init(disp_drv);
    disp_drv->hor_res = DISPLAY_HOR_RES;
    disp_drv->ver_res = DISPLAY_VER_RES;
    disp_drv->flush_cb = display_flush_cb;
    disp_drv->draw_buf = &draw_buf;
}
