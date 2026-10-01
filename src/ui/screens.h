#pragma once
#include <lvgl.h>

#include "../market_data.h"

// แต่ละหน้าอยู่ในไฟล์ของตัวเอง: create() สร้าง screen, update() เติมข้อมูล
namespace scr_overview {
lv_obj_t* create();
void update(const MarketData& md);
}
namespace scr_mood {  // อารมณ์ตลาด (Fear/Greed) + VIX + อัตราดอกเบี้ย
lv_obj_t* create();
void update(const MarketData& md);
}
namespace scr_bubbles {  // หน้าแรก: bubble หุ้นใหญ่ ~40 ตัว
lv_obj_t* create();
void update(const MarketData& md);
}
namespace scr_chart {
lv_obj_t* create();
void update(const MarketData& md);
void tick();  // นับถอยหลังเหตุการณ์
}
namespace scr_clock {
lv_obj_t* create();
void update(const MarketData& md);
void tick();
}
