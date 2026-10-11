#pragma once

#include <string>

namespace relay::domain {

/**
 * @brief Entidad pura de dominio que contiene la configuración del dispositivo.
 * Desacoplada de frameworks o hardware para permitir compilación y pruebas en host.
 * Incluye campos para identificación de hardware (Device ID + Device Key) y vinculación a cuenta (Bind Token).
 */
struct DeviceConfig {
    std::string wifiSsid;
    std::string wifiPass;

    std::string mqttBroker;
    int         mqttPort = 8883;
    std::string mqttUser;
    std::string mqttPass;

    std::string deviceId;
    std::string deviceKey;    // Clave privada de hardware / Device Key para autenticación mutua
    std::string bindToken;    // Token de registro / reclamo emitido por la app para enlazar a la cuenta
    std::string otaHmacKey;

    bool        provisioned = false;
};

} // namespace relay::domain

