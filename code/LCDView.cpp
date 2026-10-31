#include "LCDView.h"
#include <Wire.h>
void LCDView::begin() { Wire.begin(); lcd_.init(); }
void LCDView::show(const char* top, const char* bottom, uint8_t r, uint8_t g, uint8_t b) {
  lcd_.clear();
  lcd_.setRGB(r, g, b);
  lcd_.setCursor(0, 0); lcd_.send_string(top);
  lcd_.setCursor(0, 1); lcd_.send_string(bottom);
}
void LCDView::setBacklight(uint8_t r, uint8_t g, uint8_t b) { lcd_.setRGB(r, g, b); }
