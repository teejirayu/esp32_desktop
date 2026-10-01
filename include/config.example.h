// คัดลอกไฟล์นี้เป็น config.h แล้วแก้ค่า — config.h ห้าม commit
#pragma once

// ---- WiFi ----
// ลองตามลำดับ ต่อไม่ได้ 20 วินาที -> ตัวถัดไป (ESP32 รองรับแค่ 2.4GHz)
#define WIFI_NETWORKS {{"your-ssid", "your-password"}, {"second-ssid", "second-password"}}

// ---- แหล่งข้อมูล: market.json บน branch `data` (สร้างโดย GitHub Actions) ----
// ยังมีคำว่า YOUR_ อยู่ = โหมด DEMO (สุ่มข้อมูลเพื่อทดสอบหน้าจอ)
#define DATA_URL "https://api.github.com/repos/YOUR_USER/esp32_desktop/contents/market.json?ref=data"
// ไม่ใส่ token: GitHub API ให้ 60 ครั้ง/ชม. -> ต้องดึงทุก >= 75 วินาที
// ใส่ fine-grained token (อ่าน public repo อย่างเดียว): 5000 ครั้ง/ชม. -> ลดเหลือ 60000 ได้
#define GITHUB_TOKEN ""
#define FETCH_INTERVAL_MS 75000
#define STALE_AFTER_SEC 300

// เวลาท้องถิ่นสำหรับนาฬิกา (POSIX TZ) — สถานะตลาดคำนวณที่ fetcher
#define TZ_INFO "<+07>-7"

// ---- จอ / ทัช ----
#define DISPLAY_ROTATION 1   // 1 หรือ 3 (แนวนอน)
#define TFT_INVERT 0         // ST7789_DRIVER กลับสีให้เองแล้ว; ใช้ 1 เฉพาะ ILI9341 ที่พื้นขาว
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3700
#define TOUCH_Y_MIN 240
#define TOUCH_Y_MAX 3800

// ---- Alert ----
#define ALERT_SPX_PCT 1.0f   // |SPX %chg| เกินนี้ -> LED กระพริบ
#define ALERT_VIX_LEVEL 25.0f
#define ALERT_SOUND 1        // 0 = ปิดเสียง

// ---- แสงอัตโนมัติ (LDR pin 34) ----
// ดูค่า "[hw] ldr=" ใน Serial แล้วปรับ: ค่ามาก = มืด
#define LDR_AUTO 1
#define LDR_BRIGHT_RAW 20    // ค่าในห้องสว่าง -> backlight สูงสุด
#define LDR_DARK_RAW 300     // เกินนี้ = มืด -> โหมดนาฬิกาหรี่แสง
#define BL_MIN 6             // 0..255
#define BL_MAX 255
