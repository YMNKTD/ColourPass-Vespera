
/*
  ColourPass - Colour Calibration and Recognition
  Arduino MKR WiFi 1010

  Improvements:
  - Automatic background capture on startup
  - Reconfirm empty background after calibration
  - Median filtering for more stable readings
  - Reject ambiguous colour classifications
  - Continuous multi-ticket validation
  - LCD lighting previews

  Colour Sensor:
    S0=D0, S1=D1, S2=D2, S3=D3, OUT=D4
    OE=GND, VDD=3.3V, GND=GND

  Touch Sensor:
    S=D5, +=3.3V, -=GND

  LCD1602 RGB:
    SDA=D11, SCL=D12, VCC=3.3V, GND=GND

  Serial Monitor: 115200 baud
*/

#include <Wire.h>
#include <math.h>
#include <Waveshare_LCD1602_RGB.h>

// =====================================================
// 1. PIN CONFIGURATION
// =====================================================

const byte PIN_S0 = 0;
const byte PIN_S1 = 1;
const byte PIN_S2 = 2;
const byte PIN_S3 = 3;
const byte PIN_OUT = 4;
const byte PIN_TOUCH = 5;

Waveshare_LCD1602_RGB lcd(16, 2);

// =====================================================
// 2. TIMING
// =====================================================

const unsigned long PULSE_TIMEOUT_US = 80000UL;
const unsigned long SCAN_INTERVAL_MS = 190UL;
const unsigned long LONG_PRESS_MS = 3000UL;
const unsigned long DEBOUNCE_MS = 40UL;

const unsigned long CONFIRM_MS = 2000UL;
const unsigned long DENIED_MS = 1900UL;
const unsigned long RESET_MS = 450UL;
const unsigned long ANIMATION_MS = 140UL;
const unsigned long LOG_MS = 1000UL;

// Preliminary thresholds.
// Adjust using actual experimental results.

const float MIN_BACKGROUND_DISTANCE = 0.12f;
const float MAX_CARD_DISTANCE = 0.18f;
const float REQUIRED_BACKGROUND_ADVANTAGE = 0.08f;
const float MIN_RUNNER_UP_GAP = 0.018f;

const int REQUIRED_STABLE = 3;

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

const char* const CARD_LABELS[CARD_COUNT] = {
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
// 4. DEVICE MODES
// =====================================================

enum Mode {
  MODE_CAL,
  MODE_CONFIRM_EMPTY,
  MODE_WAIT_EMPTY,
  MODE_READY,
  MODE_GRANTED,
  MODE_DENIED,
  MODE_STATIC,
  MODE_AMBIENT,
  MODE_NO_MODE,
  MODE_RESET
};

Mode mode = MODE_CAL;

// =====================================================
// 5. DATA STRUCTURES
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

struct Result {
  int card;
  float dBG;
  float best;
  float second;
};

Feature background = {0, 0, 0, 0};
Feature profiles[CARD_COUNT];

float backgroundNoise = 0;
float emptyGate = MIN_BACKGROUND_DISTANCE;

int calIndex = 0;
int ticket = CARD_UNKNOWN;

int lastTriggered = CARD_UNKNOWN;
int candidate = CARD_UNKNOWN;
int candidateCount = 0;
int effectIndex = 0;

unsigned long stateTime = 0;
unsigned long lastScan = 0;
unsigned long lastLog = 0;
unsigned long lastAnim = 0;

// Touch input

bool touchRawPrev = false;
bool touchStable = false;
bool longPressDone = false;

unsigned long touchChanged = 0;
unsigned long pressedAt = 0;

// =====================================================
// 6. LCD HELPERS
// =====================================================

const char* cardName(int c) {
  if (c == CARD_EMPTY) return "EMPTY";

  if (c >= 0 && c < CARD_COUNT) {
    return CARD_LABELS[c];
  }

  return "UNKNOWN";
}

void displayText(
  const char* top,
  const char* bottom,
  byte r,
  byte g,
  byte b
) {
  lcd.clear();
  lcd.setRGB(r, g, b);

  lcd.setCursor(0, 0);
  lcd.send_string(top);

  lcd.setCursor(0, 1);
  lcd.send_string(bottom);
}

// =====================================================
// 7. SENSOR READING
// =====================================================

unsigned long readChannel(byte s2, byte s3) {
  digitalWrite(PIN_S2, s2);
  digitalWrite(PIN_S3, s3);

  delay(5);

  return pulseIn(
    PIN_OUT,
    LOW,
    PULSE_TIMEOUT_US
  );
}

Raw readRaw() {
  Raw x;

  x.r = readChannel(LOW, LOW);
  x.g = readChannel(HIGH, HIGH);
  x.b = readChannel(LOW, HIGH);
  x.c = readChannel(HIGH, LOW);

  x.ok =
    x.r > 0 &&
    x.g > 0 &&
    x.b > 0 &&
    x.c > 0;

  return x;
}

// =====================================================
// 8. MEDIAN FILTER
// =====================================================

unsigned long median(unsigned long* a, int n) {
  for (int i = 1; i < n; i++) {
    unsigned long v = a[i];
    int j = i - 1;

    while (j >= 0 && a[j] > v) {
      a[j + 1] = a[j];
      j--;
    }

    a[j + 1] = v;
  }

  return a[n / 2];
}

Raw sampleMedian(int n) {
  unsigned long rr[11];
  unsigned long gg[11];
  unsigned long bb[11];
  unsigned long cc[11];

  Raw invalid = {0, 0, 0, 0, false};

  if (
    n < 1 ||
    n > 11 ||
    n % 2 == 0
  ) {
    return invalid;
  }

  for (int i = 0; i < n; i++) {
    Raw x = readRaw();

    if (!x.ok) {
      return invalid;
    }

    rr[i] = x.r;
    gg[i] = x.g;
    bb[i] = x.b;
    cc[i] = x.c;
  }

  Raw v = {
    median(rr, n),
    median(gg, n),
    median(bb, n),
    median(cc, n),
    true
  };

  return v;
}

// =====================================================
// 9. FEATURE EXTRACTION
// =====================================================

Feature features(Raw x) {
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

float distanceF(Feature a, Feature b) {
  float dr = a.r - b.r;
  float dg = a.g - b.g;
  float db = a.b - b.b;
  float dc = a.logC - b.logC;

  return
    6.0f * (dr * dr + dg * dg + db * db)
    + 0.18f * dc * dc;
}

void printRaw(Raw x) {
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
// 10. BACKGROUND CALIBRATION
// =====================================================

bool captureBackground() {
  Raw a = sampleMedian(11);
  Raw b = sampleMedian(7);

  if (!a.ok || !b.ok) {
    return false;
  }

  Feature fa = features(a);
  Feature fb = features(b);

  background = fa;

  backgroundNoise = distanceF(fa, fb);

  emptyGate = fmaxf(
    MIN_BACKGROUND_DISTANCE,
    4.0f * backgroundNoise + 0.04f
  );

  if (emptyGate > 0.25f) {
    return false;
  }

  Serial.print("EMPTY BACKGROUND | ");
  printRaw(a);

  Serial.print("Noise=");
  Serial.print(backgroundNoise, 4);

  Serial.print(" EmptyGate=");
  Serial.println(emptyGate, 4);

  return true;
}

// =====================================================
// 11. COLOUR CALIBRATION
// =====================================================

void showCalibration() {
  char line[17];

  snprintf(
    line,
    sizeof(line),
    "CAL %02d/09 %s",
    calIndex + 1,
    CARD_LABELS[calIndex]
  );

  displayText(
    line,
    "PLACE + TAP",
    35, 65, 160
  );

  Serial.print("Place and tap: ");
  Serial.println(CARD_LABELS[calIndex]);
}

void captureCard() {
  Raw v = sampleMedian(11);

  if (!v.ok) {
    displayText(
      "SENSOR TIMEOUT",
      "TAP TO RETRY",
      255, 40, 0
    );

    delay(650);
    showCalibration();
    return;
  }

  Feature f = features(v);

  profiles[calIndex] = f;

  Serial.print("CAL ");
  Serial.print(CARD_LABELS[calIndex]);
  Serial.print(" | ");
  printRaw(v);

  float d = distanceF(f, background);

  Serial.print("Difference from empty: ");
  Serial.println(d, 4);

  if (d < emptyGate + 0.04f) {
    Serial.println(
      "WARNING: Card too similar to background."
    );
  }

  calIndex++;

  if (calIndex < CARD_COUNT) {
    showCalibration();
  } else {
    mode = MODE_CONFIRM_EMPTY;

    displayText(
      "REMOVE ALL CARDS",
      "TAP WHEN EMPTY",
      30, 65, 170
    );

    Serial.println(
      "REMOVE ALL CARDS, then TAP to confirm empty."
    );
  }
}

// =====================================================
// 12. CONFIDENCE-BASED CLASSIFICATION
// =====================================================

Result classify(Feature f) {
  Result out = {
    CARD_UNKNOWN,
    distanceF(f, background),
    1e9f,
    1e9f
  };

  int nearest = CARD_UNKNOWN;

  for (int i = 0; i < CARD_COUNT; i++) {
    float d = distanceF(f, profiles[i]);

    if (d < out.best) {
      out.second = out.best;
      out.best = d;
      nearest = i;
    } else if (d < out.second) {
      out.second = d;
    }
  }

  // Background takes priority over ticket matching.

  if (out.dBG <= emptyGate) {
    out.card = CARD_EMPTY;
    return out;
  }

  // Require a close colour match and a clear
  // advantage over the background and runner-up.

  if (
    out.best <= MAX_CARD_DISTANCE &&
    out.best + REQUIRED_BACKGROUND_ADVANTAGE
      < out.dBG &&
    out.second - out.best >= MIN_RUNNER_UP_GAP
  ) {
    out.card = nearest;
  }

  return out;
}

// =====================================================
// 13. STATIC LIGHTING PREVIEW
// =====================================================

void showStatic() {
  const char* names[5] = {
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
    "TAP/HOLD 3 SEC",
    rgb[i][0],
    rgb[i][1],
    rgb[i][2]
  );

  Serial.print("PREVIEW STATIC ");
  Serial.println(names[i]);
}

// =====================================================
// 14. AMBIENT LIGHTING PREVIEW
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
    "TAP/HOLD 3 SEC",
    25, 35, 170
  );

  Serial.print("PREVIEW AMBIENT ");
  Serial.println(effectIndex % 5 + 1);
}

// =====================================================
// 15. DEVICE STATE MANAGEMENT
// =====================================================

void enterMode(Mode m) {
  mode = m;
  stateTime = millis();
  lastAnim = 0;

  if (m == MODE_READY) {
    displayText(
      "COLOURPASS",
      "SCAN YOUR TICKET",
      30, 65, 170
    );

    Serial.println(
      "READY: Waiting for a new ticket."
    );
  }

  else if (m == MODE_GRANTED) {
    char line[17];

    snprintf(
      line,
      sizeof(line),
      "VALID: %s",
      cardName(ticket)
    );

    displayText(
      "ACCESS GRANTED",
      line,
      0, 220, 25
    );
  }

  else if (m == MODE_DENIED) {
    char line[17];

    snprintf(
      line,
      sizeof(line),
      "INVALID: %s",
      cardName(ticket)
    );

    displayText(
      "ACCESS DENIED",
      line,
      255, 0, 0
    );
  }

  else if (m == MODE_STATIC) {
    showStatic();
  }

  else if (m == MODE_AMBIENT) {
    showAmbient();
  }

  else if (m == MODE_NO_MODE) {
    displayText(
      "NO LIGHTING MODE",
      "WHITE/BLACK CARD",
      75, 60, 60
    );
  }

  else if (m == MODE_RESET) {
    displayText(
      "RESET COMPLETE",
      "REMOVE CARD",
      240, 240, 240
    );
  }

  else if (m == MODE_WAIT_EMPTY) {
    displayText(
      "CHECKING EMPTY",
      "TAP TO RELEARN",
      30, 65, 170
    );
  }
}

// =====================================================
// 16. NEW TICKET HANDLING
// =====================================================

void newTicket(int c) {
  ticket = c;
  lastTriggered = c;

  Serial.print("NEW TICKET -> ");
  Serial.println(cardName(c));

  if (
    c == CARD_RED ||
    c == CARD_ORANGE ||
    c == CARD_PINK
  ) {
    enterMode(MODE_DENIED);
  }

  else if (
    c == CARD_YELLOW ||
    c == CARD_GREEN ||
    c == CARD_BLUE ||
    c == CARD_PURPLE
  ) {
    enterMode(MODE_GRANTED);
  }

  else {
    enterMode(MODE_NO_MODE);
  }
}

// =====================================================
// 17. CONTINUOUS TICKET SCANNING
// =====================================================

void processColour() {
  if (
    mode == MODE_CAL ||
    mode == MODE_CONFIRM_EMPTY ||
    mode == MODE_RESET
  ) {
    return;
  }

  unsigned long now = millis();

  if (now - lastScan < SCAN_INTERVAL_MS) {
    return;
  }

  lastScan = now;

  Raw v = sampleMedian(3);

  if (!v.ok) {
    candidate = CARD_UNKNOWN;
    candidateCount = 0;

    Serial.println("SENSOR TIMEOUT");
    return;
  }

  Result r = classify(features(v));

  if (now - lastLog >= LOG_MS) {
    lastLog = now;

    printRaw(v);

    Serial.print("Class=");
    Serial.print(cardName(r.card));

    Serial.print(" dBG=");
    Serial.print(r.dBG, 3);

    Serial.print(" dCard=");
    Serial.print(r.best, 3);

    Serial.print(" gap=");
    Serial.println(r.second - r.best, 3);
  }

  // Require multiple consistent detections.

  if (r.card == candidate) {
    if (candidateCount < REQUIRED_STABLE) {
      candidateCount++;
    }
  } else {
    candidate = r.card;
    candidateCount = 1;
  }

  if (candidateCount < REQUIRED_STABLE) {
    return;
  }

  // Re-arm when card is removed.

  if (r.card == CARD_EMPTY) {
    lastTriggered = CARD_UNKNOWN;

    if (mode == MODE_WAIT_EMPTY) {
      enterMode(MODE_READY);
    }

    return;
  }

  if (
    mode == MODE_WAIT_EMPTY ||
    r.card == CARD_UNKNOWN
  ) {
    return;
  }

  // Another colour can immediately trigger
  // another ticket validation.

  if (r.card != lastTriggered) {
    newTicket(r.card);
  }
}

// =====================================================
// 18. RECAPTURE EMPTY BACKGROUND
// =====================================================

void recaptureEmpty() {
  displayText(
    "SAVING EMPTY...",
    "NO CARD PLEASE",
    30, 65, 170
  );

  delay(200);

  if (!captureBackground()) {
    displayText(
      "BG NOT STABLE",
      "TAP TO RETRY",
      255, 40, 0
    );

    delay(550);

    if (mode == MODE_CONFIRM_EMPTY) {
      displayText(
        "REMOVE ALL CARDS",
        "TAP WHEN EMPTY",
        30, 65, 170
      );
    } else {
      enterMode(MODE_WAIT_EMPTY);
    }

    return;
  }

  lastTriggered = CARD_UNKNOWN;
  candidate = CARD_UNKNOWN;
  candidateCount = 0;

  lastScan = millis();

  enterMode(MODE_WAIT_EMPTY);
}

// =====================================================
// 19. TOUCH CONTROLS
// =====================================================

void shortTap() {
  if (mode == MODE_CAL) {
    captureCard();
  }

  else if (
    mode == MODE_CONFIRM_EMPTY ||
    mode == MODE_WAIT_EMPTY ||
    mode == MODE_READY
  ) {
    // Only do this with NO CARD in front of sensor.
    recaptureEmpty();
  }

  else if (mode == MODE_STATIC) {
    effectIndex = (effectIndex + 1) % 5;
    showStatic();
  }

  else if (mode == MODE_AMBIENT) {
    effectIndex = (effectIndex + 1) % 5;
    showAmbient();
  }
}

void longPress() {
  if (
    mode == MODE_CAL ||
    mode == MODE_CONFIRM_EMPTY ||
    mode == MODE_WAIT_EMPTY ||
    mode == MODE_RESET
  ) {
    return;
  }

  lastTriggered = CARD_UNKNOWN;
  candidate = CARD_UNKNOWN;
  candidateCount = 0;

  enterMode(MODE_RESET);
}

void updateTouch() {
  bool current =
    digitalRead(PIN_TOUCH) == HIGH;

  unsigned long now = millis();

  if (current != touchRawPrev) {
    touchRawPrev = current;
    touchChanged = now;
  }

  if (
    current != touchStable &&
    now - touchChanged >= DEBOUNCE_MS
  ) {
    touchStable = current;

    if (current) {
      pressedAt = now;
      longPressDone = false;
    } else if (!longPressDone) {
      shortTap();
    }
  }

  if (
    touchStable &&
    !longPressDone &&
    mode != MODE_CAL &&
    now - pressedAt >= LONG_PRESS_MS
  ) {
    longPressDone = true;
    longPress();
  }
}

// =====================================================
// 20. AMBIENT RGB PREVIEW
// =====================================================

void animateAmbient() {
  unsigned long now = millis();

  if (now - lastAnim < ANIMATION_MS) {
    return;
  }

  lastAnim = now;

  float t =
    (now - stateTime) / 1000.0f;

  float w =
    0.5f + 0.5f * sinf(t * 2.0f);

  int r = 0;
  int g = 0;
  int b = 0;

  switch (effectIndex % 5) {
    case 0:
      // Blue-purple breathing
      r = 20 + int(120 * w);
      b = 140 + int(100 * (1 - w));
      break;

    case 1:
      // Red-white alternating
      if ((now / 500UL) % 2 == 0) {
        r = 245;
      } else {
        r = g = b = 220;
      }
      break;

    case 2:
      // Ocean
      g = 30 + int(115 * w);
      b = 135 + int(90 * (1 - w));
      break;

    case 3:
      // Warm glow
      r = 140 + int(105 * w);
      g = 30 + int(80 * w);
      b = 5;
      break;

    default:
      // Rainbow
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
// 21. OUTPUT STATE UPDATES
// =====================================================

void updateOutputs() {
  unsigned long now = millis();

  if (
    mode == MODE_GRANTED &&
    now - stateTime >= CONFIRM_MS
  ) {
    effectIndex =
      (ticket == CARD_YELLOW) ? 1 : 0;

    if (
      ticket == CARD_YELLOW ||
      ticket == CARD_GREEN
    ) {
      enterMode(MODE_STATIC);
    } else {
      enterMode(MODE_AMBIENT);
    }
  }

  else if (mode == MODE_DENIED) {
    if (now - stateTime >= DENIED_MS) {
      enterMode(MODE_READY);
    }

    else if (now - lastAnim >= ANIMATION_MS) {
      lastAnim = now;

      lcd.setRGB(
        ((now - stateTime) / 270UL) % 2 == 0
          ? 255 : 0,
        0,
        0
      );
    }
  }

  else if (
    mode == MODE_NO_MODE &&
    now - stateTime >= 1400UL
  ) {
    enterMode(MODE_READY);
  }

  else if (
    mode == MODE_RESET &&
    now - stateTime >= RESET_MS
  ) {
    enterMode(MODE_WAIT_EMPTY);
  }

  else if (mode == MODE_AMBIENT) {
    animateAmbient();
  }
}

// =====================================================
// 22. SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  pinMode(PIN_S0, OUTPUT);
  pinMode(PIN_S1, OUTPUT);
  pinMode(PIN_S2, OUTPUT);
  pinMode(PIN_S3, OUTPUT);

  pinMode(PIN_OUT, INPUT);
  pinMode(PIN_TOUCH, INPUT);

  // TCS3200: 20% frequency scaling
  digitalWrite(PIN_S0, HIGH);
  digitalWrite(PIN_S1, LOW);

  Wire.begin();
  lcd.init();

  Serial.println(
    "ColourPass v1.0.3"
  );

  displayText(
    "CLEAR SENSOR",
    "EMPTY FOR 3 SEC",
    30, 65, 170
  );

  delay(3000);

  // Initial automatic background learning

  while (!captureBackground()) {
    displayText(
      "BG SENSOR ERROR",
      "CHECK + RESTART",
      255, 35, 0
    );

    delay(1500);
  }

  calIndex = 0;
  mode = MODE_CAL;

  showCalibration();
}

// =====================================================
// 23. MAIN LOOP
// =====================================================

void loop() {
  updateTouch();
  processColour();
  updateOutputs();
}
