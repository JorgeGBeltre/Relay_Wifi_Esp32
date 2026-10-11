#pragma once

namespace relay::domain {

class ISystemControl {
public:
    virtual ~ISystemControl() = default;
    virtual void reboot() = 0;
};

}
