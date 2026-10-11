#pragma once

#include <Arduino.h>
#include "Domain/Interfaces/IIndicator.h"

namespace relay::infra {

class GpioIndicator : public domain::IIndicator {
public:
    explicit GpioIndicator(uint8_t pin, int fastBlinkMs = 100, int slowBlinkMs = 400)
        : pin_(pin), fastBlinkMs_(fastBlinkMs), slowBlinkMs_(slowBlinkMs) {
        pinMode(pin_, OUTPUT);
        digitalWrite(pin_, LOW);
    }

    void show(domain::IndicatorStatus status) override {
        status_ = status;
        if (status_ == domain::IndicatorStatus::Operational) {
            digitalWrite(pin_, HIGH);
            ledState_ = true;
        }
    }

    void update() override {
        if (status_ == domain::IndicatorStatus::Operational) {
            digitalWrite(pin_, HIGH);
            return;
        }

        unsigned long now = millis();
        int interval = (status_ == domain::IndicatorStatus::Disconnected) ? fastBlinkMs_ : slowBlinkMs_;

        if (now - lastToggle_ >= (unsigned long)interval) {
            lastToggle_ = now;
            ledState_ = !ledState_;
            digitalWrite(pin_, ledState_ ? HIGH : LOW);
        }
    }

private:
    uint8_t pin_;
    int fastBlinkMs_;
    int slowBlinkMs_;
    domain::IndicatorStatus status_ = domain::IndicatorStatus::Disconnected;
    unsigned long lastToggle_ = 0;
    bool ledState_ = false;
};

}
