#pragma once

namespace relay::domain {

class IClock {
public:
    virtual ~IClock() = default;
    virtual unsigned long nowMs() = 0;
    virtual void sleepMs(unsigned long ms) = 0;
};

}
