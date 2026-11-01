#pragma once
#include <Arduino.h>

enum TouchEvent : uint8_t {
  TOUCH_NONE,
  TOUCH_TAP,
  TOUCH_RECHECK,
  TOUCH_REBOOT,
  TOUCH_HOLD_3S,
  TOUCH_HOLD_5S
};

class TouchInput {
public:
  void begin();
  TouchEvent poll();
  bool isPressed() const { return stable_; }
private:
  bool previousRaw_ = false;
  bool stable_ = false;
  unsigned long edgeAt_ = 0;
  unsigned long pressedAt_ = 0;
  uint8_t holdNoticeStep_ = 0;
};
