// หน้า 1: Overview — 8 แถว สไตล์ terminal
//  SPX  S&P 500        5,812.34 [+0.45%]
#include <math.h>

#include "screens.h"
#include "ui_common.h"

using namespace ui;

namespace scr_overview {
namespace {

struct RowDef {
  const char* sym;
  const char* desc;
};
const RowDef CASH[8] = {
    {"SPX", "S&P 500"},     {"NDX", "NASDAQ 100"}, {"DJI", "DOW 30"},   {"10Y", "US 10Y YLD"},
    {"VIX", "VOLATILITY"},  {"DXY", "DOLLAR IDX"}, {"GOLD", "GOLD FUT"}, {"WTI", "WTI CRUDE"},
};
const RowDef FUT[2] = {{"ES", "S&P FUTURES"}, {"NQ", "NDX FUTURES"}};

struct Row {
  lv_obj_t *name, *desc, *price, *chg;
};
Row rows[8];
constexpr int ROW_H = 25;

}  // namespace

lv_obj_t* create() {
  lv_obj_t* scr = make_screen();
  chrome_create(scr, "US MARKETS", 1);

  for (int i = 0; i < 8; ++i) {
    int y = BODY_Y + 1 + i * ROW_H;
    if (i % 2) make_rect(scr, 0, y, W, ROW_H, COL_ROW_ALT);
    Row& r = rows[i];

    r.name = make_label(scr, FONT_MONO, COL_AMBER, CASH[i].sym);
    lv_obj_set_pos(r.name, 6, y + 5);

    r.desc = make_label(scr, FONT_S, COL_GRAY, CASH[i].desc);
    lv_obj_set_pos(r.desc, 48, y + 6);

    r.price = make_label(scr, FONT_MONO, COL_WHITE, "--");
    lv_obj_set_width(r.price, 100);
    lv_obj_set_style_text_align(r.price, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(r.price, 134, y + 5);

    r.chg = make_label(scr, FONT_MONO, lv_color_black(), "--");
    set_pill(r.chg, COL_FLAT);
    lv_obj_set_size(r.chg, 72, 20);
    lv_obj_set_style_pad_top(r.chg, 2, 0);
    lv_obj_set_style_text_align(r.chg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(r.chg, 242, y + 2);
  }
  return scr;
}

void update(const MarketData& md) {
  const bool fut = md.futuresMode();
  char buf[20];
  for (int i = 0; i < 8; ++i) {
    RowDef d = CASH[i];
    if (fut && i < 2 && md.find(FUT[i].sym)) d = FUT[i];
    Row& r = rows[i];
    lv_label_set_text(r.name, d.sym);
    lv_label_set_text(r.desc, d.desc);

    const Quote* q = md.find(d.sym);
    float p = q ? q->p : NAN, c = q ? q->c : NAN;
    fmt_price(buf, sizeof(buf), p);
    lv_label_set_text(r.price, buf);

    // yield แสดงเป็น basis point
    if (i == 3) fmt_bps(buf, sizeof(buf), p, c);
    else fmt_chg(buf, sizeof(buf), c);
    lv_label_set_text(r.chg, buf);
    lv_obj_set_style_bg_color(r.chg, chg_color(c), 0);
  }
}

}  // namespace scr_overview
