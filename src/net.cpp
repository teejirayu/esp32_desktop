#include "net.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <sys/time.h>
#include <time.h>

#include "bubble_layout.h"
#include "config.h"

namespace net {
namespace {

SemaphoreHandle_t s_mtx;
MarketData s_shared;  // ป้องกันด้วย s_mtx
NetStatus s_status;   // ป้องกันด้วย s_mtx
bool s_new = false;
MarketData s_work;    // ใช้เฉพาะใน net task

constexpr uint32_t RETRY_MS = 15000;
constexpr uint32_t WIFI_RETRY_MS = 20000;

bool isDemo() { return strlen(DATA_URL) == 0 || strstr(DATA_URL, "YOUR_") != nullptr; }

Market parseMarket(const char* m) {
  if (!strcmp(m, "OPEN")) return Market::OPEN;
  if (!strcmp(m, "PRE")) return Market::PRE;
  if (!strcmp(m, "POST")) return Market::POST;
  if (!strcmp(m, "CLOSED")) return Market::CLOSED;
  return Market::UNKNOWN;
}

bool parse(const char* body, MarketData& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[net] json error: %s\n", err.c_str());
    return false;
  }
  out = MarketData();
  out.ts = doc["ts"] | 0u;
  out.market = parseMarket(doc["market"] | "");

  for (JsonObject o : doc["q"].as<JsonArray>()) {
    const char* s = o["s"];
    if (!s || out.nq >= MD_MAX_QUOTES) continue;
    Quote& q = out.q[out.nq++];
    strlcpy(q.s, s, sizeof(q.s));
    q.p = o["p"] | NAN;
    q.c = o["c"] | 0.0f;

    JsonArray sp = o["sp"];
    if (sp && out.nspark < MD_MAX_SPARKS) {
      Spark& k = out.spark[out.nspark++];
      strlcpy(k.s, s, sizeof(k.s));
      k.n = 0;
      for (float v : sp) {
        if (k.n >= MD_MAX_SPARK_PTS) break;
        k.v[k.n++] = v;
      }
    }
  }

  for (JsonObject o : doc["b"].as<JsonArray>()) {
    const char* s = o["s"];
    if (!s || out.nb >= MD_MAX_BUBBLES) continue;
    BubbleQuote& bq = out.b[out.nb++];
    strlcpy(bq.s, s, sizeof(bq.s));
    bq.p = o["p"].isNull() ? NAN : o["p"].as<float>();
    bq.c = o["c"].isNull() ? NAN : o["c"].as<float>();
    bq.hot = o["h"] | 0;
  }

  JsonObject mood = doc["mood"];
  if (mood) {
    out.mood = mood["v"] | -1;
    strlcpy(out.moodWhy, mood["r"] | "", sizeof(out.moodWhy));
  }
  out.fg = doc["fg"] | -1;

  if (!doc["curve"].isNull()) {
    out.curve = doc["curve"];
    out.hasCurve = true;
  }

  for (JsonObject e : doc["events"].as<JsonArray>()) {
    if (out.nev >= MD_MAX_EVENTS) break;
    Event& ev = out.ev[out.nev++];
    strlcpy(ev.t, e["t"] | "?", sizeof(ev.t));
    ev.at = e["at"] | 0u;
  }
  return out.nq > 0;
}

// อ่าน market.json จาก branch data ผ่าน GitHub API (raw.githubusercontent แคช 5 นาที)
bool fetchData(MarketData& out, int& code) {
  WiFiClientSecure client;
  client.setInsecure();  // ไม่ตรวจ cert — ข้อมูลสาธารณะ
  HTTPClient http;
  http.setTimeout(15000);
  http.setUserAgent("CYD-MarketDesk/1.0");  // GitHub API บังคับ
  if (!http.begin(client, DATA_URL)) {
    code = -1;
    return false;
  }
  http.addHeader("Accept", "application/vnd.github.raw+json");
  if (strlen(GITHUB_TOKEN)) http.addHeader("Authorization", "Bearer " GITHUB_TOKEN);
  const char* hdrs[] = {"X-RateLimit-Remaining"};
  http.collectHeaders(hdrs, 1);
  code = http.GET();
  bool ok = false;
  if (code == HTTP_CODE_OK) {
    String body = http.getString();  // ~2-3KB ครั้งเดียวต่อรอบ (ไม่ต่อในลูป)
    ok = parse(body.c_str(), out);
  }
  Serial.printf("[net] github rate-limit remaining=%s\n", http.header("X-RateLimit-Remaining").c_str());
  http.end();
  return ok;
}

// ---------- DEMO: สุ่มข้อมูลเพื่อทดสอบหน้าจอเมื่อยังไม่ได้ตั้ง DATA_URL ----------
struct DemoSym {
  const char* s;
  float base;
  float vol;  // % ต่อรอบ
};
const DemoSym DEMO_SYMS[] = {
    {"SPX", 5812.3f, 0.15f}, {"NDX", 20510.f, 0.2f},  {"DJI", 42180.f, 0.12f}, {"RUT", 2210.f, 0.2f},
    {"ES", 5830.5f, 0.15f},  {"NQ", 20600.f, 0.2f},   {"10Y", 4.231f, 0.3f},   {"2Y", 3.884f, 0.3f},
    {"FED", 4.33f, 0.f},     {"VIX", 16.4f, 2.0f},    {"DXY", 101.2f, 0.05f},  {"THB", 32.45f, 0.05f},
    {"GOLD", 2650.f, 0.1f},  {"WTI", 71.3f, 0.3f},    {"BRENT", 74.9f, 0.3f},  {"COPPER", 4.51f, 0.2f},
};
// ตรงกับ CORE ใน fetcher/fetch.py + ตัวอย่างหุ้นร้อนแรง
const char* DEMO_CORE[BUBBLE_CORE] = {
    "NVDA", "MSFT", "AAPL", "GOOGL", "AMZN", "META", "AVGO", "TSLA", "BRK.B", "JPM",
    "WMT",  "LLY",  "ORCL", "V",     "MA",   "NFLX", "XOM",  "COST", "PLTR",  "JNJ",
    "HD",   "PG",   "AMD",  "ABBV",  "BAC"};
const char* DEMO_HOT[BUBBLE_HOT] = {"SMCI", "MSTR", "COIN", "HOOD", "RIVN", "SOFI", "MARA", "RKLB",
                                    "IONQ", "NIO",  "SNAP", "UBER", "SHOP", "F",    "PFE"};
constexpr int DEMO_N = sizeof(DEMO_SYMS) / sizeof(DEMO_SYMS[0]);

float frand(float a) { return ((float)esp_random() / 4294967295.0f * 2.f - 1.f) * a; }

void makeDemo(MarketData& out) {
  static float chg[DEMO_N];
  static float spk[2][MD_MAX_SPARK_PTS];
  static bool init = false;
  if (!init) {
    for (int i = 0; i < DEMO_N; ++i) chg[i] = frand(2.f);
    for (int k = 0; k < 2; ++k) {
      float v = DEMO_SYMS[k == 0 ? 0 : 4].base * 0.985f;
      for (int i = 0; i < MD_MAX_SPARK_PTS; ++i) spk[k][i] = v += frand(12.f) + 1.5f;
    }
    init = true;
  }
  out = MarketData();
  time_t now = time(nullptr);
  out.ts = now > 1700000000 ? now : 1790000000;
  out.market = Market::OPEN;
  for (int i = 0; i < DEMO_N; ++i) {
    chg[i] += frand(DEMO_SYMS[i].vol);
    if (chg[i] > 5) chg[i] = 5;
    if (chg[i] < -5) chg[i] = -5;
    Quote& q = out.q[out.nq++];
    strlcpy(q.s, DEMO_SYMS[i].s, sizeof(q.s));
    q.c = chg[i];
    q.p = DEMO_SYMS[i].base * (1.f + chg[i] / 100.f);
  }
  static float bchg[BUBBLE_COUNT];
  for (int i = 0; i < BUBBLE_COUNT; ++i) {
    bool hot = i >= BUBBLE_CORE;
    bchg[i] += frand(hot ? 1.2f : 0.5f);
    float lim = hot ? 12.f : 4.f;
    if (bchg[i] > lim) bchg[i] = lim;
    if (bchg[i] < -lim) bchg[i] = -lim;
    BubbleQuote& b = out.b[out.nb++];
    strlcpy(b.s, hot ? DEMO_HOT[i - BUBBLE_CORE] : DEMO_CORE[i], sizeof(b.s));
    b.c = bchg[i];
    b.p = 100.f + i * 7.3f;
    b.hot = hot;
  }
  for (int k = 0; k < 2; ++k) {
    memmove(spk[k], spk[k] + 1, sizeof(float) * (MD_MAX_SPARK_PTS - 1));
    spk[k][MD_MAX_SPARK_PTS - 1] = spk[k][MD_MAX_SPARK_PTS - 2] + frand(12.f);
    Spark& s = out.spark[out.nspark++];
    strlcpy(s.s, k == 0 ? "SPX" : "ES", sizeof(s.s));
    s.n = MD_MAX_SPARK_PTS;
    memcpy(s.v, spk[k], sizeof(s.v));
  }
  out.hasCurve = true;
  out.curve = out.find("10Y")->p - out.find("2Y")->p;
  static float mood = 55;
  mood += frand(6.f);
  mood = mood < 5 ? 5 : (mood > 95 ? 95 : mood);
  out.mood = (int16_t)mood;
  out.fg = 42;
  snprintf(out.moodWhy, sizeof(out.moodWhy), "FUT %+.1f%%  VIX 16 %+.0f%%  UP 26/40", (double)chg[4],
           (double)chg[9]);
  const struct { const char* t; int32_t dt; } evs[] = {
      {"NFP", -3 * 3600}, {"CPI", 26 * 3600 + 1200}, {"FOMC", 3 * 86400 + 7200}, {"PCE", 5 * 86400}};
  for (auto& e : evs) {
    Event& ev = out.ev[out.nev++];
    strlcpy(ev.t, e.t, sizeof(ev.t));
    ev.at = out.ts + e.dt;
  }
}

// ---------------------------------------------------------------

void doFetch(bool demo) {
  int code = 0;
  uint32_t t0 = millis();
  bool ok;
  if (demo) {
    makeDemo(s_work);
    ok = true;
    code = 200;
  } else {
    ok = fetchData(s_work, code);
  }

  // ถ้า NTP ยังไม่ซิงก์ ใช้เวลาจากข้อมูลแทน
  if (ok && time(nullptr) < 1700000000 && s_work.ts > 1700000000) {
    timeval tv = {(time_t)s_work.ts, 0};
    settimeofday(&tv, nullptr);
  }

  xSemaphoreTake(s_mtx, portMAX_DELAY);
  s_status.lastHttp = code;
  s_status.fetchCount++;
  if (ok) {
    s_shared = s_work;
    s_new = true;
    s_status.everOk = true;
    s_status.lastOkMs = millis();
  }
  xSemaphoreGive(s_mtx);

  Serial.printf("[net] fetch %s http=%d q=%d %lums heap=%u min=%u\n", ok ? "OK" : "FAIL", code,
                ok ? s_work.nq : 0, (unsigned long)(millis() - t0), ESP.getFreeHeap(),
                ESP.getMinFreeHeap());
}

void task(void*) {
  const bool demo = isDemo();
  xSemaphoreTake(s_mtx, portMAX_DELAY);
  s_status.demo = demo;
  xSemaphoreGive(s_mtx);
  if (demo) Serial.println("[net] DATA_URL not set -> DEMO mode");

  // หลายเครือข่าย: ต่อไม่ได้ภายใน WIFI_RETRY_MS -> สลับไปตัวถัดไป
  struct Cred {
    const char *ssid, *pass;
  };
  static const Cred creds[] = WIFI_NETWORKS;
  constexpr int ncred = sizeof(creds) / sizeof(creds[0]);
  int credIdx = 0;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(creds[0].ssid, creds[0].pass);
  Serial.printf("[net] WiFi connecting to %s\n", creds[0].ssid);

  bool ntpStarted = false, wasUp = false;
  uint32_t lastFetch = 0, lastWifiTry = millis();
  bool first = true;

  for (;;) {
    bool up = WiFi.status() == WL_CONNECTED;
    if (up != wasUp) {
      wasUp = up;
      xSemaphoreTake(s_mtx, portMAX_DELAY);
      s_status.wifi = up;
      xSemaphoreGive(s_mtx);
      if (up) Serial.printf("[net] WiFi up %s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      else Serial.println("[net] WiFi down");
    }

    if (!up) {
      if (millis() - lastWifiTry > WIFI_RETRY_MS) {
        lastWifiTry = millis();
        static bool scanned = false;
        Serial.printf("[net] WiFi not connected (status=%d), retry\n", (int)WiFi.status());
        if (!scanned) {  // ครั้งแรกที่ต่อไม่ได้: สแกนดูว่าเห็น SSID ไหม (ESP32 รองรับแค่ 2.4GHz)
          scanned = true;
          WiFi.disconnect();
          int n = WiFi.scanNetworks();
          Serial.printf("[net] scan found %d networks\n", n);
          for (int i = 0; i < n; ++i)
            Serial.printf("[net]   %-24s ch%-2d rssi=%d\n", WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
          WiFi.scanDelete();
        }
        WiFi.disconnect();
        credIdx = (credIdx + 1) % ncred;
        WiFi.begin(creds[credIdx].ssid, creds[credIdx].pass);
        Serial.printf("[net] WiFi connecting to %s\n", creds[credIdx].ssid);
      }
      // demo ยังทำงานได้แม้ไม่มี WiFi
      if (demo && (first || millis() - lastFetch >= FETCH_INTERVAL_MS)) {
        first = false;
        lastFetch = millis();
        doFetch(true);
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    if (!ntpStarted) {
      configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
      ntpStarted = true;
    }

    if (first || millis() - lastFetch >= FETCH_INTERVAL_MS) {
      first = false;
      lastFetch = millis();
      doFetch(demo);
      // fetch ล้มเหลว -> ลองใหม่ใน RETRY_MS แทนที่จะรอครบรอบ
      xSemaphoreTake(s_mtx, portMAX_DELAY);
      bool failed = millis() - s_status.lastOkMs > 5000 || !s_status.everOk;
      xSemaphoreGive(s_mtx);
      if (failed) lastFetch = millis() - FETCH_INTERVAL_MS + RETRY_MS;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

}  // namespace

void begin() {
  s_mtx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(task, "net", 16384, nullptr, 1, nullptr, 0);
}

bool take(MarketData& out, NetStatus& st) {
  xSemaphoreTake(s_mtx, portMAX_DELAY);
  bool n = s_new;
  if (n) {
    out = s_shared;
    s_new = false;
  }
  st = s_status;
  xSemaphoreGive(s_mtx);
  return n;
}

}  // namespace net
