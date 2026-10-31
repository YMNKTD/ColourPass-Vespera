#pragma once
#include <Arduino.h>

enum TouchEvent : uint8_t { TOUCH_NONE, TOUCH_TAP, TOUCH_RECHECK, TOUCH_REBOOT };
class TouchInput {
public:
  void begin();
  TouchEvent poll();
private:
  bool previousRaw_ = false;
  bool stable_ = false;
  unsigned long edgeAt_ = 0;
  unsigned long pressedAt_ = 0;
};
