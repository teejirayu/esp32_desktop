#include "ui_common.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"

namespace ui {

lv_obj_t* make_screen() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, COL_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(scr, COL_AMBER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);  // scroll จะกิน gesture
  return scr;
}

lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* txt) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  lv_label_set_text(l, txt);
  return l;
}

lv_obj_t* make_rect(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t color) {
  lv_obj_t* r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(r, x, y);
  lv_obj_set_size(r, w, h);
  lv_obj_set_style_bg_color(r, color, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
  return r;
}

void set_pill(lv_obj_t* l, lv_color_t bg) {
  lv_obj_set_style_bg_color(l, bg, 0);
  lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(l, 3, 0);
}

lv_color_t chg_color(float c) {
  if (isnan(c) || fabsf(c) < 0.005f) return COL_FLAT;
  return c > 0 ? COL_UP : COL_DOWN;
}

void fmt_price(char* out, size_t n, float p) {
  if (isnan(p)) {
    snprintf(out, n, "--");
    return;
  }
  char tmp[24];
  snprintf(tmp, sizeof(tmp), "%.*f", fabsf(p) < 10 ? 3 : 2, (double)p);
  const char* dot = strchr(tmp, '.');
  int intLen = dot ? (int)(dot - tmp) : (int)strlen(tmp);
  int start = tmp[0] == '-' ? 1 : 0;
  size_t o = 0;
  for (int i = 0; tmp[i] && o + 2 < n; ++i) {
    if (i > start && i < intLen && (intLen - i) % 3 == 0) out[o++] = ',';
    out[o++] = tmp[i];
  }
  out[o] = 0;
}

void fmt_chg(char* out, size_t n, float c) {
  if (isnan(c)) snprintf(out, n, "--");
  else snprintf(out, n, "%+.2f%%", (double)c);
}

void fmt_bps(char* out, size_t n, float p, float c) {
  if (isnan(p) || isnan(c)) {
    snprintf(out, n, "--");
    return;
  }
  float prev = p / (1.f + c / 100.f);
  snprintf(out, n, "%+.1fbp", (double)((p - prev) * 100.f));
}

bool time_valid() { return time(nullptr) > 1700000000; }

const char* market_text(Market m) {
  switch (m) {
    case Market::PRE: return "PRE";
    case Market::OPEN: return "OPEN";
    case Market::POST: return "POST";
    case Market::CLOSED: return "CLOSED";
    default: return "----";
  }
}

lv_color_t market_color(Market m) {
  switch (m) {
    case Market::OPEN: return COL_UP;
    case Market::PRE:
    case Market::POST: return COL_AMBER;
    case Market::CLOSED: return lv_color_hex(0x5A5A5A);
    default: return lv_color_hex(0x303030);
  }
}

// ---------------- header / footer ----------------
namespace {
struct Chrome {
  lv_obj_t *clock, *wifi, *badge, *upd, *stale;
};
Chrome s_chrome[PAGE_COUNT];
int s_nchrome = 0;
}  // namespace

void chrome_create(lv_obj_t* scr, const char* title, int page) {
  Chrome& c = s_chrome[page];
  if (page + 1 > s_nchrome) s_nchrome = page + 1;

  const bool header = title != nullptr;
  if (header) {
    // header:  TITLE ............ 21:34  wifi  [OPEN]
    make_rect(scr, 0, HDR_H - 1, W, 1, COL_AMBER_DIM);
    lv_obj_t* t = make_label(scr, FONT_M, COL_AMBER, title);
    lv_obj_set_pos(t, 6, 3);
  }
  // ไม่มี header -> นาฬิกา/wifi/สถานะย้ายไปอยู่ในแถบล่าง
  const int sy = header ? 3 : H - FTR_H + 1;
  const lv_font_t* sf = header ? FONT_M : FONT_S;

  c.clock = make_label(scr, sf, COL_WHITE, "--:--");
  lv_obj_set_width(c.clock, header ? 46 : 36);
  lv_obj_set_style_text_align(c.clock, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(c.clock, header ? 184 : 150, sy);

  c.wifi = make_label(scr, sf, COL_AMBER, LV_SYMBOL_WIFI);
  lv_obj_set_pos(c.wifi, header ? 237 : 192, sy);

  c.badge = make_label(scr, FONT_S, lv_color_black(), "----");
  set_pill(c.badge, market_color(Market::UNKNOWN));
  lv_obj_set_size(c.badge, header ? 56 : 52, header ? 16 : 14);
  lv_obj_set_style_pad_top(c.badge, header ? 1 : 0, 0);
  lv_obj_set_style_text_align(c.badge, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(c.badge, header ? W - 60 : 210, header ? 3 : H - FTR_H + 1);

  // footer:  UPD 21:34:05     [STALE]        o o o o
  make_rect(scr, 0, H - FTR_H, W, 1, COL_GRID);
  c.upd = make_label(scr, FONT_S, COL_GRAY, "CONNECTING...");
  lv_obj_set_pos(c.upd, 6, H - FTR_H + 1);

  c.stale = make_label(scr, FONT_S, lv_color_black(), "STALE");
  set_pill(c.stale, COL_DOWN);
  lv_obj_set_style_pad_hor(c.stale, 5, 0);
  lv_obj_set_pos(c.stale, header ? 150 : 88, H - FTR_H + 1);
  lv_obj_add_flag(c.stale, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < PAGE_COUNT; ++i) {
    lv_obj_t* d = make_rect(scr, W - 8 - (PAGE_COUNT - i) * 10, H - 10, 6, 6,
                            i == page ? COL_AMBER : lv_color_hex(0x3A3A3A));
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  }
}

void chrome_update(const MarketData* md, const NetStatus& st) {
  char clk[8] = "--:--", upd[40];
  time_t now = time(nullptr);
  bool tv = time_valid();
  struct tm tm;
  if (tv) {
    localtime_r(&now, &tm);
    strftime(clk, sizeof(clk), "%H:%M", &tm);
  }

  bool haveData = md && st.everOk;
  uint32_t ageMs = millis() - st.lastOkMs;
  // ตอนตลาดปิด GitHub Actions อัปเดตแค่ทุก ~5-15 นาที -> ยอมให้ข้อมูลเก่าได้ 30 นาที
  uint32_t dataMaxAge = (md && md->market == Market::CLOSED) ? 1800 : STALE_AFTER_SEC;
  bool stale = haveData && (ageMs > STALE_AFTER_SEC * 1000UL ||
                            (tv && md->ts && (uint32_t)now > md->ts + dataMaxAge));

  if (!haveData) {
    snprintf(upd, sizeof(upd), st.wifi ? "WAITING FOR DATA..." : "CONNECTING WIFI...");
  } else if (tv && md->ts) {
    time_t ts = md->ts;
    localtime_r(&ts, &tm);
    strftime(upd, sizeof(upd), "UPD %H:%M:%S", &tm);
  } else {
    snprintf(upd, sizeof(upd), "UPD %lus AGO", (unsigned long)(ageMs / 1000));
  }

  Market m = md ? md->market : Market::UNKNOWN;
  const char* badge = st.demo ? "DEMO" : market_text(m);
  lv_color_t badgeCol = st.demo ? lv_color_hex(0x3A7BFF) : market_color(m);

  for (int i = 0; i < s_nchrome; ++i) {
    Chrome& c = s_chrome[i];
    if (!c.clock) continue;
    lv_label_set_text(c.clock, clk);
    lv_label_set_text(c.upd, upd);
    lv_label_set_text(c.badge, badge);
    lv_obj_set_style_bg_color(c.badge, badgeCol, 0);
    lv_obj_set_style_text_color(c.wifi, st.wifi ? COL_AMBER : COL_DOWN, 0);
    lv_label_set_text(c.wifi, st.wifi ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
    if (stale) lv_obj_remove_flag(c.stale, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(c.stale, LV_OBJ_FLAG_HIDDEN);
  }
}

}  // namespace ui
