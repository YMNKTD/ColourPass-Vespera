
#include "ColourSensor.h"
#include "Config.h"
#include <math.h>

void ColourSensor::begin() {
  pinMode(Config::SENSOR_S0, OUTPUT);
  pinMode(Config::SENSOR_S1, OUTPUT);
  pinMode(Config::SENSOR_S2, OUTPUT);
  pinMode(Config::SENSOR_S3, OUTPUT);
  pinMode(Config::SENSOR_OUT, INPUT);

  // TCS3200 frequency scaling: 20%
  digitalWrite(Config::SENSOR_S0, HIGH);
  digitalWrite(Config::SENSOR_S1, LOW);
}

unsigned long ColourSensor::readChannel(
  uint8_t s2,
  uint8_t s3
) {
  digitalWrite(Config::SENSOR_S2, s2);
  digitalWrite(Config::SENSOR_S3, s3);

  delay(5);

  return pulseIn(Config::SENSOR_OUT, LOW, 80000UL);
}

SensorRaw ColourSensor::readRaw() {
  SensorRaw x;

  x.r = readChannel(LOW, LOW);
  x.g = readChannel(HIGH, HIGH);
  x.b = readChannel(LOW, HIGH);
  x.c = readChannel(HIGH, LOW);

  x.ok = x.r && x.g && x.b && x.c;

  return x;
}

unsigned long ColourSensor::median(
  unsigned long* values,
  int n
) {
  for (int i = 1; i < n; ++i) {
    unsigned long value = values[i];
    int j = i - 1;

    while (j >= 0 && values[j] > value) {
      values[j + 1] = values[j];
      --j;
    }

    values[j + 1] = value;
  }

  return values[n / 2];
}

SensorRaw ColourSensor::sampleMedian(uint8_t n) {
  SensorRaw invalid = {0, 0, 0, 0, false};

  if (n < 1 || n > 11 || (n % 2 == 0)) {
    return invalid;
  }

  unsigned long r[11];
  unsigned long g[11];
  unsigned long b[11];
  unsigned long c[11];

  for (uint8_t i = 0; i < n; ++i) {
    SensorRaw x = readRaw();

    if (!x.ok) return invalid;

    r[i] = x.r;
    g[i] = x.g;
    b[i] = x.b;
    c[i] = x.c;
  }

  SensorRaw out = {
    median(r, n),
    median(g, n),
    median(b, n),
    median(c, n),
    true
  };

  return out;
}

SensorFeature ColourSensor::feature(const SensorRaw& x) {
  float r = 1.0f / float(x.r);
  float g = 1.0f / float(x.g);
  float b = 1.0f / float(x.b);

  float total = r + g + b;

  SensorFeature out = {
    r / total,
    g / total,
    b / total,
    logf(float(x.c))
  };

  return out;
}

float ColourSensor::distance(
  const SensorFeature& a,
  const SensorFeature& b
) {
  float dr = a.r - b.r;
  float dg = a.g - b.g;
  float db = a.b - b.b;
  float dc = a.logC - b.logC;

  // Reduced brightness-related weight.
  return 6.0f * (dr*dr + dg*dg + db*db)
       + 0.10f * dc*dc;
}

void ColourSensor::printRaw(const SensorRaw& x) {
  Serial.print(F("R:"));
  Serial.print(x.r);

  Serial.print(F(" G:"));
  Serial.print(x.g);

  Serial.print(F(" B:"));
  Serial.print(x.b);

  Serial.print(F(" C:"));
  Serial.println(x.c);
}

bool ColourSensor::captureBackground() {
  SensorRaw a = sampleMedian(11);
  SensorRaw b = sampleMedian(7);

  if (!a.ok || !b.ok) return false;

  SensorFeature f1 = feature(a);
  SensorFeature f2 = feature(b);

  float noise = distance(f1, f2);

  float proposed = fmaxf(
    Config::MIN_BG_DISTANCE,
    4.0f * noise + 0.04f
  );

  if (proposed > 0.25f) return false;

  background_ = f1;
  emptyGate_ = proposed;

  Serial.print(F("EMPTY BACKGROUND | "));
  printRaw(a);

  Serial.print(F("Noise="));
  Serial.print(noise, 4);

  Serial.print(F(" Gate="));
  Serial.println(emptyGate_, 4);

  return true;
}

bool ColourSensor::calibrateCard(uint8_t index) {
  if (index >= CARD_COUNT) return false;

  SensorRaw x = sampleMedian(11);

  if (!x.ok) return false;

  profiles_[index] = feature(x);

  Serial.print(F("CAL SAVED "));
  Serial.print(cardName(static_cast<Card>(index)));
  Serial.print(F(" | "));
  printRaw(x);

  Serial.print(F("Distance from BG: "));
  Serial.println(
    distance(profiles_[index], background_),
    4
  );

  return true;
}

SensorMatch ColourSensor::detect() {
  SensorRaw x = sampleMedian(3);

  SensorMatch m = {
    CARD_UNKNOWN,
    0.0f,
    1e9f,
    1e9f,
    x.ok,
    CARD_UNKNOWN
  };

  if (!x.ok) return m;

  SensorFeature f = feature(x);
  m.dBg = distance(f, background_);

  int nearest = CARD_UNKNOWN;

  for (uint8_t i = 0; i < CARD_COUNT; ++i) {
    float d = distance(f, profiles_[i]);

    if (d < m.best) {
      m.second = m.best;
      m.best = d;
      nearest = i;
    } else if (d < m.second) {
      m.second = d;
    }
  }

  m.nearest = static_cast<Card>(nearest);

  if (m.dBg <= emptyGate_) {
    m.card = CARD_EMPTY;
  } else if (
    m.best <= Config::MAX_CARD_DISTANCE &&
    m.best + Config::BG_ADVANTAGE < m.dBg &&
    m.second - m.best >= Config::MIN_SECOND_GAP
  ) {
    m.card = static_cast<Card>(nearest);
  }

  return m;
}
