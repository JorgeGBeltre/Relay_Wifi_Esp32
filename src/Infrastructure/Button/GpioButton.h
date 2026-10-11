#pragma once

#include <cstdint>
#include "Domain/Interfaces/IButtonHandler.h"

namespace relay::infra {

class GpioButton : public domain::IButtonHandler {
public:
    GpioButton(uint8_t pin, uint16_t debounceMs = 50, uint16_t longPressMs = 3000)
        : pin_(pin), debounceMs_(debounceMs), longPressMs_(longPressMs) {}

    void init() override;
    domain::ButtonEvent update() override;
    uint8_t getPin() const override { return pin_; }

private:
    uint8_t pin_;
    uint16_t debounceMs_;
    uint16_t longPressMs_;

    bool lastState_ = true;
    bool isPressed_ = false;
    unsigned long pressStartTime_ = 0;
};

}
