#pragma once

#include "Domain/Entities/DeviceIdentity.h"
#include "Domain/Entities/SystemMetrics.h"

namespace relay::domain {

class IDeviceRepository {
public:
    virtual ~IDeviceRepository() = default;
    virtual DeviceIdentity identity() = 0;
    virtual SystemMetrics  metrics() = 0;
};

}
