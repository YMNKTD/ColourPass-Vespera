#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "ColourModels.h"
#include "Config.h"

class LightingEngine {
public:
  static constexpr uint8_t STATIC_COUNT = 6;
  // 7 touch steps contain 6 unique ambient themes: Blue Mood appears twice.
  static constexpr uint8_t AMBIENT_STEPS = 7;

  LightingEngine() : strip_(Config::LOCAL_PIXELS, Config::NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800) {}
  void begin();
  void render(OutputKind kind, uint8_t effect, unsigned long elapsedMs);
  const uint8_t* payload() const { return frame_; }
  static const char* staticName(uint8_t index);
  static const char* ambientName(uint8_t step);
  static uint8_t nextAmbientStep(uint8_t step);
  static bool isAnimated(OutputKind kind, uint8_t effect);

private:
  uint8_t frame_[Config::VESPERA_PAYLOAD_SIZE] = {0};
  Adafruit_NeoPixel strip_;
  void setPixel(uint8_t col, uint8_t row, uint8_t r, uint8_t g, uint8_t b);
  void setColumn(uint8_t col, uint8_t r, uint8_t g, uint8_t b);
  void solid(uint8_t r, uint8_t g, uint8_t b);
  void copyToLocal();
};
