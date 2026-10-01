// LVGL 9.2 config — ใส่เฉพาะค่าที่ต่างจาก default (ที่เหลือมาจาก lv_conf_internal.h)
// ไฟล์นี้ต้องมีแต่ preprocessor เท่านั้น (ถูก include จากไฟล์ assembly ด้วย)
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// ใช้ heap ของ ESP32 แทน pool คงที่ — UI สร้างครั้งเดียวตอนบูต
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_USE_DRAW_SW_COMPLEX_GRADIENTS 1  // radial gradient สำหรับ bubble หน้า Mag 7
#define LV_GRADIENT_MAX_STOPS 4

#define LV_USE_OS LV_OS_NONE
#define LV_DEF_REFR_PERIOD 33
#define LV_DPI_DEF 130

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

// ฟอนต์
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
// ราคาใช้ JetBrains Mono (src/fonts/mono_*.c) — unscii_16 กว้าง 16px/ตัว ล้นช่อง
#define LV_FONT_DEFAULT &lv_font_montserrat_14

// ไม่ใช้ theme/widget ที่ไม่จำเป็น
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_CALENDAR 0
#define LV_USE_KEYBOARD 0
#define LV_USE_TEXTAREA 0
#define LV_USE_SPINBOX 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0
#define LV_USE_MSGBOX 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0

#endif
