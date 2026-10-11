#pragma once

namespace relay::domain {

/**
 * @brief Interfaz para portales de aprovisionamiento (BLE, SoftAP, etc.).
 * Bloquea la ejecución hasta recibir configuración o agotar el tiempo límite.
 */
class IProvisioningPortal {
public:
    virtual ~IProvisioningPortal() = default;

    /**
     * @brief Ejecuta el portal de aprovisionamiento.
     * @param timeoutMs Tiempo máximo de espera en milisegundos.
     * @return true si se recibió y guardó configuración válida, false en caso de timeout.
     */
    virtual bool run(unsigned long timeoutMs) = 0;
};

} // namespace relay::domain
