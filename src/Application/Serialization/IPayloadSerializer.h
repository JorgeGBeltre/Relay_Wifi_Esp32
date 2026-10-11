#pragma once

#include <string>
#include <vector>
#include "Domain/Entities/DeviceEvent.h"
#include "Domain/Entities/DeviceIdentity.h"
#include "Domain/Entities/RelayState.h"
#include "Domain/Entities/SystemMetrics.h"

namespace relay::app {

class IPayloadSerializer {
public:
    virtual ~IPayloadSerializer() = default;

    virtual std::string formatStatus(const domain::DeviceIdentity& id,
                                     const domain::SystemMetrics& metrics,
                                     const std::vector<domain::RelayState>& relays,
                                     const std::string& timestamp,
                                     bool syncCompleted) = 0;

    virtual std::string formatEvent(const domain::DeviceIdentity& id,
                                    const domain::DeviceEvent& event,
                                    const std::string& timestamp,
                                    bool isConnected) = 0;

    virtual std::string formatTelemetry(const domain::DeviceIdentity& id,
                                        const domain::SystemMetrics& metrics,
                                        const std::vector<domain::RelayState>& relays,
                                        const std::string& timestamp) = 0;
};

}
