#pragma once
#include "../market_data.h"

// จัดการหน้าจอทั้งหมด — เรียกจาก loop() (core 1) เท่านั้น
namespace ui {
void init();
void update(const MarketData& md);           // ข้อมูลใหม่จาก net
void tick(const NetStatus& st, bool dark);   // ทุก 1 วินาที: นาฬิกา, STALE, โหมดมืด
bool inClockMode();
}
