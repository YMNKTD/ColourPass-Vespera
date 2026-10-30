
/*
 ColourPass -> Vespera v1.0
 Arduino MKR WiFi 1010

 Features:
 - 9-colour calibration
 - Background detection
 - Continuous ticket validation
 - White standby lighting
 - Six-colour static mode
 - Six-theme ambient mode
 - Wi-Fi + MQTT control
 - Serial Monitor diagnostic commands

 Touch:
 - Short tap: calibration / effect change
 - Hold 3-5 seconds: background recheck
 - Hold 5+ seconds: restart and recalibrate

 MQTT:
 - Luminaire ID XX
 - 216 RGB bytes
*/

#include <Arduino.h>
#include <WiFiNINA.h>

#include "Config.h"
#include "ColourModels.h"
#include "ColourSensor.h"
#include "TouchInput.h"
#include "LCDView.h"
#include "LightingEngine.h"
#include "VesperaClient.h"

// ============================================
// MODULE INSTANCES
// ============================================

ColourSensor sensor;
TouchInput touch;
LCDView screen;
LightingEngine lighting;
VesperaClient vespera;

// ============================================
// SYSTEM STATE
// ============================================

RunState state = ST_CALIBRATE;

Card currentTicket = CARD_UNKNOWN;
Card candidate = CARD_UNKNOWN;
Card lastTriggered = CARD_UNKNOWN;

uint8_t calibrationIndex = 0;
uint8_t candidateCount = 0;
uint8_t effectIndex = 0;

bool calibrationComplete = false;

// ============================================
// SERIAL MQTT TEST MODE
// ============================================

bool serialTestMode = false;
OutputKind serialTestKind = OUT_OFF;

uint8_t serialTestEffect = 0;
unsigned long serialTestAt = 0;

// ============================================
// BACKGROUND RECALIBRATION
// ============================================

bool resumeCalibrationAfterBg = false;
bool restoreActiveAfterBg = false;

RunState savedActiveState = ST_READY;

// ============================================
// TIMING
// ============================================

unsigned long stateAt = 0;
unsigned long lastScanAt = 0;
unsigned long lastLogAt = 0;
unsigned long lastFrameAt = 0;
unsigned long lastPublishAt = 0;

bool frameDirty = true;
bool forceRender = true;

// ============================================
// NETWORK LCD STATUS
// ============================================

bool lastWiFiConnected = false;

bool networkNoticeActive = false;

unsigned long networkNoticeAt = 0;

// ============================================
// LCD - CALIBRATION
// ============================================

void showCalibration() {
  char line[17];

  snprintf(
    line,
    sizeof(line),
    "CAL %c/9 %.7s",
    char('1' + calibrationIndex),
    cardName(static_cast<Card>(calibrationIndex)));

  screen.show(
    line,
    "PLACE + TAP",
    35, 65, 160);

  Serial.print(F("CALIBRATE: "));
  Serial.println(
    cardName(static_cast<Card>(calibrationIndex)));
}

// ============================================
// LCD - LIGHTING MODE
// ============================================

void showCurrentMode() {
  char line[17];

  if (state == ST_STATIC) {
    snprintf(
      line,
      sizeof(line),
      "STATIC %s",
      LightingEngine::staticName(effectIndex));

    screen.show(
      line,
      "TAP TO CHANGE",
      35, 65, 150);

    Serial.print(F("STATIC: "));
    Serial.println(
      LightingEngine::staticName(effectIndex));

  } else if (state == ST_AMBIENT) {
    snprintf(
      line,
      sizeof(line),
      "AMBIENT %.8s",
      LightingEngine::ambientName(effectIndex));

    screen.show(
      line,
      "TAP TO CHANGE",
      25, 35, 170);

    Serial.print(F("AMBIENT: "));
    Serial.println(
      LightingEngine::ambientName(effectIndex));
  }
}

// ============================================
// STATE MANAGEMENT
// ============================================

void refreshStateScreen() {

  char line[17];

  switch (state) {

    case ST_CALIBRATE:
      showCalibration();
      break;

    case ST_BG_CONFIRM:
      screen.show(
        "REMOVE ALL CARDS",
        "TAP WHEN EMPTY",
        30, 65, 170);
      Serial.println(
        F("BG CHECK: remove all cards, then short tap"));
      break;

    case ST_WAIT_EMPTY:
      screen.show(
        "REMOVE TICKET",
        "WAIT FOR EMPTY",
        30, 65, 170);
      break;

    case ST_READY:
      screen.show(
        "COLOURPASS",
        "SCAN YOUR TICKET",
        30, 65, 170);
      Serial.println(
        F("READY: continuous multi-ticket scanning"));
      break;

    case ST_GRANTED:
      snprintf(
        line,
        sizeof(line),
        "VALID: %s",
        cardName(currentTicket));

      screen.show(
        "ACCESS GRANTED",
        line,
        0, 220, 25);
      break;

    case ST_DENIED:
      snprintf(
        line,
        sizeof(line),
        "INVALID: %s",
        cardName(currentTicket));

      screen.show(
        "ACCESS DENIED",
        line,
        255, 0, 0);
      break;

    case ST_STATIC:
    case ST_AMBIENT:
      showCurrentMode();
      break;

  }

}


// ============================================
// ENTER NEW SYSTEM STATE
// ============================================

void enterState(RunState next) {

  state = next;

  stateAt = millis();

  forceRender = true;

  networkNoticeActive = false;

  refreshStateScreen();
}


// ============================================
// CALIBRATION
// ============================================

void calibrateCurrentCard() {
  if (!sensor.calibrateCard(calibrationIndex)) {
    screen.show(
      "SENSOR TIMEOUT",
      "TAP TO RETRY",
      255, 30, 0);

    Serial.println(
      F("Calibration failed; check positioning/wiring, then tap"));

    return;
  }

  ++calibrationIndex;

  if (calibrationIndex < CARD_COUNT) {
    showCalibration();
  } else {
    calibrationComplete = true;
    resumeCalibrationAfterBg = false;
    enterState(ST_BG_CONFIRM);
  }
}

// ============================================
// EMPTY BACKGROUND CONFIRMATION
// ============================================

void confirmEmptyBackground() {
  screen.show(
    "SAVING EMPTY...",
    "NO CARD PLEASE",
    30, 65, 170);

  if (!sensor.captureBackground()) {
    screen.show(
      "BG UNSTABLE",
      "TAP TO RETRY",
      255, 35, 0);
    return;
  }

  candidate = CARD_UNKNOWN;
  candidateCount = 0;
  lastTriggered = CARD_UNKNOWN;

  lastScanAt = millis();

  if (resumeCalibrationAfterBg) {
    resumeCalibrationAfterBg = false;
    enterState(ST_CALIBRATE);

  } else if (restoreActiveAfterBg) {
    restoreActiveAfterBg = false;
    enterState(savedActiveState);

  } else {
    enterState(ST_WAIT_EMPTY);
  }
}

// ============================================
// BACKGROUND RECHECK - HOLD 3 SECONDS
// ============================================

void requestBackgroundRecheck() {
  serialTestMode = false;

  resumeCalibrationAfterBg =
    (state == ST_CALIBRATE);

  restoreActiveAfterBg =
    (state == ST_STATIC || state == ST_AMBIENT);

  if (restoreActiveAfterBg) {
    savedActiveState = state;
  }

  Serial.println(
    F("3-5s: manual background recheck"));

  enterState(ST_BG_CONFIRM);
}

// ============================================
// REBOOT - HOLD 5 SECONDS
// ============================================

void rebootAndRecalibrate() {
  Serial.println(
    F("5s: rebooting and recalibrating"));

  screen.show(
    "RESTARTING...",
    "RECALIBRATING",
    245, 245, 245);

  lighting.render(OUT_STANDBY, 0, 0);

  if (vespera.connected()) {
    vespera.publish(
      lighting.payload(),
      Config::VESPERA_PAYLOAD_SIZE);
  }

  delay(250);
  Serial.flush();

  NVIC_SystemReset();
}

// ============================================
// TOUCH INPUT
// ============================================

void onTap() {
  if (state == ST_CALIBRATE) {
    calibrateCurrentCard();

  } else if (state == ST_BG_CONFIRM) {
    confirmEmptyBackground();

  } else if (state == ST_STATIC) {
    effectIndex =
      (effectIndex + 1) % LightingEngine::STATIC_COUNT;

    stateAt = millis();
    forceRender = true;
    showCurrentMode();

  } else if (state == ST_AMBIENT) {
    effectIndex =
      LightingEngine::nextAmbientStep(effectIndex);

    stateAt = millis();
    forceRender = true;
    showCurrentMode();
  }
}

// ============================================
// TICKET VALIDATION
// ============================================

void onNewTicket(Card card) {
  // WHITE / BLACK / UNKNOWN:
  // no authorisation and no new lighting action.
  if (
    card == CARD_WHITE || card == CARD_BLACK || card == CARD_UNKNOWN) {
    return;
  }

  currentTicket = card;
  lastTriggered = card;

  Serial.print(F("NEW TICKET: "));
  Serial.println(cardName(card));

  // Invalid cards: red flash, then white standby.
  if (
    card == CARD_RED || card == CARD_ORANGE || card == CARD_PINK) {
    enterState(ST_DENIED);
  }

  // Valid cards: green confirmation.
  else if (
    card == CARD_YELLOW || card == CARD_GREEN || card == CARD_BLUE || card == CARD_PURPLE) {
    enterState(ST_GRANTED);
  }
}

// ============================================
// CONTINUOUS TICKET SCANNING
// ============================================

void scanTicketIfNeeded() {
  if (
    serialTestMode || !calibrationComplete || state == ST_CALIBRATE || state == ST_BG_CONFIRM) {
    return;
  }

  unsigned long now = millis();

  if (
    now - lastScanAt < Config::SCAN_MS) {
    return;
  }

  lastScanAt = now;

  SensorMatch match = sensor.detect();

  if (!match.validRead) {
    candidate = CARD_UNKNOWN;
    candidateCount = 0;
    Serial.println(F("SENSOR TIMEOUT"));
    return;
  }

  // Sensor diagnostic logs.
  if (
    now - lastLogAt >= Config::SERIAL_LOG_MS) {
    lastLogAt = now;

    Serial.print(F("Class="));
    Serial.print(cardName(match.card));

    Serial.print(F(" dBG="));
    Serial.print(match.dBg, 3);

    Serial.print(F(" dCard="));
    Serial.print(match.best, 3);

    Serial.print(F(" gap="));
    Serial.print(
      match.second - match.best,
      3);

    Serial.print(F(" nearest="));
    Serial.println(cardName(match.nearest));
  }

  // Require consecutive matching readings.
  if (match.card == candidate) {
    if (
      candidateCount < Config::STABLE_READS) {
      ++candidateCount;
    }
  } else {
    candidate = match.card;
    candidateCount = 1;
  }

  if (
    candidateCount < Config::STABLE_READS) {
    return;
  }

  // Empty scan area.
  if (match.card == CARD_EMPTY) {
    lastTriggered = CARD_UNKNOWN;

    if (state == ST_WAIT_EMPTY) {
      enterState(ST_READY);
    }

    return;
  }

  if (state == ST_WAIT_EMPTY) return;

  // White and black cards have no response.
  if (
    match.card == CARD_WHITE || match.card == CARD_BLACK) {
    lastTriggered = CARD_UNKNOWN;
    return;
  }

  // Ignore ambiguous readings.
  if (match.card == CARD_UNKNOWN) return;

  // Allow different cards to be validated
  // without restarting the system.
  if (match.card != lastTriggered) {
    onNewTicket(match.card);
  }
}

// ============================================
// SERIAL MONITOR MQTT TEST
// ============================================

void startSerialTest(
  OutputKind kind,
  uint8_t effect,
  const __FlashStringHelper* name) {
  serialTestMode = true;

  serialTestKind = kind;
  serialTestEffect = effect;
  serialTestAt = millis();

  frameDirty = true;
  forceRender = true;

  Serial.print(F("MQTT HOME TEST: "));
  Serial.println(name);

  Serial.println(
    F("Open Lumi web client on luminaire 25; type s for status, x to resume"));
}

void processSerialCommands() {
  while (Serial.available() > 0) {
    char key = static_cast<char>(Serial.read());

    if (key == '1') {
      startSerialTest(
        OUT_AMBIENT,
        6,
        F("BAYERN / red-white static"));

    } else if (key == '2') {
      startSerialTest(
        OUT_AMBIENT,
        0,
        F("BLUEMOOD / dynamic"));

    } else if (key == '3') {
      startSerialTest(
        OUT_AMBIENT,
        1,
        F("RAINBOW / static"));

    } else if (key == '0') {
      startSerialTest(
        OUT_OFF,
        0,
        F("ALL OFF"));

    } else if (key == 'x' || key == 'X') {
      serialTestMode = false;

      candidate = CARD_UNKNOWN;
      candidateCount = 0;
      lastTriggered = CARD_UNKNOWN;

      frameDirty = true;
      forceRender = true;

      Serial.println(
        F("MQTT TEST EXIT: returning to normal ColourPass"));

    } else if (key == 's' || key == 'S') {
      vespera.printStatus();

      Serial.print(F("Calibration complete: "));
      Serial.println(
        calibrationComplete ? F("YES") : F("NO"));

      Serial.print(F("Home test: "));
      Serial.println(
        serialTestMode ? F("ON") : F("OFF"));

    } else if (
      key == '?' || key == 'h' || key == 'H') {
      Serial.println(
        F("1=Bayern 2=BlueMood 3=Rainbow 0=Off x=exit s=status"));
    }
  }
}

// ============================================
// TIMED STATE TRANSITIONS
// ============================================

void updateStateTimers() {
  unsigned long elapsed = millis() - stateAt;

  if (
    state == ST_GRANTED && elapsed >= Config::GRANTED_MS) {
    // Yellow / Green -> Static Mode.
    if (
      currentTicket == CARD_YELLOW || currentTicket == CARD_GREEN) {
      effectIndex =
        (currentTicket == CARD_YELLOW) ? 0 : 1;

      enterState(ST_STATIC);

    } else {
      // Blue -> Blue Mood.
      // Purple -> Rainbow.
      effectIndex =
        (currentTicket == CARD_BLUE) ? 0 : 1;

      enterState(ST_AMBIENT);
    }
  }

  else if (
    state == ST_DENIED && elapsed >= Config::DENIED_MS) {
    // Always return to WHITE standby.
    enterState(ST_READY);
  }
}

// ============================================
// OUTPUT MODE SELECTION
// ============================================

OutputKind activeOutput() {
  if (serialTestMode) {
    return serialTestKind;
  }

  switch (state) {
    case ST_GRANTED:
      return OUT_GRANTED;

    case ST_DENIED:
      return OUT_DENIED;

    case ST_STATIC:
      return OUT_STATIC;

    case ST_AMBIENT:
      return OUT_AMBIENT;

    default:
      return OUT_STANDBY;
  }
}

// ============================================
// LIGHTING AND MQTT UPDATE
// ============================================


void updateLightingAndNetwork() {

  unsigned long now = millis();

  // Maintain Wi-Fi and MQTT connection
  vespera.tick();

  // ============================================
  // CHECK WIFI STATUS
  // ============================================

  bool wifiConnected =
    (WiFi.status() == WL_CONNECTED);

  bool mqttJustConnected =
    vespera.freshlyConnected();

  // Wi-Fi connected, but MQTT not ready yet
  if (
    wifiConnected && !lastWiFiConnected && !mqttJustConnected) {

    screen.show(
      "WIFI CONNECTED",
      "CONNECTING MQTT",
      30, 80, 180);

    networkNoticeActive = true;
    networkNoticeAt = millis();
  }

  lastWiFiConnected = wifiConnected;

  // ============================================
  // MQTT CONNECTION SUCCESS
  // ============================================

  if (mqttJustConnected) {

    frameDirty = true;

    screen.show(
      "MQTT CONNECTED",
      "VESPERA READY",
      0, 200, 40);

    networkNoticeActive = true;
    networkNoticeAt = millis();

    Serial.println(
      F("LCD: MQTT CONNECTED / VESPERA READY"));
  }

  // ============================================
  // RESTORE ORIGINAL LCD AFTER 1.5 SECONDS
  // ============================================

  if (
    networkNoticeActive && millis() - networkNoticeAt >= 1500UL) {

    networkNoticeActive = false;

    // Restore display without resetting timers
    refreshStateScreen();
  }

  OutputKind kind = activeOutput();

  uint8_t effect =
    serialTestMode ? serialTestEffect : effectIndex;

  bool animated =
    LightingEngine::isAnimated(kind, effect);

  bool needsFrame =
    forceRender || (animated && now - lastFrameAt >= Config::FRAME_MS);

  if (needsFrame) {
    unsigned long elapsed = now - (serialTestMode ? serialTestAt : stateAt);

    lighting.render(
      kind,
      effect,
      elapsed);

    frameDirty = true;
    forceRender = false;
    lastFrameAt = now;

    if (
      !serialTestMode && (state == ST_AMBIENT || state == ST_DENIED)) {
      const uint8_t* rgb = lighting.payload();

      screen.setBacklight(
        rgb[0],
        rgb[1],
        rgb[2]);
    }
  }

  // Publish current RGB frame to Luminaire 25.
  if (
    vespera.connected() && (frameDirty || now - lastPublishAt >= Config::MQTT_RESEND_MS)) {
    if (
      vespera.publish(
        lighting.payload(),
        Config::VESPERA_PAYLOAD_SIZE)) {
      frameDirty = false;
      lastPublishAt = millis();
    }
  }
}

// ============================================
// ARDUINO SETUP
// ============================================

void setup() {
  Serial.begin(115200);

  sensor.begin();
  touch.begin();
  screen.begin();
  lighting.begin();

  // Immediately display WHITE standby
  // on the local NeoPixel strip.
  lighting.render(
    OUT_STANDBY,
    0,
    0);

  vespera.begin();

  Serial.println(
    F("ColourPass -> Vespera | ID 25 | sensor tuning + white standby"));

  Serial.println(
    F("Home MQTT tests: 1=Bayern 2=BlueMood 3=Rainbow 0=Off x=exit s=status"));

  Serial.println(
    F("3-5s hold=BG recheck | >=5s hold=reboot (on release)"));

  screen.show(
    "CONNECTING WIFI",
    "CLEAR SENSOR 3S",
    30, 65, 170);

  // Keep MQTT active during startup wait.
  unsigned long clearStartAt = millis();

  while (
    millis() - clearStartAt < 3000UL) {
    updateLightingAndNetwork();
    delay(10);
  }

  // Capture initial empty background.
  while (!sensor.captureBackground()) {
    screen.show(
      "BG SENSOR ERROR",
      "CHECK + RESTART",
      255, 35, 0);

    Serial.println(
      F("Initial background capture failed; retrying"));

    unsigned long retryAt = millis();

    while (
      millis() - retryAt < 1500UL) {
      updateLightingAndNetwork();
      delay(10);
    }
  }

  calibrationIndex = 0;
  enterState(ST_CALIBRATE);
}

// ============================================
// ARDUINO MAIN LOOP
// ============================================

void loop() {
  processSerialCommands();

  TouchEvent event = touch.poll();

  if (event == TOUCH_REBOOT) {
    rebootAndRecalibrate();

  } else if (event == TOUCH_RECHECK) {
    requestBackgroundRecheck();

  } else if (
    event == TOUCH_TAP && !serialTestMode) {
    onTap();
  }

  scanTicketIfNeeded();
  updateStateTimers();
  updateLightingAndNetwork();
}
