
#pragma once
#include <Arduino.h>

namespace Config {
  // Existing ColourPass wiring. Do not move these pins when upgrading.
  constexpr uint8_t SENSOR_S0 = 0;
  constexpr uint8_t SENSOR_S1 = 1;
  constexpr uint8_t SENSOR_S2 = 2;
  constexpr uint8_t SENSOR_S3 = 3;
  constexpr uint8_t SENSOR_OUT = 4;
  constexpr uint8_t TOUCH_PIN = 5;
  constexpr uint8_t NEOPIXEL_PIN = 6;
  constexpr uint8_t LOCAL_PIXELS = 8;
  constexpr uint8_t LOCAL_BRIGHTNESS = 40;
  constexpr uint8_t STANDBY_WHITE = 180; // remote RGB; local strip is brightness-limited

  // LCD uses MKR hardware I2C (SDA D11, SCL D12).
  constexpr unsigned long TOUCH_DEBOUNCE_MS = 40UL;
  constexpr unsigned long BG_HOLD_MS = 3000UL;
  constexpr unsigned long REBOOT_HOLD_MS = 5000UL;
  constexpr unsigned long SCAN_MS = 150UL; // faster checks; 3 consecutive matching readings still required
  constexpr unsigned long GRANTED_MS = 2000UL;
  constexpr unsigned long DENIED_MS = 1900UL;
  constexpr unsigned long FRAME_MS = 140UL;
  constexpr unsigned long MQTT_RESEND_MS = 2000UL;
  constexpr unsigned long WIFI_RETRY_MS = 15000UL;
  constexpr unsigned long MQTT_RETRY_MS = 5000UL;
  constexpr unsigned long SERIAL_LOG_MS = 1000UL;
  constexpr int STABLE_READS = 3;

  // Experimental colour recognition thresholds.
  constexpr float MIN_BG_DISTANCE = 0.12f;
  constexpr float MAX_CARD_DISTANCE = 0.25f;
  constexpr float BG_ADVANTAGE = 0.055f;
  constexpr float MIN_SECOND_GAP = 0.012f;

  // Vespera settings.
  constexpr char MQTT_HOST[] = "0000"; // Change before use
  constexpr uint16_t MQTT_PORT = 0000; // Change before use
  constexpr char MQTT_TOPIC[] = "0000"; // Change before use
  constexpr uint8_t VESPERA_COLUMNS = 12;
  constexpr uint8_t VESPERA_ROWS = 6;
  constexpr int VESPERA_PAYLOAD_SIZE =
      VESPERA_COLUMNS * VESPERA_ROWS * 3; // 216 bytes
}
