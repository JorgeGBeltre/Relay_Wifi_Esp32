#include "Infrastructure/Actuators/GpioRelay.h"
#include <Arduino.h>

namespace relay::infra {

void GpioRelay::init() {
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
    state_ = false;
}

void GpioRelay::setState(bool on) {
    state_ = on;
    digitalWrite(pin_, on ? HIGH : LOW);
}

}
