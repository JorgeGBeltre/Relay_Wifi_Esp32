#pragma once

namespace relay::domain {

enum class IndicatorStatus {
    Disconnected,
    Connecting,
    Operational,
    Error
};

class IIndicator {
public:
    virtual ~IIndicator() = default;
    virtual void show(IndicatorStatus status) = 0;
    virtual void update() = 0;
};

}
