#include "Infrastructure/System/EspDeviceRepository.h"
#include <Arduino.h>
#include <WiFi.h>

namespace relay::infra {

domain::DeviceIdentity EspDeviceRepository::identity() {
    domain::DeviceIdentity id;
    uint64_t chipID = ESP.getEfuseMac();
    char chipIdStr[17];
    snprintf(chipIdStr, sizeof(chipIdStr), "%04X%08X",
             (uint16_t)(chipID >> 32), (uint32_t)chipID);

    id.chipId = chipIdStr;
    id.chipType = ESP.getChipModel();
    id.macAddress = WiFi.macAddress().c_str();
    id.firmwareVersion = firmwareVersion_;
    id.clientId = id.chipId;

    return id;
}

domain::SystemMetrics EspDeviceRepository::metrics() {
    domain::SystemMetrics m;
    m.rssi = WiFi.RSSI();
    m.freeHeap = ESP.getFreeHeap();
    m.uptime = millis();
    m.wifiConnected = (WiFi.status() == WL_CONNECTED);
    m.mqttConnected = false;
    return m;
}

}
