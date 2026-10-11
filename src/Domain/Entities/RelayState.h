#pragma once

#include <cstdint>
#include <string>

namespace relay::domain {

struct RelayState {
    std::string deviceId;
    uint8_t     pin = 0;
    bool        currentState = false;
    bool        desiredState = false;
    std::string topic;
    bool        stateChanged = false;
};

}
