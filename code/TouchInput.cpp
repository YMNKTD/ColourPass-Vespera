#include "TouchInput.h"
#include "Config.h"

void TouchInput::begin() { pinMode(Config::TOUCH_PIN, INPUT); }

TouchEvent TouchInput::poll() {
  bool raw = digitalRead(Config::TOUCH_PIN) == HIGH;
  unsigned long now = millis();
  if (raw != previousRaw_) { previousRaw_ = raw; edgeAt_ = now; }
  if (raw != stable_ && now - edgeAt_ >= Config::TOUCH_DEBOUNCE_MS) {
    stable_ = raw;
    if (stable_) { pressedAt_ = now; }
    else {
      unsigned long held = now - pressedAt_;
      if (held >= Config::REBOOT_HOLD_MS) return TOUCH_REBOOT;
      if (held >= Config::BG_HOLD_MS) return TOUCH_RECHECK;
      return TOUCH_TAP;
    }
  }
  return TOUCH_NONE;
}
