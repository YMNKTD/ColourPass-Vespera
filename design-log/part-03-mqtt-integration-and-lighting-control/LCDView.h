#pragma once
#include <Arduino.h>
#include <Waveshare_LCD1602_RGB.h>

class LCDView {
public:
  LCDView() : lcd_(16, 2) {}
  void begin();
  void show(const char* top, const char* bottom, uint8_t r, uint8_t g, uint8_t b);
  void setBacklight(uint8_t r, uint8_t g, uint8_t b);
private:
  Waveshare_LCD1602_RGB lcd_;
};
