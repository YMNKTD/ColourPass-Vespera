
#pragma once
#include <Arduino.h>
#include "ColourModels.h"

struct SensorRaw {
  unsigned long r, g, b, c;
  bool ok;
};

struct SensorFeature {
  float r, g, b, logC;
};

struct SensorMatch {
  Card card;
  float dBg, best, second;
  bool validRead;
  Card nearest; // diagnostic closest colour
};

class ColourSensor {
public:
  void begin();
  void loadPresetProfiles();
  bool captureBackground();
  bool calibrateCard(uint8_t index);
  SensorMatch detect();
  void printRaw(const SensorRaw& x);

private:
  SensorFeature background_ = {0, 0, 0, 0};
  SensorFeature profiles_[CARD_COUNT];
  float emptyGate_ = 0.12f;

  unsigned long readChannel(uint8_t s2, uint8_t s3);
  SensorRaw readRaw();
  SensorRaw sampleMedian(uint8_t n);

  static unsigned long median(
    unsigned long* samples,
    int n
  );

  static SensorFeature feature(const SensorRaw& x);

  static float distance(
    const SensorFeature& a,
    const SensorFeature& b
  );
};
