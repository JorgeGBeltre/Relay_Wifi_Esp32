#pragma once

#include <string>

namespace relay::domain {

struct DeviceIdentity {
    std::string chipId;
    std::string chipType;
    std::string macAddress;
    std::string firmwareVersion;
    std::string clientId;
};

}
