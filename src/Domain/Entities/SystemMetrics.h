#pragma once

#include <cstdint>

namespace relay::domain {

struct SystemMetrics {
    int           rssi = 0;
    uint32_t      freeHeap = 0;
    unsigned long uptime = 0;
    bool          wifiConnected = false;
    bool          mqttConnected = false;
};

}
