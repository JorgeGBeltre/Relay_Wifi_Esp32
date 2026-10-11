#pragma once

#include <cstdint>
#include "Domain/Interfaces/IRelayController.h"

namespace relay::infra {

class GpioRelay : public domain::IRelayController {
public:
    explicit GpioRelay(uint8_t pin) : pin_(pin) {}

    void init() override;
    void setState(bool on) override;
    bool getState() const override { return state_; }
    uint8_t getPin() const override { return pin_; }

private:
    uint8_t pin_;
    bool state_ = false;
};

}
