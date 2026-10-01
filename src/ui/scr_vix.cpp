// หน้า 2: VIX gauge (ซ้าย) + yield curve / อัตราดอกเบี้ย (ขวา)
#include <math.h>
#include <stdio.h>

#include "screens.h"
#include "ui_common.h"

using namespace ui;

namespace scr_vix {
namespace {

constexpr int CX = 102, CY = BODY_Y + 104;  // จุดศูนย์กลางเกจ
constexpr int R = 88, ARC_W = 12, NEEDLE_LEN = 68;
constexpr float VMIN = 10, VMAX = 40;
constexpr int PX = 206;  // คอลัมน์ขวา

lv_obj_t *needle, *vixVal, *vixChg;
lv_obj_t *curveVal, *curveTag, *rateVal[3];
lv_point_precise_t pts[2];
int32_t curX100 = (int32_t)(VMIN * 100);

float angleFor(float v) {
  if (v < VMIN) v = VMIN;
  if (v > VMAX) v = VMAX;
  return 135.f + (v - VMIN) / (VMAX - VMIN) * 270.f;  // 0° = 3 นาฬิกา, ตามเข็ม
}

lv_color_t zoneColor(float v) {
  if (v < 15) return COL_UP;
  if (v < 20) return COL_YELLOW;
  if (v < 30) return COL_ORANGE;
  return COL_DOWN;
}

void setNeedle(void*, int32_t vx100) {
  curX100 = vx100;
  float a = angleFor(vx100 / 100.f) * (float)M_PI / 180.f;
  pts[0].x = CX;
  pts[0].y = CY;
  pts[1].x = CX + (int32_t)lroundf(cosf(a) * NEEDLE_LEN);
  pts[1].y = CY + (int32_t)lroundf(sinf(a) * NEEDLE_LEN);
  lv_line_set_points(needle, pts, 2);
}

void band(lv_obj_t* scr, float v0, float v1, lv_color_t c) {
  lv_obj_t* arc = lv_arc_create(scr);
  lv_obj_remove_style_all(arc);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(arc, R * 2, R * 2);
  lv_obj_set_pos(arc, CX - R, CY - R);
  int a0 = (int)angleFor(v0) + 1, a1 = (int)angleFor(v1) - 1;  // เว้นช่องเล็กระหว่างโซน
  lv_arc_set_bg_angles(arc, a0 % 360, a1 % 360);
  lv_obj_set_style_arc_width(arc, ARC_W, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, c, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
}

lv_obj_t* rateRow(lv_obj_t* scr, int y, const char* name) {
  lv_obj_t* n = make_label(scr, FONT_MONO, COL_AMBER, name);
  lv_obj_set_pos(n, PX, y);
  lv_obj_t* v = make_label(scr, FONT_MONO, COL_WHITE, "--");
  lv_obj_set_width(v, W - 6 - (PX + 34));
  lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(v, PX + 34, y);
  return v;
}

}  // namespace

lv_obj_t* create() {
  lv_obj_t* scr = make_screen();
  chrome_create(scr, "VIX / RATES", 2);

  // --- เกจ ---
  band(scr, 10, 15, COL_UP);
  band(scr, 15, 20, COL_YELLOW);
  band(scr, 20, 30, COL_ORANGE);
  band(scr, 30, 40, COL_DOWN);

  for (int v = 10; v <= 40; v += 5) {
    float a = angleFor(v) * (float)M_PI / 180.f;
    int x = CX + (int)lroundf(cosf(a) * (R - ARC_W - 12));
    int y = CY + (int)lroundf(sinf(a) * (R - ARC_W - 12));
    char b[4];
    snprintf(b, sizeof(b), "%d", v);
    lv_obj_t* l = make_label(scr, FONT_S, COL_GRAY, b);
    lv_obj_set_width(l, 24);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, x - 12, y - 7);
  }

  lv_obj_t* t = make_label(scr, FONT_M, COL_AMBER, "VIX");
  lv_obj_set_width(t, 60);
  lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(t, CX - 30, CY - 36);

  vixVal = make_label(scr, FONT_XL, COL_WHITE, "--");
  lv_obj_set_width(vixVal, 110);
  lv_obj_set_style_text_align(vixVal, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(vixVal, CX - 55, CY + 22);

  vixChg = make_label(scr, FONT_M, COL_FLAT, "--");
  lv_obj_set_width(vixChg, 110);
  lv_obj_set_style_text_align(vixChg, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(vixChg, CX - 55, CY + 56);

  needle = lv_line_create(scr);
  lv_obj_set_style_line_width(needle, 3, 0);
  lv_obj_set_style_line_color(needle, COL_WHITE, 0);
  lv_obj_set_style_line_rounded(needle, true, 0);
  setNeedle(nullptr, curX100);

  lv_obj_t* cap = make_rect(scr, CX - 7, CY - 7, 14, 14, COL_AMBER);
  lv_obj_set_style_radius(cap, LV_RADIUS_CIRCLE, 0);

  // --- คอลัมน์ขวา: yield curve ---
  make_rect(scr, PX - 8, BODY_Y + 6, 1, BODY_H - 12, COL_GRID);

  lv_obj_t* h = make_label(scr, FONT_S, COL_GRAY, "YIELD CURVE\n10Y - 2Y");
  lv_obj_set_pos(h, PX, BODY_Y + 6);

  curveVal = make_label(scr, FONT_XL, COL_WHITE, "--");
  lv_obj_set_pos(curveVal, PX, BODY_Y + 36);

  curveTag = make_label(scr, FONT_S, lv_color_black(), "----");
  set_pill(curveTag, COL_FLAT);
  lv_obj_set_style_pad_hor(curveTag, 6, 0);
  lv_obj_set_style_pad_ver(curveTag, 1, 0);
  lv_obj_set_pos(curveTag, PX, BODY_Y + 72);

  make_rect(scr, PX, BODY_Y + 100, W - 6 - PX, 1, COL_GRID);
  rateVal[0] = rateRow(scr, BODY_Y + 110, "10Y");
  rateVal[1] = rateRow(scr, BODY_Y + 134, "2Y");
  rateVal[2] = rateRow(scr, BODY_Y + 158, "FED");
  return scr;
}

void update(const MarketData& md) {
  char b[20];
  const Quote* vix = md.find("VIX");
  if (vix && !isnan(vix->p)) {
    snprintf(b, sizeof(b), "%.2f", (double)vix->p);
    lv_label_set_text(vixVal, b);
    lv_obj_set_style_text_color(vixVal, zoneColor(vix->p), 0);
    fmt_chg(b, sizeof(b), vix->c);
    lv_label_set_text(vixChg, b);
    // VIX ขึ้น = ตลาดกลัว -> สีแดง
    lv_obj_set_style_text_color(vixChg, chg_color(-vix->c), 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, needle);
    lv_anim_set_exec_cb(&a, setNeedle);
    lv_anim_set_values(&a, curX100, (int32_t)lroundf(vix->p * 100));
    lv_anim_set_duration(&a, 700);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
  }

  if (md.hasCurve) {
    snprintf(b, sizeof(b), "%+.2f%%", (double)md.curve);
    lv_label_set_text(curveVal, b);
    bool inv = md.curve < 0;
    lv_obj_set_style_text_color(curveVal, inv ? COL_DOWN : COL_UP, 0);
    lv_label_set_text(curveTag, inv ? "INVERTED" : "NORMAL");
    set_pill(curveTag, inv ? COL_DOWN : COL_UP);
  }

  const char* syms[3] = {"10Y", "2Y", "FED"};
  for (int i = 0; i < 3; ++i) {
    const Quote* q = md.find(syms[i]);
    if (q && !isnan(q->p)) snprintf(b, sizeof(b), "%.2f%%", (double)q->p);
    else snprintf(b, sizeof(b), "--");
    lv_label_set_text(rateVal[i], b);
  }
}

}  // namespace scr_vix
