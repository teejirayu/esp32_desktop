#pragma once
#include <stdint.h>
#include <string.h>

constexpr int MD_MAX_QUOTES = 32;
constexpr int MD_MAX_BUBBLES = 40;
constexpr int MD_MAX_SPARKS = 3;
constexpr int MD_MAX_SPARK_PTS = 30;
constexpr int MD_MAX_EVENTS = 8;

enum class Market : uint8_t { UNKNOWN, PRE, OPEN, POST, CLOSED };

struct Quote {
  char s[8];   // ชื่อย่อแสดงผล
  float p;     // ราคา (NAN = ไม่มีข้อมูล)
  float c;     // % change
};

// หุ้นหน้า bubble: core (เรียงตาม market cap) ตามด้วย hot (เรียงตามความแรง)
struct BubbleQuote {
  char s[8];
  float p;   // NAN = ไม่มีข้อมูล
  float c;
  bool hot;
};

struct Spark {
  char s[8];
  uint8_t n;
  float v[MD_MAX_SPARK_PTS];
};

struct Event {
  char t[12];
  uint32_t at;  // epoch (UTC)
};

struct MarketData {
  uint32_t ts = 0;  // เวลาที่ fetcher สร้างข้อมูล (epoch)
  Market market = Market::UNKNOWN;
  uint8_t nq = 0, nb = 0, nspark = 0, nev = 0;
  bool hasCurve = false;
  float curve = 0;  // 10Y - 2Y (percentage points)
  Quote q[MD_MAX_QUOTES];
  BubbleQuote b[MD_MAX_BUBBLES];
  Spark spark[MD_MAX_SPARKS];
  Event ev[MD_MAX_EVENTS];

  const Quote* find(const char* s) const {
    for (int i = 0; i < nq; ++i)
      if (strcmp(q[i].s, s) == 0) return &q[i];
    return nullptr;
  }
  const Spark* findSpark(const char* s) const {
    for (int i = 0; i < nspark; ++i)
      if (strcmp(spark[i].s, s) == 0) return &spark[i];
    return nullptr;
  }
  // ตอน PRE/CLOSED ใช้ futures แทน cash index
  bool futuresMode() const { return market == Market::PRE || market == Market::CLOSED; }
  // SPX หรือ ES ตามสถานะตลาด (ใช้กับ alert / นาฬิกา)
  const Quote* headline() const {
    const Quote* f = futuresMode() ? find("ES") : nullptr;
    return f ? f : find("SPX");
  }
};

struct NetStatus {
  bool wifi = false;
  bool demo = false;
  bool everOk = false;     // เคยได้ข้อมูลอย่างน้อยครั้งหนึ่ง
  uint32_t lastOkMs = 0;   // millis() ตอน fetch สำเร็จล่าสุด
  int lastHttp = 0;
  uint32_t fetchCount = 0;
};
