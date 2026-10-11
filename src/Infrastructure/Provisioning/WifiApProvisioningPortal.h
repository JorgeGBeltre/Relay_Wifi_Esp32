#pragma once

#include <Arduino.h>
#include <WiFiManager.h>

namespace relay::infra {

class WifiApProvisioningPortal {
public:
  static void startPortal(uint32_t timeoutSec = 180) {
    WiFiManager wifiManager;
    uint64_t chipID = ESP.getEfuseMac();
    String apSSID = "VESIS-" + String((uint16_t)(chipID >> 32), DEC) +
                    String((uint32_t)chipID, DEC);
    const char *apPassword = "12345678";

    Serial.printf("[PROVISION] Iniciando Access Point SoftAP: %s (Clave: %s)\n",
                  apSSID.c_str(), apPassword);

    wifiManager.setConfigPortalTimeout(timeoutSec);
    wifiManager.startConfigPortal(apSSID.c_str(), apPassword);

    Serial.println("[PROVISION] Portal finalizado. Reiniciando...");
    delay(100);
    ESP.restart();
  }
};

} // namespace relay::infra
