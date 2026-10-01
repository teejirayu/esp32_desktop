#include "display.h"

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "config.h"

namespace display {
namespace {

constexpr int SCR_W = 320, SCR_H = 240;
constexpr int BUF_LINES = 24;

constexpr int T_CLK = 25, T_MISO = 39, T_MOSI = 32, T_CS = 33, T_IRQ = 36;

TFT_eSPI tft;
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen ts(T_CS, T_IRQ);

// partial buffer 320x24 px (16-bit) — uint32_t เพื่อ alignment 4 byte
uint32_t s_buf[SCR_W * BUF_LINES / 2];

void flush_cb(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  uint32_t w = lv_area_get_width(a), h = lv_area_get_height(a);
  tft.startWrite();
  tft.setAddrWindow(a->x1, a->y1, w, h);
  tft.pushColors((uint16_t*)px, w * h, true);  // swap byte (LVGL เป็น little-endian)
  tft.endWrite();
  lv_display_flush_ready(d);
}

void touch_cb(lv_indev_t*, lv_indev_data_t* data) {
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCR_W - 1);
    int y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCR_H - 1);
    data->point.x = constrain(x, 0, SCR_W - 1);
    data->point.y = constrain(y, 0, SCR_H - 1);
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

}  // namespace

void begin() {
  tft.begin();
  tft.setRotation(DISPLAY_ROTATION);
  tft.invertDisplay(TFT_INVERT);
  tft.fillScreen(TFT_BLACK);

  touchSpi.begin(T_CLK, T_MISO, T_MOSI, T_CS);
  ts.begin(touchSpi);
  ts.setRotation(DISPLAY_ROTATION);

  lv_init();
  lv_log_register_print_cb([](lv_log_level_t, const char* buf) { Serial.println(buf); });
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  lv_display_t* disp = lv_display_create(SCR_W, SCR_H);
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_buffers(disp, s_buf, nullptr, sizeof(s_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_cb);
}

}  // namespace display
