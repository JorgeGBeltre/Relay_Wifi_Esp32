#pragma once

#include <string>

namespace relay::domain {

struct DeviceEvent {
    std::string eventName;
    std::string description;
    std::string state;
};

}
