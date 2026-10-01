#include "ui.h"

#include <Arduino.h>

#include "screens.h"
#include "ui_common.h"

namespace ui {
namespace {

constexpr uint32_t WAKE_MS = 60000;  // แตะตอนโหมดนาฬิกา -> กลับหน้าปกติ 60 วินาที

lv_obj_t* s_pages[PAGE_COUNT];
lv_obj_t* s_clock = nullptr;
int s_cur = 0;
bool s_clockMode = false;
uint32_t s_wakeUntil = 0;
uint32_t s_lastGesture = 0;
const MarketData* s_md = nullptr;

void load(lv_obj_t* scr) {
  if (lv_screen_active() != scr) lv_screen_load(scr);
}

void show(int i) {
  s_cur = (i % PAGE_COUNT + PAGE_COUNT) % PAGE_COUNT;
  load(s_pages[s_cur]);
}

void wake() {
  s_wakeUntil = millis() + WAKE_MS;
  s_clockMode = false;
  load(s_pages[s_cur]);
}

// ปัดซ้าย/ขวา = เปลี่ยนหน้า, แตะ = หน้าถัดไป
void nav_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_GESTURE) {
    s_lastGesture = lv_tick_get();
    if (s_clockMode) return wake();
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT) show(s_cur + 1);
    else if (dir == LV_DIR_RIGHT) show(s_cur - 1);
  } else if (code == LV_EVENT_CLICKED) {
    if (lv_tick_elaps(s_lastGesture) < 500) return;  // CLICKED ตามหลัง gesture
    if (s_clockMode) return wake();
    show(s_cur + 1);
  }
}

void attach(lv_obj_t* scr) {
  lv_obj_add_event_cb(scr, nav_cb, LV_EVENT_GESTURE, nullptr);
  lv_obj_add_event_cb(scr, nav_cb, LV_EVENT_CLICKED, nullptr);
}

}  // namespace

void init() {
  s_pages[0] = scr_bubbles::create();
  s_pages[1] = scr_overview::create();
  s_pages[2] = scr_mood::create();
  s_pages[3] = scr_chart::create();
  s_clock = scr_clock::create();
  for (lv_obj_t* p : s_pages) attach(p);
  attach(s_clock);
  lv_screen_load(s_pages[0]);
}

void update(const MarketData& md) {
  s_md = &md;
  scr_overview::update(md);
  scr_mood::update(md);
  scr_bubbles::update(md);
  scr_chart::update(md);
  scr_clock::update(md);
}

void tick(const NetStatus& st, bool dark) {
  chrome_update(s_md, st);
  scr_chart::tick();
  scr_clock::tick();

  bool wantClock = dark && (int32_t)(millis() - s_wakeUntil) >= 0;
  if (wantClock != s_clockMode) {
    s_clockMode = wantClock;
    load(s_clockMode ? s_clock : s_pages[s_cur]);
  }
}

bool inClockMode() { return s_clockMode; }

}  // namespace ui
