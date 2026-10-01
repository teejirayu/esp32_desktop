// โหมดนาฬิกาหรี่แสง (ห้องมืด) — แตะเพื่อกลับหน้าปกติชั่วคราว
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "screens.h"
#include "ui_common.h"

using namespace ui;

namespace scr_clock {
namespace {
lv_obj_t *timeLbl, *dateLbl, *mktLbl;
}

lv_obj_t* create() {
  lv_obj_t* scr = make_screen();
  timeLbl = make_label(scr, FONT_XXL, lv_color_hex(0xB06A14), "--:--");
  lv_obj_align(timeLbl, LV_ALIGN_CENTER, 0, -24);
  dateLbl = make_label(scr, FONT_16, lv_color_hex(0x5A5A5A), "");
  lv_obj_align(dateLbl, LV_ALIGN_CENTER, 0, 20);
  mktLbl = make_label(scr, FONT_M, lv_color_hex(0x5A5A5A), "");
  lv_obj_align(mktLbl, LV_ALIGN_BOTTOM_MID, 0, -16);
  return scr;
}

void update(const MarketData& md) {
  const Quote* q = md.headline();
  if (!q) return;
  char p[20], c[12];
  fmt_price(p, sizeof(p), q->p);
  fmt_chg(c, sizeof(c), q->c);
  lv_label_set_text_fmt(mktLbl, "%s %s  %s   %s", q->s, p, c, market_text(md.market));
  // สีเข้มลงเพื่อไม่แสบตาในที่มืด
  lv_obj_set_style_text_color(mktLbl, lv_color_mix(chg_color(q->c), lv_color_black(), 140), 0);
}

void tick() {
  if (!time_valid()) return;
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  char b[24];
  strftime(b, sizeof(b), "%H:%M", &tm);
  lv_label_set_text(timeLbl, b);
  strftime(b, sizeof(b), "%a %d %b %Y", &tm);
  for (char* p = b; *p; ++p) *p = toupper((unsigned char)*p);
  lv_label_set_text(dateLbl, b);
}

}  // namespace scr_clock
