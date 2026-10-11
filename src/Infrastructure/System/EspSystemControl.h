#pragma once

#include <Arduino.h>
#include "Domain/Interfaces/ISystemControl.h"

namespace relay::infra {

class EspSystemControl : public domain::ISystemControl {
public:
    void reboot() override {
        Serial.println("[SYSTEM] Reiniciando ESP32...");
        delay(100);
        ESP.restart();
    }
};

}
