#pragma once

#include <cstdint>

namespace relay::domain {

enum class TimerType : uint8_t {
    Once     = 0,
    Repeat   = 1,
    Duration = 2
};

struct ScheduledTimer {
    bool      enabled             = false;
    uint8_t   id                  = 0;
    TimerType type                = TimerType::Once;
    bool      targetAction        = false; // Para Once y Repeat (true = ON, false = OFF)
    uint32_t  targetEpochSec      = 0;     // Para Once: timestamp UTC exacto
    uint8_t   weekdaysMask        = 0;     // Para Repeat: bit0=Dom .. bit6=Sab
    uint8_t   hourUtc             = 0;     // Para Repeat: 0..23
    uint8_t   minuteUtc           = 0;     // Para Repeat: 0..59
    uint32_t  cycleStartEpochSec  = 0;     // Para Duration (Loop Timer)
    uint32_t  cyclePeriodSec      = 0;     // Duración total del ciclo en segundos
    uint32_t  cycleOnDurationSec  = 0;     // Duración del primer tramo en segundos
    bool      startAction         = true;
    bool      endAction           = false;
    uint32_t  lastTriggeredMinute = 0;
};

} // namespace relay::domain

