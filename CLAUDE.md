# CLAUDE.md — US Market Desk Gauge (ESP32 CYD)

## ภาพรวมโปรเจกต์
อุปกรณ์ตั้งโต๊ะทำงาน ใช้บอร์ด **ESP32 Cheap Yellow Display (CYD)** แสดงข้อมูลตลาดหุ้นสหรัฐฯ แบบเรียลไทม์ (อัปเดตทุก ~60 วินาที) สไตล์ Bloomberg Terminal + เกจเข็ม ผู้ใช้อยู่ประเทศไทย (Asia/Bangkok, UTC+7)

เฟสแรก: **หุ้นอเมริกาเท่านั้น** (คริปโต/ทองไทย/อากาศ ไว้เฟสหลัง)

## ฮาร์ดแวร์
- บอร์ด: ESP32-2432S028R (CYD) — ESP32-WROOM-32, 4MB flash, **ไม่มี PSRAM** (~320KB RAM)
- จอ: 2.8" 320×240, ไดรเวอร์ ILI9341 (บางล็อตที่มี USB 2 ช่องเป็น ST7789 → ต้องเปิด inversion) — ถ้าสีเพี้ยนให้สงสัยเรื่องนี้ก่อน
- ทัช: XPT2046 (resistive) อยู่บน SPI คนละบัสกับจอ

### Pinout
| ส่วน | Pin |
|---|---|
| TFT (HSPI) | MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST -1, Backlight 21 |
| Touch XPT2046 (VSPI) | CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36 |
| RGB LED (active LOW) | R 4, G 16, B 17 |
| LDR (เซนเซอร์แสง) | 34 (ADC) |
| Speaker | 26 |
| SD card | CS 5, SCK 18, MISO 19, MOSI 23 |

อ้างอิง: https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display

## สถาปัตยกรรม
```
[Yahoo / Alpaca / FRED] --> [GitHub Actions: fetcher/fetch.py] --market.json บน branch `data`--> [GitHub API] --> [ESP32 CYD]
```
- **ESP32 ห้ามเรียก API ภายนอกหลายเจ้าโดยตรง** — RAM ไม่พอสำหรับ HTTPS หลาย session + LVGL
- Fetcher ดึงทุกแหล่ง, คำนวณ % change, สถานะตลาด, เลือกหุ้นร้อนแรง แล้วเขียน JSON เล็ก (< 4KB) force-push ไป branch `data`
- คีย์ทั้งหมดอยู่ใน GitHub Actions Secrets (`ALPACA_KEY_ID`, `ALPACA_SECRET`, `FRED_API_KEY`) — repo ต้องเป็น public
- cron ถี่สุดทุก 5 นาที → แต่ละรอบวนดึงทุก 60 วินาที ~4.5 นาที (เฉพาะ PRE/OPEN/POST)
- ESP32 อ่านผ่าน `api.github.com/.../contents/market.json?ref=data` (ไม่ใช้ raw.githubusercontent เพราะแคช 5 นาที); ไม่มี token = 60 req/ชม. → ดึงทุก 75 วินาที
- เปลี่ยนแหล่งข้อมูล = แก้ที่ fetcher เท่านั้น ไม่ต้องแฟลชบอร์ดใหม่

## โครงสร้างโฟลเดอร์
```
(root)           PlatformIO project (Arduino framework, env: cyd)
  /src
    main.cpp
    market_data.h    struct MarketData / NetStatus
    net.cpp/.h       WiFi + HTTP fetch + JSON parse (task core 0) + โหมด DEMO
    display.cpp/.h   TFT_eSPI + XPT2046 -> LVGL
    hw.cpp/.h        LED, LDR, speaker, backlight
    ui/              ui.cpp (นำทาง/โหมดนาฬิกา), ui_common (สี/ฟอนต์/header-footer), scr_*.cpp หน้าละไฟล์
  /include
    config.h         (gitignore) WiFi SSID/PASS, DATA_URL, GITHUB_TOKEN
    config.example.h
    lv_conf.h
  platformio.ini
  bubble_layout.h    (src/, generated) ตำแหน่งช่อง bubble
/fetcher
  fetch.py           ตัวดึงข้อมูล (stdlib Python) — รันเองได้: python fetcher/fetch.py
/.github/workflows
  market-data.yml    cron ทุก 5 นาที
/tools
  pack_bubbles.py    จัดวางช่อง bubble -> src/bubble_layout.h + docs/bubbles_preview.png
/docs
```

## Tech stack
- **PlatformIO + Arduino framework** (board: `esp32dev`)
- **LVGL 9.x** สำหรับ UI, **TFT_eSPI** เป็น display driver (ตั้งค่าผ่าน `build_flags` ใน platformio.ini ไม่แก้ไฟล์ใน library)
- **XPT2046_Touchscreen** สำหรับทัช
- **ArduinoJson 7** สำหรับ parse
- LVGL buffer: ใช้ partial buffer ขนาด 320×24 px เพื่อประหยัด RAM
- ฟอนต์: Montserrat ในตัว LVGL สำหรับตัวเลข/อังกฤษ; ถ้าต้องการไทยให้แปลงด้วย lv_font_conv เฉพาะตัวอักษรที่ใช้

## ข้อมูลที่แสดง (Yahoo symbols)
| กลุ่ม | ข้อมูล | Symbol |
|---|---|---|
| ดัชนี | S&P 500, Nasdaq 100, Dow, Russell 2000 | `^GSPC` `^NDX` `^DJI` `^RUT` |
| Futures | S&P, Nasdaq (ใช้ตอนตลาดปิด) | `ES=F` `NQ=F` |
| บอนด์ | US 10Y yield | `^TNX` |
| บอนด์ | US 2Y yield, Fed Funds | FRED: `DGS2`, `DFF` |
| อารมณ์ตลาด | VIX | `^VIX` |
| ดอลลาร์ | DXY, USD/THB | `DX-Y.NYB` `THB=X` |
| สินค้าโภคภัณฑ์ | ทอง, WTI, Brent, ทองแดง | `GC=F` `CL=F` `BZ=F` `HG=F` |
| หุ้นใหญ่ (Mag 7) | AAPL MSFT NVDA GOOGL AMZN META TSLA | ตามชื่อ |

ค่าที่คำนวณที่ fetcher: yield curve (10Y − 2Y), % change, สถานะตลาด

## สถานะตลาด (คำนวณที่ fetcher ด้วย timezone `America/New_York`)
- PRE: 04:00–09:30 ET, OPEN: 09:30–16:00 ET, POST: 16:00–20:00 ET, CLOSED: อื่นๆ + เสาร์-อาทิตย์ + วันหยุด NYSE
- เวลาไทย: OPEN = 20:30–03:00 (ช่วง DST) / 21:30–04:00 (ช่วงปกติ) — **ห้าม hardcode offset** ให้ใช้ timezone library
- ตอน CLOSED/PRE หน้า Overview แสดง futures แทน cash index

## JSON schema (market.json)
```json
{
  "ts": 1790000000,
  "market": "OPEN",
  "q": [
    {"s": "SPX", "p": 5800.12, "c": 0.45, "sp": [5780, 5790, 5801]},
    {"s": "VIX", "p": 14.2, "c": -3.1}
  ],
  "b": [
    {"s": "NVDA", "p": 181.2, "c": 1.2},
    {"s": "SMCI", "p": 44.1, "c": 9.8, "h": 1}
  ],
  "curve": 0.35,
  "mood": {"v": 61, "r": "FUT +1.0%  10Y +4bp  VIX 16 +2%"},
  "fg": 31,
  "events": [{"t": "CPI", "at": 1790050000}]
}
```
- `s` ชื่อย่อแสดงผล, `p` ราคา, `c` % change, `sp` sparkline (optional, ≤ 30 จุด)
- `b` หุ้นหน้า bubble: core 25 ตัวเรียงตาม market cap (ครบทุกตัวเสมอ, ไม่มีข้อมูล = `p`/`c` เป็น null) ตามด้วยหุ้นร้อนแรง (`h:1`, Alpaca most-actives เรียงตาม |%chg|) ≤ 15 ตัว
- `mood` คะแนน risk-on/off ที่ fetcher คำนวณ (`market_mood()`: futures 30%, VIX 20%, breadth 20%, 10Y 10%, DXY 10%, ทอง 5%, น้ำมัน 5%) + `r` เหตุผล 4 ตัวที่มีผลมากสุด; `fg` CNN Fear & Greed (endpoint ไม่เป็นทางการ, null ถ้าดึงไม่ได้)
- คีย์สั้นโดยเจตนาเพื่อประหยัด RAM ฝั่ง ESP32

## หน้าจอ (แตะ/ปัดเพื่อเปลี่ยน)
0. **Bubbles** (หน้าแรก, ไม่มีแถบบน) — 40 วงเต็มจอ: core ตาม market cap + หุ้นร้อนแรง (ชื่อสีอำพัน); กลางวงมืด ขอบเรืองเขียว/แดง สว่างตาม |%chg|
1. **Overview** — 8 แถว: SPX/NDX/DJI (หรือ futures), 10Y, VIX, DXY, Gold, WTI + ป้ายสถานะตลาดมุมขวาบน
2. **Market Mood** — เกจคะแนนอารมณ์ตลาด 0–100 (Fear ↔ Greed) + เหตุผล; คอลัมน์ขวา VIX (สีตามโซน <15/20/30), yield curve, 10Y, 2Y, FED, CNN F&G
4. **Chart + Calendar** — sparkline S&P 5 วัน + เหตุการณ์สำคัญของสัปดาห์ (CPI, NFP, FOMC)

## สไตล์ UI
- พื้นดำ ตัวอักษรหลักสีส้ม/อำพัน (#FFA028) แบบ terminal
- ขึ้น = เขียว (#00D26A), ลง = แดง (#FF3B30), ไม่เปลี่ยน = เทา
- ตัวเลขชิดขวา, ใช้ฟอนต์ monospace สำหรับราคา
- แสดงเวลาอัปเดตล่าสุด; ถ้าข้อมูลเก่ากว่า 5 นาทีให้ขึ้นเตือน "STALE"

## ฟีเจอร์ฮาร์ดแวร์
- RGB LED: กระพริบเขียว/แดงเมื่อ SPX เปลี่ยน > ±1% หรือ VIX > 25 (threshold ตั้งใน config.h)
- LDR: ปรับ backlight อัตโนมัติ (PWM บน pin 21); มืดมาก → โหมดนาฬิกาหรี่แสง
- Speaker: เสียงเตือนสั้นๆ เฉพาะ alert เท่านั้น ปิดได้ใน config

## กฎการเขียนโค้ด
- ห้ามใช้ `String` ต่อกันในลูป — ใช้ `char[]` / `snprintf` เพื่อลด heap fragmentation
- งาน network ต้องไม่บล็อก UI: fetch แยก task (FreeRTOS, core 0) ส่งข้อมูลเข้า UI ผ่าน queue/mutex; LVGL เรียกจาก task เดียว (core 1) เท่านั้น
- WiFi หลุดต้อง reconnect เอง และ UI ยังแสดงข้อมูลเดิมพร้อมไอคอนออฟไลน์
- ห้าม commit `config.h` หรือ secret ใดๆ
- แต่ละหน้า UI อยู่ไฟล์ของตัวเองใน `src/ui/` มีฟังก์ชัน `create()` และ `update(const MarketData&)`
- เช็ก free heap (`ESP.getFreeHeap()`) ผ่าน Serial ทุกครั้งที่ fetch ระหว่างพัฒนา

## คำสั่งที่ใช้บ่อย
```bash
pio run                  # build
pio run -t upload        # แฟลช
pio device monitor -b 115200
```

## ข้อควรระวัง
- Yahoo Finance เป็น endpoint ไม่เป็นทางการ อาจเปลี่ยนหรือโดนบล็อก (IP ของ GitHub runner ด้วย) — fetcher มี fallback และใช้ค่าล่าสุดจาก market.json เดิม
- Alpaca ฟรี = IEX feed (ราคาใกล้จริง แต่ volume ไม่ครบ); ไม่มีคีย์ = หุ้น core มาจาก Yahoo และไม่มีหุ้นร้อนแรง
- คีย์ทุกตัวเก็บใน GitHub Actions Secrets เท่านั้น ห้ามใส่ในโค้ด
- จอบอร์ดนี้เป็น ST7789 (`ST7789_DRIVER` + `TFT_RGB_ORDER=TFT_BGR`, `TFT_INVERT 0`) — ILI9341 driver สีซีด, invert ซ้ำ = พื้นขาว
