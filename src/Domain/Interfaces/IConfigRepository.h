#pragma once

#include "Domain/Entities/DeviceConfig.h"

namespace relay::domain {

/**
 * @brief Interfaz para almacenamiento y recuperación de configuración de credenciales.
 */
class IConfigRepository {
public:
    virtual ~IConfigRepository() = default;

    virtual void begin() = 0;
    virtual bool load(DeviceConfig& out) = 0;
    virtual bool save(const DeviceConfig& cfg) = 0;
    virtual bool erase() = 0;
    virtual bool isProvisioned() = 0;
};

} // namespace relay::domain
