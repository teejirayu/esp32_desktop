#include "hw.h"

#include <Arduino.h>
#include <esp_arduino_version.h>

#include "config.h"

namespace hw {
namespace {

constexpr int PIN_BL = 21;
constexpr int PIN_LED_R = 4, PIN_LED_G = 16, PIN_LED_B = 17;  // active LOW
constexpr int PIN_LDR = 34;
constexpr int PIN_SPK = 26;
constexpr int CH_BL = 0, CH_SPK = 4;  // ledc channel (core 2.x)

float s_ldr = 0;        // ค่า LDR แบบ smooth
bool s_dark = false;
int s_bl = BL_MAX;      // backlight ปัจจุบัน
Alert s_alert = Alert::NONE;
uint32_t s_toneOffAt = 0;
uint8_t s_beepsLeft = 0;

void blWrite(int v) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_BL, v);
#else
  ledcWrite(CH_BL, v);
#endif
}

void toneWrite(uint32_t f) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(PIN_SPK, f);
#else
  ledcWriteTone(CH_SPK, f);
#endif
}

void led(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r ? LOW : HIGH);
  digitalWrite(PIN_LED_G, g ? LOW : HIGH);
  digitalWrite(PIN_LED_B, b ? LOW : HIGH);
}

void startBeep(uint8_t count) {
#if ALERT_SOUND
  s_beepsLeft = count * 2;  // on/off สลับกัน
  s_toneOffAt = 0;
#endif
}

void serviceBeep(uint32_t now) {
  if (!s_beepsLeft || (int32_t)(now - s_toneOffAt) < 0) return;
  bool on = (s_beepsLeft % 2) == 0;
  toneWrite(on ? 2200 : 0);
  s_toneOffAt = now + (on ? 70 : 90);
  s_beepsLeft--;
}

}  // namespace

void begin() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  led(false, false, false);

  analogSetPinAttenuation(PIN_LDR, ADC_0db);  // LDR บน CYD ให้ช่วงแคบ ต้องใช้ 0dB
  s_ldr = analogRead(PIN_LDR);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_BL, 5000, 8);
  ledcAttach(PIN_SPK, 2000, 8);
#else
  ledcSetup(CH_BL, 5000, 8);
  ledcAttachPin(PIN_BL, CH_BL);
  ledcSetup(CH_SPK, 2000, 8);
  ledcAttachPin(PIN_SPK, CH_SPK);
#endif
  toneWrite(0);
  blWrite(s_bl);
}

bool isDark() { return s_dark; }

void setAlert(Alert a) {
  if (a == s_alert) return;
  if (a != Alert::NONE) startBeep(a == Alert::DOWN ? 3 : 2);
  s_alert = a;
  if (a == Alert::NONE) led(false, false, false);
}

void loop(bool dimMode) {
  uint32_t now = millis();
  serviceBeep(now);

  // LED: กระพริบสั้นทุก 1 วินาทีระหว่างมี alert
  if (s_alert != Alert::NONE) {
    bool on = (now % 1000) < 180;
    led(on && s_alert == Alert::DOWN, on && s_alert == Alert::UP, false);
  }

  static uint32_t lastSample = 0, lastLog = 0;
  if (now - lastSample < 50) return;
  lastSample = now;

  // LDR: ค่ามาก = มืด
  s_ldr = s_ldr * 0.9f + analogRead(PIN_LDR) * 0.1f;
  if (now - lastLog > 10000) {
    lastLog = now;
    Serial.printf("[hw] ldr=%d bl=%d dark=%d\n", (int)s_ldr, s_bl, s_dark);
  }

  int target = BL_MAX;
#if LDR_AUTO
  if (!s_dark && s_ldr > LDR_DARK_RAW) s_dark = true;
  else if (s_dark && s_ldr < LDR_DARK_RAW * 0.75f) s_dark = false;

  float t = (s_ldr - LDR_BRIGHT_RAW) / (float)(LDR_DARK_RAW - LDR_BRIGHT_RAW);
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  target = BL_MAX - (int)(t * (BL_MAX - BL_MIN * 4));
#endif
  if (dimMode) target = BL_MIN;

  // เฟดนุ่มๆ
  if (s_bl != target) {
    int step = abs(target - s_bl) > 8 ? 8 : 1;
    s_bl += target > s_bl ? step : -step;
    blWrite(s_bl);
  }
}

}  // namespace hw
