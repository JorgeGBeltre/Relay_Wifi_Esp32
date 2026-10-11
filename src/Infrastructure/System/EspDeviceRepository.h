#pragma once

#include "Domain/Interfaces/IDeviceRepository.h"

namespace relay::infra {

class EspDeviceRepository : public domain::IDeviceRepository {
public:
    explicit EspDeviceRepository(const std::string& firmwareVersion)
        : firmwareVersion_(firmwareVersion) {}

    domain::DeviceIdentity identity() override;
    domain::SystemMetrics  metrics() override;

private:
    std::string firmwareVersion_;
};

}
