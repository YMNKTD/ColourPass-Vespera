#pragma once
#include <Arduino.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>

class VesperaClient {
public:
  VesperaClient() : mqtt_(wifi_) {}
  void begin();
  void tick();
  bool connected() { return mqtt_.connected(); }
  bool publish(const uint8_t* bytes, unsigned int length);
  bool freshlyConnected(); // true once after each successful MQTT connection
  bool takeWiFiFailure();
  bool takeMqttFailure();
  void printStatus(); // Serial Monitor: network, broker, last publish
private:
  WiFiClient wifi_;
  PubSubClient mqtt_;
  char clientId_[48] = {0};
  unsigned long wifiAttemptAt_ = 0;
  unsigned long mqttAttemptAt_ = 0;
  bool wifiEverAttempted_ = false;
  bool mqttEverAttempted_ = false;
  bool newlyConnected_ = false;
  bool wifiWasConnected_ = false;
  bool wifiFailurePending_ = false;
  bool mqttFailurePending_ = false;
  bool wifiModuleMissingReported_ = false;
  bool publishEverLogged_ = false;
  unsigned long lastPublishLogAt_ = 0;
  unsigned long lastPublishAt_ = 0;
  unsigned long publishCount_ = 0;
  bool lastPublishOk_ = false;
};
