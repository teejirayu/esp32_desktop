#pragma once
#include <stdint.h>

// LED RGB, LDR, speaker, backlight
namespace hw {

enum class Alert : uint8_t { NONE, UP, DOWN };

void begin();
void loop(bool dimMode);  // เรียกทุกรอบใน loop()
bool isDark();            // มีฮิสเทอรีซิส
void setAlert(Alert a);   // เปลี่ยนเป็น UP/DOWN -> beep ครั้งเดียว, LED กระพริบจนกว่าจะเป็น NONE

}  // namespace hw
