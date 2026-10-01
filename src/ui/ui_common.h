#pragma once
#include <lvgl.h>
#include <stddef.h>

#include "../market_data.h"

// JetBrains Mono Bold (src/fonts, สร้างด้วย lv_font_conv) — ไฟล์ .c จึงต้อง extern "C"
extern "C" {
LV_FONT_DECLARE(mono_14)
LV_FONT_DECLARE(mono_16)
}

namespace ui {

constexpr int W = 320, H = 240;
constexpr int HDR_H = 22, FTR_H = 16;
constexpr int BODY_Y = HDR_H;
constexpr int BODY_H = H - HDR_H - FTR_H;
constexpr int PAGE_COUNT = 4;

// สีแบบ terminal
#define COL_BG lv_color_hex(0x000000)
#define COL_AMBER lv_color_hex(0xFFA028)
#define COL_AMBER_DIM lv_color_hex(0x7A4A10)
#define COL_UP lv_color_hex(0x00D26A)
#define COL_DOWN lv_color_hex(0xFF3B30)
#define COL_FLAT lv_color_hex(0x8A8A8A)
#define COL_WHITE lv_color_hex(0xF0F0F0)
#define COL_GRAY lv_color_hex(0x8A8A8A)
#define COL_GRID lv_color_hex(0x262626)
#define COL_ROW_ALT lv_color_hex(0x0E0E0E)
#define COL_YELLOW lv_color_hex(0xFFD60A)
#define COL_ORANGE lv_color_hex(0xFF8C00)

#define FONT_MONO (&mono_16)
#define FONT_MONO_S (&mono_14)
#define FONT_XS (&lv_font_montserrat_10)
#define FONT_S (&lv_font_montserrat_12)
#define FONT_M (&lv_font_montserrat_14)
#define FONT_16 (&lv_font_montserrat_16)
#define FONT_L (&lv_font_montserrat_20)
#define FONT_XL (&lv_font_montserrat_28)
#define FONT_XXL (&lv_font_montserrat_48)

lv_obj_t* make_screen();
lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* txt = "");
lv_obj_t* make_rect(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t color);
void set_pill(lv_obj_t* label, lv_color_t bg);  // label -> ป้ายพื้นทึบมุมมน

lv_color_t chg_color(float c);
void fmt_price(char* out, size_t n, float p);  // 5,812.34 / 4.231 / --
void fmt_chg(char* out, size_t n, float c);    // +0.45%
void fmt_bps(char* out, size_t n, float p, float c);  // yield: +3.2bp

bool time_valid();
const char* market_text(Market m);
lv_color_t market_color(Market m);

// header/footer ที่ทุกหน้าใช้ร่วมกัน
// title = nullptr -> ไม่มีแถบบน (สถานะไปอยู่แถบล่าง)
void chrome_create(lv_obj_t* scr, const char* title, int page);
void chrome_update(const MarketData* md, const NetStatus& st);

}  // namespace ui
