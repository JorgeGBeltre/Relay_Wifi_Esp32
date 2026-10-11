#pragma once

#include "Application/Serialization/IPayloadSerializer.h"

namespace relay::infra {

class ArduinoJsonPayloadSerializer : public app::IPayloadSerializer {
public:
    std::string formatStatus(const domain::DeviceIdentity& id,
                             const domain::SystemMetrics& metrics,
                             const std::vector<domain::RelayState>& relays,
                             const std::string& timestamp,
                             bool syncCompleted) override;

    std::string formatEvent(const domain::DeviceIdentity& id,
                            const domain::DeviceEvent& event,
                            const std::string& timestamp,
                            bool isConnected) override;

    std::string formatTelemetry(const domain::DeviceIdentity& id,
                                const domain::SystemMetrics& metrics,
                                const std::vector<domain::RelayState>& relays,
                                const std::string& timestamp) override;
};

}
