#pragma once

#include <string>

namespace relay::domain {

class IOtaService {
public:
    virtual ~IOtaService() = default;

    virtual void begin(const std::string& deviceId, const std::string& firmwareVersion) = 0;
    virtual void handle() = 0;
    virtual bool isInProgress() = 0;
};

}
