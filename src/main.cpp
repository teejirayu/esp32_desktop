// US Market Desk Gauge — ESP32 CYD
#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "display.h"
#include "hw.h"
#include "net.h"
#include "ui/ui.h"

static MarketData g_md;  // สำเนาฝั่ง UI (core 1)
static NetStatus g_st;

static void checkAlerts() {
  const Quote* idx = g_md.headline();
  const Quote* vix = g_md.find("VIX");
  hw::Alert a = hw::Alert::NONE;
  if (vix && vix->p > ALERT_VIX_LEVEL) a = hw::Alert::DOWN;
  else if (idx && idx->c <= -ALERT_SPX_PCT) a = hw::Alert::DOWN;
  else if (idx && idx->c >= ALERT_SPX_PCT) a = hw::Alert::UP;
  hw::setAlert(a);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n[main] US Market Desk Gauge");
  hw::begin();
  display::begin();
  ui::init();
  net::begin();
  Serial.printf("[main] heap after init=%u\n", ESP.getFreeHeap());
}

void loop() {
  static uint32_t lastTick = 0;

  if (net::take(g_md, g_st)) {
    ui::update(g_md);
    checkAlerts();
  }

  uint32_t now = millis();
  if (now - lastTick >= 1000) {
    lastTick = now;
    ui::tick(g_st, hw::isDark());
  }

  hw::loop(ui::inClockMode());
  lv_timer_handler();
  delay(5);
}
