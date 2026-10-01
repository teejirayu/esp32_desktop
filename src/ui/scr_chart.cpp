// หน้า 4: กราฟ S&P 5 วัน (บน) + ปฏิทินเหตุการณ์สัปดาห์นี้ เวลาไทย (ล่าง)
#include <ctype.h>
#include <initializer_list>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "screens.h"
#include "ui_common.h"

using namespace ui;

namespace scr_chart {
namespace {

constexpr int EV_ROWS = 4;
constexpr int EV_Y = BODY_Y + 132, EV_ROW_H = 17;

lv_obj_t *title, *price, *chg, *chart, *hiLbl, *loLbl, *noData;
lv_chart_series_t* ser;
struct EvRow {
  lv_obj_t *when, *name, *count;
};
EvRow evRows[EV_ROWS];
lv_obj_t* noEvents;

Event s_ev[MD_MAX_EVENTS];
int s_nev = 0;

void fmtCountdown(char* b, size_t n, int32_t dt) {
  if (dt < -3600) snprintf(b, n, "DONE");
  else if (dt < 0) snprintf(b, n, "LIVE");
  else if (dt < 3600) snprintf(b, n, "%ldm", (long)(dt / 60));
  else if (dt < 86400) snprintf(b, n, "%ldh%02ldm", (long)(dt / 3600), (long)(dt % 3600 / 60));
  else snprintf(b, n, "%ldd%02ldh", (long)(dt / 86400), (long)(dt % 86400 / 3600));
}

void renderEvents() {
  time_t now = time(nullptr);
  bool tv = time_valid();
  int shown = 0;
  for (int i = 0; i < s_nev && shown < EV_ROWS; ++i) {
    const Event& e = s_ev[i];
    int32_t dt = (int32_t)((int64_t)e.at - (int64_t)now);
    if (tv && dt < -12 * 3600) continue;  // ผ่านไปนานแล้ว
    EvRow& r = evRows[shown++];

    char b[24];
    time_t at = e.at;
    struct tm tm;
    localtime_r(&at, &tm);
    strftime(b, sizeof(b), "%a %d %H:%M", &tm);
    for (char* p = b; *p; ++p) *p = toupper((unsigned char)*p);
    lv_label_set_text(r.when, b);
    lv_label_set_text(r.name, e.t);

    if (tv) fmtCountdown(b, sizeof(b), dt);
    else b[0] = 0;
    lv_label_set_text(r.count, b);

    bool done = tv && dt < -3600;
    bool soon = tv && dt >= -3600 && dt < 86400;
    lv_obj_set_style_text_color(r.when, done ? COL_GRID : COL_WHITE, 0);
    lv_obj_set_style_text_color(r.name, done ? COL_GRID : COL_AMBER, 0);
    lv_obj_set_style_text_color(r.count, done ? COL_GRID : (soon ? COL_DOWN : COL_GRAY), 0);
    for (lv_obj_t* o : {r.when, r.name, r.count}) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
  for (int i = shown; i < EV_ROWS; ++i)
    for (lv_obj_t* o : {evRows[i].when, evRows[i].name, evRows[i].count})
      lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  if (shown) lv_obj_add_flag(noEvents, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(noEvents, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

lv_obj_t* create() {
  lv_obj_t* scr = make_screen();
  chrome_create(scr, "CHART / EVENTS", 3);

  // --- หัวกราฟ ---
  title = make_label(scr, FONT_M, COL_AMBER, "S&P 500  5D");
  lv_obj_set_pos(title, 6, BODY_Y + 4);
  price = make_label(scr, FONT_MONO, COL_WHITE, "--");
  lv_obj_set_width(price, 90);
  lv_obj_set_style_text_align(price, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(price, 146, BODY_Y + 4);
  chg = make_label(scr, FONT_MONO, COL_FLAT, "--");
  lv_obj_set_width(chg, 72);
  lv_obj_set_style_text_align(chg, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(chg, 242, BODY_Y + 4);

  // --- กราฟ ---
  chart = lv_chart_create(scr);
  lv_obj_remove_flag(chart, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(chart, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(chart, 4, BODY_Y + 24);
  lv_obj_set_size(chart, W - 8, 86);
  lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(chart, COL_GRID, 0);
  lv_obj_set_style_border_width(chart, 1, 0);
  lv_obj_set_style_radius(chart, 0, 0);
  lv_obj_set_style_pad_all(chart, 4, 0);
  lv_obj_set_style_line_color(chart, COL_GRID, LV_PART_MAIN);  // เส้น grid
  lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
  lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);       // ไม่วาดจุด
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_div_line_count(chart, 4, 6);
  lv_chart_set_point_count(chart, MD_MAX_SPARK_PTS);
  ser = lv_chart_add_series(chart, COL_UP, LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(chart, ser, LV_CHART_POINT_NONE);

  hiLbl = make_label(scr, FONT_S, COL_GRAY, "");
  lv_obj_set_width(hiLbl, 80);
  lv_obj_set_style_text_align(hiLbl, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(hiLbl, W - 90, BODY_Y + 27);
  loLbl = make_label(scr, FONT_S, COL_GRAY, "");
  lv_obj_set_width(loLbl, 80);
  lv_obj_set_style_text_align(loLbl, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(loLbl, W - 90, BODY_Y + 92);

  noData = make_label(scr, FONT_S, COL_GRAY, "NO CHART DATA");
  lv_obj_set_pos(noData, 110, BODY_Y + 60);

  // --- ปฏิทิน ---
  lv_obj_t* eh = make_label(scr, FONT_S, COL_AMBER, "EVENTS  NEXT 7 DAYS  (ICT)");
  lv_obj_set_pos(eh, 6, EV_Y - 20 + 2);
  make_rect(scr, 4, EV_Y - 3, W - 8, 1, COL_GRID);

  for (int i = 0; i < EV_ROWS; ++i) {
    int y = EV_Y + i * EV_ROW_H;
    EvRow& r = evRows[i];
    r.when = make_label(scr, FONT_MONO, COL_WHITE, "");
    lv_obj_set_pos(r.when, 6, y);
    r.name = make_label(scr, FONT_MONO, COL_AMBER, "");
    lv_obj_set_pos(r.name, 116, y);
    r.count = make_label(scr, FONT_MONO, COL_GRAY, "");
    lv_obj_set_width(r.count, 90);
    lv_obj_set_style_text_align(r.count, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(r.count, W - 96, y);
  }
  noEvents = make_label(scr, FONT_S, COL_GRAY, "NO MAJOR EVENTS");
  lv_obj_set_pos(noEvents, 6, EV_Y + 2);
  return scr;
}

void update(const MarketData& md) {
  const Spark* sp = md.futuresMode() ? md.findSpark("ES") : nullptr;
  if (!sp) sp = md.findSpark("SPX");

  if (sp && sp->n >= 2) {
    bool isFut = !strcmp(sp->s, "ES");
    lv_label_set_text(title, isFut ? "S&P FUT  5D" : "S&P 500  5D");
    const Quote* q = md.find(sp->s);
    char b[20];
    fmt_price(b, sizeof(b), q ? q->p : NAN);
    lv_label_set_text(price, b);
    fmt_chg(b, sizeof(b), q ? q->c : NAN);
    lv_label_set_text(chg, b);
    lv_obj_set_style_text_color(chg, chg_color(q ? q->c : NAN), 0);

    float lo = sp->v[0], hi = sp->v[0];
    for (int i = 1; i < sp->n; ++i) {
      if (sp->v[i] < lo) lo = sp->v[i];
      if (sp->v[i] > hi) hi = sp->v[i];
    }
    float pad = (hi - lo) * 0.1f + 0.5f;
    // lv_chart เก็บ int -> คูณ 10 เพื่อความละเอียด
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, (int32_t)floorf((lo - pad) * 10),
                       (int32_t)ceilf((hi + pad) * 10));
    lv_chart_set_point_count(chart, sp->n);
    for (int i = 0; i < sp->n; ++i)
      lv_chart_set_value_by_id(chart, ser, i, (int32_t)lroundf(sp->v[i] * 10));
    bool upTrend = sp->v[sp->n - 1] >= sp->v[0];
    lv_chart_set_series_color(chart, ser, upTrend ? COL_UP : COL_DOWN);
    lv_chart_refresh(chart);

    fmt_price(b, sizeof(b), hi);
    lv_label_set_text_fmt(hiLbl, "H %s", b);
    fmt_price(b, sizeof(b), lo);
    lv_label_set_text_fmt(loLbl, "L %s", b);
    lv_obj_add_flag(noData, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_remove_flag(noData, LV_OBJ_FLAG_HIDDEN);
  }

  s_nev = md.nev;
  memcpy(s_ev, md.ev, sizeof(Event) * md.nev);
  renderEvents();
}

void tick() { renderEvents(); }

}  // namespace scr_chart
