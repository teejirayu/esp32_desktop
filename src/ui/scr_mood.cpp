// หน้า 3: MARKET MOOD — เกจคะแนนอารมณ์ตลาด 0-100 (Fear <-> Greed) จาก fetcher
// คอลัมน์ขวา: VIX, yield curve, 10Y, 2Y, FED, CNN Fear & Greed; ล่าง: เหตุผลที่มีผลมากสุด
#include <math.h>
#include <stdio.h>

#include "screens.h"
#include "ui_common.h"

using namespace ui;

namespace scr_mood {
namespace {

constexpr int CX = 100, CY = BODY_Y + 96;  // จุดศูนย์กลางเกจ
constexpr int R = 82, ARC_W = 12, NEEDLE_LEN = 62;
constexpr int PX = 206;  // คอลัมน์ขวา
constexpr int ROW_H = 24;

lv_obj_t *needle, *scoreVal, *scoreTag, *why;
lv_obj_t *vixVal, *curveVal, *rateVal[3], *fgVal;
lv_point_precise_t pts[2];
int32_t curVal = 50;

float angleFor(float v) {
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  return 135.f + v / 100.f * 270.f;  // 0° = 3 นาฬิกา, ตามเข็ม
}

lv_color_t moodColor(int v) {
  if (v < 25) return COL_DOWN;
  if (v < 45) return COL_ORANGE;
  if (v <= 55) return COL_YELLOW;
  if (v <= 75) return lv_color_hex(0x7ED957);
  return COL_UP;
}

const char* moodLabel(int v) {
  if (v < 25) return "EXTREME FEAR";
  if (v < 45) return "FEAR";
  if (v <= 55) return "NEUTRAL";
  if (v <= 75) return "GREED";
  return "EXTREME GREED";
}

lv_color_t vixColor(float v) {
  if (v < 15) return COL_UP;
  if (v < 20) return COL_YELLOW;
  if (v < 30) return COL_ORANGE;
  return COL_DOWN;
}

void setNeedle(void*, int32_t v) {
  curVal = v;
  float a = angleFor(v) * (float)M_PI / 180.f;
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

lv_obj_t* row(lv_obj_t* scr, int i, const char* name) {
  int y = BODY_Y + 4 + i * ROW_H;
  lv_obj_t* n = make_label(scr, FONT_MONO, COL_AMBER, name);
  lv_obj_set_pos(n, PX, y);
  lv_obj_t* v = make_label(scr, FONT_MONO, COL_WHITE, "--");
  lv_obj_set_width(v, W - 4 - (PX + 50));
  lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(v, PX + 50, y);
  return v;
}

}  // namespace

lv_obj_t* create() {
  lv_obj_t* scr = make_screen();
  chrome_create(scr, "MARKET MOOD", 2);

  // --- เกจ ---
  band(scr, 0, 25, COL_DOWN);
  band(scr, 25, 45, COL_ORANGE);
  band(scr, 45, 55, COL_YELLOW);
  band(scr, 55, 75, lv_color_hex(0x7ED957));
  band(scr, 75, 100, COL_UP);

  for (int v = 0; v <= 100; v += 25) {
    float a = angleFor(v) * (float)M_PI / 180.f;
    int x = CX + (int)lroundf(cosf(a) * (R - ARC_W - 12));
    int y = CY + (int)lroundf(sinf(a) * (R - ARC_W - 12));
    char b[4];
    snprintf(b, sizeof(b), "%d", v);
    lv_obj_t* l = make_label(scr, FONT_S, COL_GRAY, b);
    lv_obj_set_width(l, 28);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, x - 14, y - 7);
  }

  scoreVal = make_label(scr, FONT_XL, COL_WHITE, "--");
  lv_obj_set_width(scoreVal, 110);
  lv_obj_set_style_text_align(scoreVal, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(scoreVal, CX - 55, CY + 16);

  scoreTag = make_label(scr, FONT_S, COL_GRAY, "");
  lv_obj_set_width(scoreTag, 110);
  lv_obj_set_style_text_align(scoreTag, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(scoreTag, CX - 55, CY + 50);

  needle = lv_line_create(scr);
  lv_obj_set_style_line_width(needle, 3, 0);
  lv_obj_set_style_line_color(needle, COL_WHITE, 0);
  lv_obj_set_style_line_rounded(needle, true, 0);
  setNeedle(nullptr, curVal);

  lv_obj_t* cap = make_rect(scr, CX - 7, CY - 7, 14, 14, COL_AMBER);
  lv_obj_set_style_radius(cap, LV_RADIUS_CIRCLE, 0);

  // --- คอลัมน์ขวา ---
  make_rect(scr, PX - 8, BODY_Y + 6, 1, 6 * ROW_H - 6, COL_GRID);
  vixVal = row(scr, 0, "VIX");
  curveVal = row(scr, 1, "CRV");
  rateVal[0] = row(scr, 2, "10Y");
  rateVal[1] = row(scr, 3, "2Y");
  rateVal[2] = row(scr, 4, "FED");
  fgVal = row(scr, 5, "CNN");

  // --- เหตุผล ---
  make_rect(scr, 4, BODY_Y + BODY_H - 21, W - 8, 1, COL_GRID);
  why = make_label(scr, FONT_S, COL_GRAY, "");
  lv_label_set_long_mode(why, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(why, W - 12);
  lv_obj_set_pos(why, 6, BODY_Y + BODY_H - 17);
  return scr;
}

void update(const MarketData& md) {
  char b[20];
  if (md.mood >= 0) {
    lv_color_t c = moodColor(md.mood);
    snprintf(b, sizeof(b), "%d", md.mood);
    lv_label_set_text(scoreVal, b);
    lv_obj_set_style_text_color(scoreVal, c, 0);
    lv_label_set_text(scoreTag, moodLabel(md.mood));
    lv_obj_set_style_text_color(scoreTag, c, 0);
    lv_label_set_text(why, md.moodWhy);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, needle);
    lv_anim_set_exec_cb(&a, setNeedle);
    lv_anim_set_values(&a, curVal, md.mood);
    lv_anim_set_duration(&a, 700);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
  }

  const Quote* vix = md.find("VIX");
  if (vix && !isnan(vix->p)) {
    snprintf(b, sizeof(b), "%.2f", (double)vix->p);
    lv_label_set_text(vixVal, b);
    lv_obj_set_style_text_color(vixVal, vixColor(vix->p), 0);
  }

  if (md.hasCurve) {
    snprintf(b, sizeof(b), "%+.2f", (double)md.curve);
    lv_label_set_text(curveVal, b);
    lv_obj_set_style_text_color(curveVal, md.curve < 0 ? COL_DOWN : COL_UP, 0);  // ติดลบ = inverted
  }

  const char* syms[3] = {"10Y", "2Y", "FED"};
  for (int i = 0; i < 3; ++i) {
    const Quote* q = md.find(syms[i]);
    if (q && !isnan(q->p)) snprintf(b, sizeof(b), "%.2f%%", (double)q->p);
    else snprintf(b, sizeof(b), "--");
    lv_label_set_text(rateVal[i], b);
  }

  if (md.fg >= 0) {
    snprintf(b, sizeof(b), "%d", md.fg);
    lv_label_set_text(fgVal, b);
    lv_obj_set_style_text_color(fgVal, moodColor(md.fg), 0);
  }
}

}  // namespace scr_mood
