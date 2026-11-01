#pragma once

#include <cstddef>
#include <cstdint>
#include "Domain/Entities/DeviceConfig.h"
#include "Domain/Entities/ScheduledTimer.h"

namespace relay::domain {

/**
 * @brief Interfaz para almacenamiento y recuperación de configuración y estado persistente (NVS).
 */
class IConfigRepository {
public:
    virtual ~IConfigRepository() = default;

    virtual void begin() = 0;
    virtual bool load(DeviceConfig& out) = 0;
    virtual bool save(const DeviceConfig& cfg) = 0;
    virtual bool erase() = 0;
    virtual bool isProvisioned() = 0;

    // Persistencia granular para evitar reescribir toda la NVS en cada conmutación del relé
    virtual bool saveLastRelayState(bool state) { (void)state; return false; }
    virtual bool saveStartupMode(StartupMode mode) { (void)mode; return false; }
    virtual bool savePulseConfig(bool enabled, uint32_t widthMs) { (void)enabled; (void)widthMs; return false; }
    virtual bool saveSledOnline(bool enabled) { (void)enabled; return false; }
    virtual bool saveTimers(const ScheduledTimer* timers, size_t count) { (void)timers; (void)count; return false; }
    virtual size_t loadTimers(ScheduledTimer* timers, size_t maxCount) { (void)timers; (void)maxCount; return 0; }
};

} // namespace relay::domain
