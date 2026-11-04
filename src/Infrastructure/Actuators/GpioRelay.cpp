#include "Infrastructure/Actuators/GpioRelay.h"
#include <Arduino.h>

namespace relay::infra {

void GpioRelay::init() {
    pinMode(pin_, INPUT | OUTPUT);
    digitalWrite(pin_, LOW);
    state_ = (digitalRead(pin_) == HIGH);
}

void GpioRelay::setState(bool on) {
    digitalWrite(pin_, on ? HIGH : LOW);
    state_ = (digitalRead(pin_) == HIGH);
}

bool GpioRelay::readHardwareState() const {
    return digitalRead(pin_) == HIGH;
}

}
