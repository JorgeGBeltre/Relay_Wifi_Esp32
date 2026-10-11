#pragma once

#include <cstdint>

namespace relay::domain {

class IRelayController {
public:
    virtual ~IRelayController() = default;
    virtual void init() = 0;
    virtual void setState(bool on) = 0;
    virtual bool getState() const = 0;
    virtual uint8_t getPin() const = 0;
};

}
