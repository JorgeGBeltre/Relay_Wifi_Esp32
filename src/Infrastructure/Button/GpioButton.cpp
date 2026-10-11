#include "Infrastructure/Button/GpioButton.h"
#include <Arduino.h>

namespace relay::infra {

void GpioButton::init() {
    pinMode(pin_, INPUT_PULLUP);
    lastState_ = digitalRead(pin_);
    isPressed_ = false;
    pressStartTime_ = 0;
}

domain::ButtonEvent GpioButton::update() {
    bool currentState = digitalRead(pin_);
    unsigned long now = millis();
    domain::ButtonEvent result = domain::ButtonEvent::None;

    if (currentState == LOW && lastState_ == HIGH) {
        pressStartTime_ = now;
        isPressed_ = true;
    }
    else if (currentState == HIGH && lastState_ == LOW && isPressed_) {
        unsigned long duration = now - pressStartTime_;
        isPressed_ = false;

        if (duration >= longPressMs_) {
            result = domain::ButtonEvent::LongPress;
        } else if (duration >= debounceMs_) {
            result = domain::ButtonEvent::ShortPress;
        }
    }

    lastState_ = currentState;
    return result;
}

}
