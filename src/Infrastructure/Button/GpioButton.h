#pragma once

#include <cstdint>
#include "Domain/Interfaces/IButtonHandler.h"

namespace relay::infra {

// Máquina de estados del botón físico:
// - Muestreo periódico de 25 ms por tick
// - Filtro antirrebote de 2 ticks (50 ms) tanto en presión como en liberación
// - Disparo inmediato de LongPress al alcanzar 200 ticks (5,000 ms) sin esperar a que el usuario suelte el botón
class GpioButton : public domain::IButtonHandler {
public:
    static constexpr unsigned long SAMPLE_TICK_MS = 25;

    GpioButton(uint8_t pin, uint16_t debounceMs = 50, uint16_t longPressMs = 5000)
        : pin_(pin), debounceMs_(debounceMs), longPressMs_(longPressMs) {}

    void init() override;
    domain::ButtonEvent update() override;
    uint8_t getPin() const override { return pin_; }
    bool isPhysicallyPressed() const;

private:
    uint8_t pin_;
    uint16_t debounceMs_;
    uint16_t longPressMs_;

    unsigned long lastSampleMs_ = 0;
    uint32_t pressTicks_ = 0;
    uint32_t releaseTicks_ = 0;
    bool samplingActive_ = false;
    bool longPressFired_ = false;
};

}
