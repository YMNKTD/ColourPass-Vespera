#include "TouchInput.h"
#include "Config.h"

void TouchInput::begin() { pinMode(Config::TOUCH_PIN, INPUT); }

TouchEvent TouchInput::poll() {
  bool raw = digitalRead(Config::TOUCH_PIN) == HIGH;
  unsigned long now = millis();
  if (raw != previousRaw_) { previousRaw_ = raw; edgeAt_ = now; }
  if (raw != stable_ && now - edgeAt_ >= Config::TOUCH_DEBOUNCE_MS) {
    stable_ = raw;
    if (stable_) {
      pressedAt_ = now;
      holdNoticeStep_ = 0;
    } else {
      unsigned long held = now - pressedAt_;
      holdNoticeStep_ = 0;
      if (held >= Config::REBOOT_HOLD_MS) return TOUCH_REBOOT;
      if (held >= Config::BG_HOLD_MS) return TOUCH_RECHECK;
      return TOUCH_TAP;
    }
  }

  // Show prompts while held; perform reset actions only on release.
  if (stable_) {
    unsigned long held = now - pressedAt_;
    if (held >= Config::REBOOT_HOLD_MS && holdNoticeStep_ < 2) {
      holdNoticeStep_ = 2;
      return TOUCH_HOLD_5S;
    }
    if (held >= Config::BG_HOLD_MS && holdNoticeStep_ < 1) {
      holdNoticeStep_ = 1;
      return TOUCH_HOLD_3S;
    }
  }
  return TOUCH_NONE;
}
