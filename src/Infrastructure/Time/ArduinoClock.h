#pragma once

#include <Arduino.h>
#include "Domain/Interfaces/IClock.h"

namespace relay::infra {

class ArduinoClock : public domain::IClock {
public:
    unsigned long nowMs() override {
        return millis();
    }

    void sleepMs(unsigned long ms) override {
        delay(ms);
    }
};

}
