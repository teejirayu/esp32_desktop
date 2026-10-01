// หน้าแรก: bubble หุ้นสหรัฐฯ 40 วง เต็มจอ (ไม่มีแถบบน)
// ช่อง 0..BUBBLE_CORE-1 = หุ้นท็อปตาม market cap, ช่องที่เหลือ = หุ้นร้อนแรงของวัน (ชื่อสีอำพัน)
// ตำแหน่ง/ขนาดช่องมาจาก tools/pack_bubbles.py -> bubble_layout.h; ชื่อหุ้นมาจากข้อมูล (key "b")
// กลางวงมืด ขอบเรืองเขียว/แดง ยิ่งขยับแรงยิ่งสว่าง
#include <math.h>
#include <stdio.h>

#include "../bubble_layout.h"
#include "screens.h"
#include "ui_common.h"

using namespace ui;

namespace scr_bubbles {
namespace {

constexpr float HEAT_FULL = 3.0f;  // ±3% = สว่างสุด

struct Bubble {
  lv_obj_t *obj, *sym, *chg;
  lv_grad_dsc_t grad;  // style เก็บ pointer -> ต้องอยู่ถาวร
};
Bubble bubbles[BUBBLE_COUNT];

void paint(Bubble& b, float c) {
  lv_color_t full;
  float t = 0;
  if (isnan(c) || fabsf(c) < 0.005f) {
    full = lv_color_hex(0x808080);
  } else {
    // สีบริสุทธิ์ (ไม่ผสมช่องสีที่สาม) เพื่อให้จอ TN ยังดูสด
    full = c > 0 ? lv_color_hex(0x10FF30) : lv_color_hex(0xFF1010);
    t = fabsf(c) / HEAT_FULL;
    if (t > 1) t = 1;
  }
  const lv_color_t black = lv_color_hex(0x000000);
  lv_color_t center = lv_color_mix(full, black, (uint8_t)(22 + t * 50));
  lv_color_t edge = lv_color_mix(full, black, (uint8_t)(170 + t * 85));
  lv_color_t colors[4] = {center, center, edge, edge};
  const uint8_t fracs[4] = {0, 130, 240, 255};
  lv_gradient_init_stops(&b.grad, colors, nullptr, fracs, 4);
  lv_obj_set_style_bg_grad(b.obj, &b.grad, 0);  // set ซ้ำเพื่อ invalidate
}

}  // namespace

lv_obj_t* create() {
  lv_obj_t* scr = make_screen();
  chrome_create(scr, nullptr, 0);

  for (int i = 0; i < BUBBLE_COUNT; ++i) {
    const BubbleSlot& d = BUBBLES[i];
    Bubble& b = bubbles[i];
    b.obj = make_rect(scr, d.cx - d.r, d.cy - d.r, d.r * 2 + 1, d.r * 2 + 1, COL_BG);
    lv_obj_set_style_radius(b.obj, LV_RADIUS_CIRCLE, 0);
    lv_grad_radial_init(&b.grad, LV_GRAD_CENTER, LV_GRAD_CENTER, LV_GRAD_RIGHT, LV_GRAD_CENTER,
                        LV_GRAD_EXTEND_PAD);

    // ขนาดตัวอักษรตามขนาดวง
    const lv_font_t *fs, *fc;
    int dy;
    if (d.r >= 35) fs = FONT_16, fc = FONT_L, dy = 10;
    else if (d.r >= 25) fs = FONT_M, fc = FONT_M, dy = 8;
    else if (d.r >= 17) fs = FONT_S, fc = FONT_XS, dy = 6;
    else fs = FONT_XS, fc = FONT_XS, dy = 5;

    b.sym = make_label(b.obj, fs, COL_WHITE, "");
    lv_obj_align(b.sym, LV_ALIGN_CENTER, 0, -dy);
    b.chg = make_label(b.obj, fc, COL_WHITE, "");
    lv_obj_align(b.chg, LV_ALIGN_CENTER, 0, dy);
    paint(b, NAN);
  }
  return scr;
}

void update(const MarketData& md) {
  // แยก core / hot ตามลำดับที่ได้รับ แล้วเติมลงช่องของแต่ละกลุ่ม
  int core = 0, hot = BUBBLE_CORE;
  bool used[BUBBLE_COUNT] = {};
  for (int i = 0; i < md.nb; ++i) {
    const BubbleQuote& q = md.b[i];
    int slot = q.hot ? hot++ : core++;
    if ((q.hot && slot >= BUBBLE_COUNT) || (!q.hot && slot >= BUBBLE_CORE)) continue;
    used[slot] = true;
    Bubble& b = bubbles[slot];
    char buf[12];
    lv_label_set_text(b.sym, q.s);
    lv_obj_set_style_text_color(b.sym, q.hot ? COL_AMBER : COL_WHITE, 0);
    if (isnan(q.c)) snprintf(buf, sizeof(buf), "--");
    else if (BUBBLES[slot].r < 17) snprintf(buf, sizeof(buf), "%+.1f", (double)q.c);  // วงเล็ก: ตัด %
    else snprintf(buf, sizeof(buf), "%+.2f%%", (double)q.c);
    lv_label_set_text(b.chg, buf);
    paint(b, q.c);
    lv_obj_remove_flag(b.obj, LV_OBJ_FLAG_HIDDEN);
  }
  // ช่องที่ไม่มีข้อมูล (เช่น ยังไม่มีคีย์ Alpaca -> ไม่มีหุ้นร้อนแรง) ซ่อนไว้
  for (int i = 0; i < BUBBLE_COUNT; ++i)
    if (!used[i]) lv_obj_add_flag(bubbles[i].obj, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace scr_bubbles
