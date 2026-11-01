#pragma once

#include <cstdint>
#include <string>

namespace relay::domain {

class ITimeProvider {
public:
    virtual ~ITimeProvider() = default;
    virtual bool sync() = 0;
    virtual std::string isoTimestamp() = 0;
    virtual bool isSynchronized() = 0;
    virtual uint32_t epochSeconds() { return 0; }
};

}
