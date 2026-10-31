#include "VesperaClient.h"
#include "Config.h"
#include "arduino_secrets.h"
#include <string.h>

void VesperaClient::begin() {
  mqtt_.setServer(Config::MQTT_HOST, Config::MQTT_PORT);
  mqtt_.setBufferSize(512); // >216-byte binary RGB payload + MQTT header + topic
  mqtt_.setKeepAlive(30);
  mqtt_.setSocketTimeout(3); // Avoid very long MQTT reconnect stalls.
  byte mac[6] = {0};
  WiFi.macAddress(mac);
  snprintf(clientId_, sizeof(clientId_), "ColourPass25_%02X%02X%02X%02X%02X%02X",
           mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  Serial.print(F("Vespera topic: "));
  Serial.println(Config::MQTT_TOPIC);
}

void VesperaClient::tick() {
  unsigned long now = millis();
  uint8_t wifiStatus = WiFi.status();
  if (wifiStatus == WL_NO_MODULE) {
    if (wifiWasConnected_) Serial.println(F("WiFi MODULE UNAVAILABLE"));
    wifiWasConnected_ = false;
    // Do not block the physical ticket reader if the WiFi radio is unavailable.
    return;
  }
  if (wifiStatus != WL_CONNECTED) {
    if (wifiWasConnected_) {
      Serial.println(F("WiFi disconnected; MQTT reconnect pending"));
      mqttEverAttempted_ = false;
    }
    wifiWasConnected_ = false;
    if (!wifiEverAttempted_ || now - wifiAttemptAt_ >= Config::WIFI_RETRY_MS) {
      wifiEverAttempted_ = true;
      wifiAttemptAt_ = now;
      const char* network = SECRET_SSID;
      const char* password = SECRET_PASS;
      // Alternate between two configured networks, if a backup exists.
      #if defined(SECRET_SSID1) && defined(SECRET_PASS1)
      if (SECRET_SSID1[0] != '\0' && (now / Config::WIFI_RETRY_MS) % 2 == 1) {
        network = SECRET_SSID1; password = SECRET_PASS1;
      }
      #endif
      Serial.print(F("WiFi connecting: ")); Serial.println(network);
      // Open Wi-Fi (e.g. Glide SSID) needs the ONE-argument overload.
      // Registration/captive-portal requirements may still prevent internet.
      if (password[0] == '\0') WiFi.begin(network);
      else WiFi.begin(network, password); // May block briefly.
    }
    return;
  }
  if (!wifiWasConnected_) {
    wifiWasConnected_ = true;
    Serial.print(F("WiFi connected. IP: "));
    Serial.println(WiFi.localIP());
    mqttEverAttempted_ = false; // reconnect as soon as Wi-Fi returns
  }
  if (!mqtt_.connected()) {
    if (!mqttEverAttempted_ || now - mqttAttemptAt_ >= Config::MQTT_RETRY_MS) {
      mqttEverAttempted_ = true;
      mqttAttemptAt_ = now;
      wifi_.stop();
      if (mqtt_.connect(clientId_, SECRET_MQTTUSER, SECRET_MQTTPASS)) {
        newlyConnected_ = true;
        Serial.println(F("MQTT connected"));
      } else {
        Serial.print(F("MQTT error code: "));
        Serial.println(mqtt_.state());
      }
    }
    return;
  }
  mqtt_.loop();
}

bool VesperaClient::freshlyConnected() {
  bool event = newlyConnected_;
  newlyConnected_ = false;
  return event;
}

bool VesperaClient::publish(const uint8_t* bytes, unsigned int length) {
  if (!mqtt_.connected() || !bytes || length != Config::VESPERA_PAYLOAD_SIZE) return false;
  bool ok = mqtt_.publish(Config::MQTT_TOPIC, bytes, length);
  lastPublishOk_ = ok;
  if (ok) {
    ++publishCount_;
    lastPublishAt_ = millis();
  }
  unsigned long now = millis();
  if (!publishEverLogged_ || now - lastPublishLogAt_ >= 2000UL || !ok) {
    Serial.print(ok ? F("MQTT publish OK | ") : F("MQTT publish FAILED | "));
    Serial.print(F("bytes=")); Serial.print(length);
    Serial.print(F(" | topic=")); Serial.print(Config::MQTT_TOPIC);
    Serial.print(F(" | RGB[0]="));
    Serial.print(bytes[0]); Serial.print(',');
    Serial.print(bytes[1]); Serial.print(','); Serial.println(bytes[2]);
    publishEverLogged_ = true;
    lastPublishLogAt_ = now;
  }
  return ok;
}

void VesperaClient::printStatus() {
  Serial.println(F("--- ColourPass network status ---"));
  Serial.print(F("WiFi: "));
  Serial.println(WiFi.status() == WL_CONNECTED ? F("CONNECTED") : F("NOT CONNECTED"));
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("IP: ")); Serial.println(WiFi.localIP());
  }
  Serial.print(F("MQTT: "));
  Serial.println(mqtt_.connected() ? F("CONNECTED") : F("NOT CONNECTED"));
  Serial.print(F("MQTT state: ")); Serial.println(mqtt_.state());
  Serial.print(F("Topic: ")); Serial.println(Config::MQTT_TOPIC);
  Serial.print(F("Successful publishes: ")); Serial.println(publishCount_);
  Serial.print(F("Last publish result: "));
  if (publishCount_ == 0) Serial.println(F("NONE"));
  else Serial.println(lastPublishOk_ ? F("OK") : F("FAILED"));
  if (publishCount_ > 0) {
    Serial.print(F("Last successful publish at uptime ms: "));
    Serial.println(lastPublishAt_);
  }
  Serial.println(F("-------------------------------"));
}
