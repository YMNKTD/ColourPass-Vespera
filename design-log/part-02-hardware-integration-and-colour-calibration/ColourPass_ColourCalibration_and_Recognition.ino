
/*
 ColourPass - Colour Calibration and Recognition
 Arduino MKR WiFi 1010

 TOUCH CONTROL:
 - Short tap: calibration / lighting effect change
 - Hold 3–5 seconds, then release: background recheck
 - Hold >= 5 seconds, then release: reboot & recalibrate

 Colour sensor:
 S0=D0, S1=D1, S2=D2, S3=D3, OUT=D4

 Touch sensor:
 S=D5, +=3.3V, -=GND

 LCD1602 RGB:
 SDA=D11, SCL=D12, VCC=3.3V, GND=GND

 Serial Monitor: 115200 baud
 MQTT is not included.
*/

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Waveshare_LCD1602_RGB.h>

// =====================================================
// 1. PIN CONFIGURATION
// =====================================================

const byte S0_PIN = 0;
const byte S1_PIN = 1;
const byte S2_PIN = 2;
const byte S3_PIN = 3;
const byte OUT_PIN = 4;
const byte TOUCH_PIN = 5;

Waveshare_LCD1602_RGB lcd(16, 2);

// =====================================================
// 2. TIMING
// =====================================================

const unsigned long DEBOUNCE_MS = 40UL;

const unsigned long BG_HOLD_MS = 3000UL;
const unsigned long REBOOT_HOLD_MS = 5000UL;

const unsigned long SCAN_INTERVAL_MS = 190UL;
const unsigned long GRANTED_MS = 2000UL;
const unsigned long DENIED_MS = 1900UL;
const unsigned long ANIMATION_MS = 140UL;
const unsigned long LOG_MS = 1000UL;

const int STABLE_READS = 3;

// Preliminary classification thresholds.

const float MIN_BG_DISTANCE = 0.12f;
const float MAX_CARD_DISTANCE = 0.18f;
const float BG_ADVANTAGE = 0.08f;
const float MIN_SECOND_GAP = 0.018f;

// =====================================================
// 3. CARD COLOURS
// =====================================================

enum Card {
  CARD_RED,
  CARD_ORANGE,
  CARD_PINK,
  CARD_YELLOW,
  CARD_GREEN,
  CARD_BLUE,
  CARD_PURPLE,
  CARD_WHITE,
  CARD_BLACK,
  CARD_EMPTY,
  CARD_UNKNOWN = -1
};

const int CARD_COUNT = 9;

const char* const CARD_NAMES[CARD_COUNT] = {
  "RED",
  "ORANGE",
  "PINK",
  "YELLOW",
  "GREEN",
  "BLUE",
  "PURPLE",
  "WHITE",
  "BLACK"
};

// =====================================================
// 4. SYSTEM STATES
// =====================================================

enum State {
  ST_CALIBRATE,
  ST_BG_CONFIRM,
  ST_WAIT_EMPTY,
  ST_READY,
  ST_GRANTED,
  ST_DENIED,
  ST_STATIC,
  ST_AMBIENT,
  ST_NO_MODE
};

State state = ST_CALIBRATE;

// =====================================================
// 5. SENSOR STRUCTURES
// =====================================================

struct Raw {
  unsigned long r;
  unsigned long g;
  unsigned long b;
  unsigned long c;
  bool ok;
};

struct Feature {
  float r;
  float g;
  float b;
  float logC;
};

struct Match {
  int card;
  float dBg;
  float best;
  float second;
};

Feature bgReference = {0, 0, 0, 0};
Feature profiles[CARD_COUNT];

float emptyGate = MIN_BG_DISTANCE;

int calibrationIndex = 0;
int currentTicket = CARD_UNKNOWN;
int lastTriggered = CARD_UNKNOWN;

int candidate = CARD_UNKNOWN;
int candidateCount = 0;
int effectIndex = 0;

bool resumeCalibrationAfterBg = false;

unsigned long stateAt = 0;
unsigned long lastScanAt = 0;
unsigned long lastAnimAt = 0;
unsigned long lastLogAt = 0;

// =====================================================
// 6. TOUCH VARIABLES
// =====================================================

bool touchRawPrevious = false;
bool touchStable = false;

unsigned long touchEdgeAt = 0;
unsigned long touchPressedAt = 0;

// =====================================================
// 7. LCD HELPERS
// =====================================================

const char* cardName(int c) {
  if (c == CARD_EMPTY) return "EMPTY";

  if (c >= 0 && c < CARD_COUNT) {
    return CARD_NAMES[c];
  }

  return "UNKNOWN";
}

void displayText(
  const char* top,
  const char* bottom,
  byte red,
  byte green,
  byte blue
) {
  lcd.clear();
  lcd.setRGB(red, green, blue);

  lcd.setCursor(0, 0);
  lcd.send_string(top);

  lcd.setCursor(0, 1);
  lcd.send_string(bottom);
}

// =====================================================
// 8. SENSOR READING
// =====================================================

unsigned long readChannel(byte s2, byte s3) {
  digitalWrite(S2_PIN, s2);
  digitalWrite(S3_PIN, s3);

  delay(5);

  return pulseIn(OUT_PIN, LOW, 80000UL);
}

Raw readRaw() {
  Raw x;

  x.r = readChannel(LOW, LOW);
  x.g = readChannel(HIGH, HIGH);
  x.b = readChannel(LOW, HIGH);
  x.c = readChannel(HIGH, LOW);

  x.ok = x.r && x.g && x.b && x.c;

  return x;
}

// =====================================================
// 9. MEDIAN FILTER
// =====================================================

unsigned long median(
  unsigned long* values,
  int n
) {
  for (int i = 1; i < n; ++i) {
    unsigned long val = values[i];
    int j = i - 1;

    while (j >= 0 && values[j] > val) {
      values[j + 1] = values[j];
      --j;
    }

    values[j + 1] = val;
  }

  return values[n / 2];
}

Raw sampleMedian(int n) {
  Raw invalid = {0, 0, 0, 0, false};

  if (n < 1 || n > 11 || n % 2 == 0) {
    return invalid;
  }

  unsigned long r[11];
  unsigned long g[11];
  unsigned long b[11];
  unsigned long c[11];

  for (int i = 0; i < n; ++i) {
    Raw x = readRaw();

    if (!x.ok) return invalid;

    r[i] = x.r;
    g[i] = x.g;
    b[i] = x.b;
    c[i] = x.c;
  }

  Raw out = {
    median(r, n),
    median(g, n),
    median(b, n),
    median(c, n),
    true
  };

  return out;
}

// =====================================================
// 10. FEATURE EXTRACTION
// =====================================================

Feature makeFeature(const Raw& x) {
  float r = 1.0f / (float)x.r;
  float g = 1.0f / (float)x.g;
  float b = 1.0f / (float)x.b;

  float total = r + g + b;

  Feature f = {
    r / total,
    g / total,
    b / total,
    logf((float)x.c)
  };

  return f;
}

float featureDistance(
  const Feature& a,
  const Feature& b
) {
  float dr = a.r - b.r;
  float dg = a.g - b.g;
  float db = a.b - b.b;
  float dc = a.logC - b.logC;

  return 6.0f * (dr*dr + dg*dg + db*db)
       + 0.18f * dc*dc;
}

void logRaw(const Raw& x) {
  Serial.print("R:");
  Serial.print(x.r);

  Serial.print(" G:");
  Serial.print(x.g);

  Serial.print(" B:");
  Serial.print(x.b);

  Serial.print(" C:");
  Serial.println(x.c);
}

// =====================================================
// 11. BACKGROUND CALIBRATION
// =====================================================

bool captureBackground() {
  Raw a = sampleMedian(11);
  Raw b = sampleMedian(7);

  if (!a.ok || !b.ok) {
    return false;
  }

  Feature f1 = makeFeature(a);
  Feature f2 = makeFeature(b);

  float noise = featureDistance(f1, f2);

  float proposedGate = fmaxf(
    MIN_BG_DISTANCE,
    4.0f * noise + 0.04f
  );

  if (proposedGate > 0.25f) {
    return false;
  }

  bgReference = f1;
  emptyGate = proposedGate;

  Serial.print("EMPTY BACKGROUND | ");
  logRaw(a);

  Serial.print("Noise=");
  Serial.print(noise, 4);

  Serial.print(" Gate=");
  Serial.println(emptyGate, 4);

  return true;
}

// =====================================================
// 12. COLOUR CALIBRATION
// =====================================================

void showCalibration() {
  char line[17];

  snprintf(
    line,
    sizeof(line),
    "CAL %c/9 %.7s",
    char('1' + calibrationIndex),
    CARD_NAMES[calibrationIndex]
  );

  displayText(
    line,
    "PLACE + TAP",
    35, 65, 160
  );

  Serial.print("Calibrate: ");
  Serial.println(CARD_NAMES[calibrationIndex]);
}

void enterState(State next);

void calibrateCurrentCard() {
  Raw x = sampleMedian(11);

  if (!x.ok) {
    displayText(
      "SENSOR TIMEOUT",
      "TAP TO RETRY",
      255, 30, 0
    );

    delay(500);
    showCalibration();
    return;
  }

  Feature f = makeFeature(x);

  profiles[calibrationIndex] = f;

  Serial.print("CAL SAVED ");
  Serial.print(CARD_NAMES[calibrationIndex]);
  Serial.print(" | ");

  logRaw(x);

  Serial.print("Distance from BG: ");
  Serial.println(
    featureDistance(f, bgReference),
    4
  );

  ++calibrationIndex;

  if (calibrationIndex < CARD_COUNT) {
    showCalibration();
  } else {
    resumeCalibrationAfterBg = false;
    enterState(ST_BG_CONFIRM);
  }
}

// =====================================================
// 13. COLOUR CLASSIFICATION
// =====================================================

Match classify(const Feature& f) {
  Match m = {
    CARD_UNKNOWN,
    featureDistance(f, bgReference),
    1e9f,
    1e9f
  };

  int nearest = CARD_UNKNOWN;

  for (int i = 0; i < CARD_COUNT; ++i) {
    float d = featureDistance(f, profiles[i]);

    if (d < m.best) {
      m.second = m.best;
      m.best = d;
      nearest = i;
    } else if (d < m.second) {
      m.second = d;
    }
  }

  if (m.dBg <= emptyGate) {
    m.card = CARD_EMPTY;
  } else if (
    m.best <= MAX_CARD_DISTANCE &&
    m.best + BG_ADVANTAGE < m.dBg &&
    m.second - m.best >= MIN_SECOND_GAP
  ) {
    m.card = nearest;
  }

  return m;
}

// =====================================================
// 14. STATIC LIGHTING
// =====================================================

void showStatic() {
  const char* const names[5] = {
    "GREEN",
    "YELLOW",
    "BLUE",
    "PURPLE",
    "WHITE"
  };

  const byte rgb[5][3] = {
    {0, 220, 30},
    {235, 190, 0},
    {0, 40, 255},
    {140, 0, 190},
    {210, 210, 210}
  };

  int i = effectIndex % 5;

  char line[17];

  snprintf(
    line,
    sizeof(line),
    "STATIC %s",
    names[i]
  );

  displayText(
    line,
    "TAP / HOLD",
    rgb[i][0],
    rgb[i][1],
    rgb[i][2]
  );

  Serial.print("STATIC PREVIEW: ");
  Serial.println(names[i]);
}

// =====================================================
// 15. AMBIENT LIGHTING
// =====================================================

void showAmbient() {
  char line[17];

  snprintf(
    line,
    sizeof(line),
    "AMBIENT %d/5",
    effectIndex % 5 + 1
  );

  displayText(
    line,
    "TAP / HOLD",
    25, 35, 170
  );

  Serial.print("AMBIENT PREVIEW: ");
  Serial.println(effectIndex % 5 + 1);
}

// =====================================================
// 16. DEVICE STATE MANAGEMENT
// =====================================================

void enterState(State next) {
  state = next;
  stateAt = millis();
  lastAnimAt = 0;

  char line[17];

  switch (next) {
    case ST_CALIBRATE:
      showCalibration();
      break;

    case ST_BG_CONFIRM:
      displayText(
        "REMOVE ALL CARDS",
        "TAP WHEN EMPTY",
        30, 65, 170
      );

      Serial.println(
        "EMPTY CHECK: remove cards, then short-tap."
      );
      break;

    case ST_WAIT_EMPTY:
      displayText(
        "CHECKING EMPTY",
        "WAIT...",
        30, 65, 170
      );
      break;

    case ST_READY:
      displayText(
        "COLOURPASS",
        "SCAN YOUR TICKET",
        30, 65, 170
      );

      Serial.println(
        "READY: continuous scanning"
      );
      break;

    case ST_GRANTED:
      snprintf(
        line,
        sizeof(line),
        "VALID: %s",
        cardName(currentTicket)
      );

      displayText(
        "ACCESS GRANTED",
        line,
        0, 220, 25
      );
      break;

    case ST_DENIED:
      snprintf(
        line,
        sizeof(line),
        "INVALID: %s",
        cardName(currentTicket)
      );

      displayText(
        "ACCESS DENIED",
        line,
        255, 0, 0
      );
      break;

    case ST_STATIC:
      showStatic();
      break;

    case ST_AMBIENT:
      showAmbient();
      break;

    case ST_NO_MODE:
      displayText(
        "NO LIGHTING MODE",
        "REMOVE CARD",
        65, 55, 55
      );
      break;
  }
}

// =====================================================
// 17. CONFIRM EMPTY BACKGROUND
// =====================================================

void confirmEmptyBackground() {
  displayText(
    "SAVING EMPTY...",
    "NO CARD PLEASE",
    30, 65, 170
  );

  if (!captureBackground()) {
    displayText(
      "BG UNSTABLE",
      "TAP TO RETRY",
      255, 35, 0
    );

    delay(650);
    enterState(ST_BG_CONFIRM);
    return;
  }

  candidate = CARD_UNKNOWN;
  candidateCount = 0;
  lastTriggered = CARD_UNKNOWN;

  lastScanAt = millis();

  if (resumeCalibrationAfterBg) {
    resumeCalibrationAfterBg = false;
    enterState(ST_CALIBRATE);
  } else {
    enterState(ST_WAIT_EMPTY);
  }
}

// =====================================================
// 18. NEW TICKET DETECTION
// =====================================================

void newTicket(int c) {
  currentTicket = c;
  lastTriggered = c;

  Serial.print("NEW TICKET -> ");
  Serial.println(cardName(c));

  if (
    c == CARD_RED ||
    c == CARD_ORANGE ||
    c == CARD_PINK
  ) {
    enterState(ST_DENIED);
  } else if (
    c == CARD_YELLOW ||
    c == CARD_GREEN ||
    c == CARD_BLUE ||
    c == CARD_PURPLE
  ) {
    enterState(ST_GRANTED);
  } else {
    enterState(ST_NO_MODE);
  }
}

// =====================================================
// 19. CONTINUOUS COLOUR SCANNING
// =====================================================

void scanTickets() {
  if (
    state == ST_CALIBRATE ||
    state == ST_BG_CONFIRM
  ) {
    return;
  }

  unsigned long now = millis();

  if (now - lastScanAt < SCAN_INTERVAL_MS) {
    return;
  }

  lastScanAt = now;

  Raw v = sampleMedian(3);

  if (!v.ok) {
    candidate = CARD_UNKNOWN;
    candidateCount = 0;

    Serial.println("SENSOR TIMEOUT");
    return;
  }

  Match m = classify(makeFeature(v));

  if (now - lastLogAt >= LOG_MS) {
    lastLogAt = now;

    logRaw(v);

    Serial.print("Class=");
    Serial.print(cardName(m.card));

    Serial.print(" dBG=");
    Serial.print(m.dBg, 3);

    Serial.print(" dCard=");
    Serial.print(m.best, 3);

    Serial.print(" gap=");
    Serial.println(m.second - m.best, 3);
  }

  if (m.card == candidate) {
    if (candidateCount < STABLE_READS) {
      ++candidateCount;
    }
  } else {
    candidate = m.card;
    candidateCount = 1;
  }

  if (candidateCount < STABLE_READS) {
    return;
  }

  if (m.card == CARD_EMPTY) {
    lastTriggered = CARD_UNKNOWN;

    if (state == ST_WAIT_EMPTY) {
      enterState(ST_READY);
    }

    return;
  }

  if (
    state == ST_WAIT_EMPTY ||
    m.card == CARD_UNKNOWN
  ) {
    return;
  }

  if (m.card != lastTriggered) {
    newTicket(m.card);
  }
}

// =====================================================
// 20. AMBIENT ANIMATION
// =====================================================

void animateAmbient() {
  unsigned long now = millis();

  if (now - lastAnimAt < ANIMATION_MS) {
    return;
  }

  lastAnimAt = now;

  float t = (now - stateAt) / 1000.0f;
  float w = 0.5f + 0.5f * sinf(2.0f * t);

  int r = 0;
  int g = 0;
  int b = 0;

  switch (effectIndex % 5) {
    case 0:
      r = 20 + int(120 * w);
      b = 140 + int(100 * (1 - w));
      break;

    case 1:
      if ((now / 500UL) % 2 == 0) {
        r = 245;
      } else {
        r = g = b = 220;
      }
      break;

    case 2:
      g = 30 + int(115 * w);
      b = 135 + int(90 * (1 - w));
      break;

    case 3:
      r = 140 + int(105 * w);
      g = 30 + int(80 * w);
      b = 5;
      break;

    default:
      r = 120 + int(
        115 * sinf(t * 1.4f)
      );

      g = 120 + int(
        115 * sinf(t * 1.4f + 2.094f)
      );

      b = 120 + int(
        115 * sinf(t * 1.4f + 4.189f)
      );
      break;
  }

  lcd.setRGB(
    (byte)constrain(r, 0, 255),
    (byte)constrain(g, 0, 255),
    (byte)constrain(b, 0, 255)
  );
}

// =====================================================
// 21. OUTPUT UPDATES
// =====================================================

void updateOutputs() {
  unsigned long now = millis();

  if (
    state == ST_GRANTED &&
    now - stateAt >= GRANTED_MS
  ) {
    effectIndex =
      (currentTicket == CARD_YELLOW) ? 1 : 0;

    if (
      currentTicket == CARD_YELLOW ||
      currentTicket == CARD_GREEN
    ) {
      enterState(ST_STATIC);
    } else {
      enterState(ST_AMBIENT);
    }
  } else if (state == ST_DENIED) {
    if (now - stateAt >= DENIED_MS) {
      enterState(ST_READY);
    } else if (now - lastAnimAt >= ANIMATION_MS) {
      lastAnimAt = now;

      lcd.setRGB(
        ((now - stateAt) / 270UL) % 2 == 0
          ? 255 : 0,
        0,
        0
      );
    }
  } else if (
    state == ST_NO_MODE &&
    now - stateAt >= 1400UL
  ) {
    enterState(ST_READY);
  } else if (state == ST_AMBIENT) {
    animateAmbient();
  }
}

// =====================================================
// 22. SHORT TAP
// =====================================================

void shortTap() {
  if (state == ST_CALIBRATE) {
    calibrateCurrentCard();
  } else if (state == ST_BG_CONFIRM) {
    confirmEmptyBackground();
  } else if (state == ST_STATIC) {
    effectIndex = (effectIndex + 1) % 5;
    showStatic();
  } else if (state == ST_AMBIENT) {
    effectIndex = (effectIndex + 1) % 5;
    showAmbient();
  }
}

// =====================================================
// 23. LONG HOLD: 3 SECONDS
// =====================================================

void requestBackgroundRecheck() {
  // Keep calibration progress if currently calibrating.

  resumeCalibrationAfterBg =
    (state == ST_CALIBRATE);

  candidate = CARD_UNKNOWN;
  candidateCount = 0;

  Serial.println(
    "3-5s HOLD: Environment recheck requested."
  );

  enterState(ST_BG_CONFIRM);
}

// =====================================================
// 24. LONG HOLD: 5 SECONDS
// =====================================================

void rebootDevice() {
  Serial.println(
    "5s HOLD: Rebooting and restarting FULL calibration."
  );

  displayText(
    "RESTARTING...",
    "RECALIBRATING",
    245, 245, 245
  );

  delay(250);
  Serial.flush();

  // Real MCU software reset for MKR WiFi 1010.
  NVIC_SystemReset();
}

// =====================================================
// 25. TOUCH GESTURE RECOGNITION
// =====================================================

void processTouch() {
  bool raw =
    digitalRead(TOUCH_PIN) == HIGH;

  unsigned long now = millis();

  if (raw != touchRawPrevious) {
    touchRawPrevious = raw;
    touchEdgeAt = now;
  }

  // Debounce touch state.

  if (
    raw != touchStable &&
    now - touchEdgeAt >= DEBOUNCE_MS
  ) {
    touchStable = raw;

    if (raw) {
      // Touch has started.

      touchPressedAt = now;
    } else {
      // Touch has ended.
      // Decide the action only AFTER release.

      unsigned long held =
        now - touchPressedAt;

      if (held >= REBOOT_HOLD_MS) {
        rebootDevice();
      } else if (held >= BG_HOLD_MS) {
        requestBackgroundRecheck();
      } else {
        shortTap();
      }
    }
  }
}

// =====================================================
// 26. SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  pinMode(S0_PIN, OUTPUT);
  pinMode(S1_PIN, OUTPUT);
  pinMode(S2_PIN, OUTPUT);
  pinMode(S3_PIN, OUTPUT);

  pinMode(OUT_PIN, INPUT);
  pinMode(TOUCH_PIN, INPUT);

  // TCS3200: 20% frequency scaling.

  digitalWrite(S0_PIN, HIGH);
  digitalWrite(S1_PIN, LOW);

  Wire.begin();
  lcd.init();

  Serial.println(
    "ColourPass v1.0.4"
  );

  Serial.println(
    "3s background / 5s restart"
  );

  // Automatic background capture on startup.

  displayText(
    "CLEAR SENSOR",
    "EMPTY FOR 3 SEC",
    30, 65, 170
  );

  delay(3000);

  while (!captureBackground()) {
    displayText(
      "BG SENSOR ERROR",
      "CHECK + RESTART",
      255, 35, 0
    );

    delay(1500);
  }

  calibrationIndex = 0;
  enterState(ST_CALIBRATE);
}

// =====================================================
// 27. MAIN LOOP
// =====================================================

void loop() {
  processTouch();
  scanTickets();
  updateOutputs();
}
