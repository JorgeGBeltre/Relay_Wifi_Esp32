#pragma once

#include <Arduino.h>
#include <functional>
#include "ESP32_MQTTv5.h"

extern ESP32_MQTTv5 mqtt;

namespace relay::infra {

void initMQTT(const char* server, uint16_t port,
              const char* clientId, const char* apiKey,
              const char* rootCa, const char* firmwareVersion);

bool connectMQTT();
void disconnectMQTT();
void mqttLoop();
bool isMQTTConnected();

bool publishMQTT(const String& topic, const String& payload, uint8_t qos = 1, bool retain = false);
bool subscribeMQTT(const String& topic, uint8_t qos = 1);

void setMqttMessageSink(std::function<void(const String& topic, const String& payload)> sink);
void setMqttConnectCallback(std::function<void()> cb);

}
