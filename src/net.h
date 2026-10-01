#pragma once
#include "market_data.h"

// WiFi + fetch market.json (GitHub) ทำงานใน FreeRTOS task บน core 0
// UI (core 1) ดึงข้อมูลผ่าน take() ซึ่งป้องกันด้วย mutex
namespace net {
void begin();
// คืน true ถ้ามีข้อมูลใหม่ตั้งแต่เรียกครั้งก่อน (คัดลอกลง out); st ถูกเติมทุกครั้ง
bool take(MarketData& out, NetStatus& st);
}
