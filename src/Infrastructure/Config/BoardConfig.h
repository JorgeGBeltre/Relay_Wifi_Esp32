#pragma once

#include <cstdint>

namespace relay::board {

constexpr uint8_t RELAY_PIN_1          = 4;
constexpr uint8_t BUTTON_PIN_1         = 0;
constexpr uint8_t WIFI_LED_PIN         = 2;

constexpr uint8_t  DEBOUNCE_TIME_MS      = 50;   // 2 ticks * 25 ms (Sonoff 0x40233431)
constexpr uint16_t LONG_PRESS_TIME_MS    = 5000; // 200 ticks * 25 ms (Sonoff 0x40233388)

constexpr int FAST_BLINK_INTERVAL_MS   = 100;
constexpr int SLOW_BLINK_INTERVAL_MS   = 400;

}
