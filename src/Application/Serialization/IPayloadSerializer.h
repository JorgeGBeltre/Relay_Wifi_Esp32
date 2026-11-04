#pragma once

#include <cstdint>
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
                                     bool syncCompleted,
                                     uint32_t stateVersion = 0,
                                     const std::string& lastSource = "") = 0;

    virtual std::string formatEvent(const domain::DeviceIdentity& id,
                                    const domain::DeviceEvent& event,
                                    const std::string& timestamp,
                                    bool isConnected) = 0;

    virtual std::string formatTelemetry(const domain::DeviceIdentity& id,
                                        const domain::SystemMetrics& metrics,
                                        const std::vector<domain::RelayState>& relays,
                                        const std::string& timestamp,
                                        uint32_t stateVersion = 0) = 0;
};

}
