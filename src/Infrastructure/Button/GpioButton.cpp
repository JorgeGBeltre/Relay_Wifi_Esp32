#include "Infrastructure/Button/GpioButton.h"
#include <Arduino.h>

namespace relay::infra {

void GpioButton::init() {
    pinMode(pin_, INPUT_PULLUP);
    pressTicks_ = 0;
    releaseTicks_ = 0;
    const unsigned long now = millis();
    if (digitalRead(pin_) == LOW) {
        // Si el botón ya está presionado al inicializar (p. ej. tras disparar LongPress de 5 s),
        // marcar longPressFired_ = true para que al soltarlo no dispare un ShortPress espurio.
        samplingActive_ = true;
        longPressFired_ = true;
        lastSampleMs_ = now;
    } else {
        samplingActive_ = false;
        longPressFired_ = false;
        lastSampleMs_ = now;
    }
}

bool GpioButton::isPhysicallyPressed() const {
    return digitalRead(pin_) == LOW;
}

domain::ButtonEvent GpioButton::update() {
    const unsigned long now = millis();

    if (digitalRead(pin_) == LOW && !samplingActive_) {
        samplingActive_ = true;
        pressTicks_ = 0;
        releaseTicks_ = 0;
        longPressFired_ = false;
        lastSampleMs_ = now - SAMPLE_TICK_MS;
    }

    if (!samplingActive_) {
        return domain::ButtonEvent::None;
    }

    const unsigned long elapsedMs = now - lastSampleMs_;
    if (elapsedMs < SAMPLE_TICK_MS) {
        return domain::ButtonEvent::None;
    }
    uint32_t elapsedTicks = static_cast<uint32_t>(elapsedMs / SAMPLE_TICK_MS);
    if (elapsedTicks > 4U) {
        elapsedTicks = 4U;
    }
    lastSampleMs_ = now;

    const uint32_t debounceTicks = (debounceMs_ >= SAMPLE_TICK_MS) ? (debounceMs_ / SAMPLE_TICK_MS) : 2U;
    const uint32_t longPressTicks = (longPressMs_ >= SAMPLE_TICK_MS) ? (longPressMs_ / SAMPLE_TICK_MS) : 200U;

    const bool pinLow = (digitalRead(pin_) == LOW);
    if (pinLow) {
        releaseTicks_ = 0;
        pressTicks_ += elapsedTicks;
        if (pressTicks_ >= longPressTicks && !longPressFired_) {
            longPressFired_ = true;
            return domain::ButtonEvent::LongPress;
        }
        return domain::ButtonEvent::None;
    }

    // Botón en nivel HIGH (liberado)
    releaseTicks_ += elapsedTicks;
    if (releaseTicks_ >= debounceTicks) {
        const bool validShortPress = (pressTicks_ >= debounceTicks && !longPressFired_);
        samplingActive_ = false;
        pressTicks_ = 0;
        releaseTicks_ = 0;
        longPressFired_ = false;
        if (validShortPress) {
            return domain::ButtonEvent::ShortPress;
        }
    }

    return domain::ButtonEvent::None;
}

}
