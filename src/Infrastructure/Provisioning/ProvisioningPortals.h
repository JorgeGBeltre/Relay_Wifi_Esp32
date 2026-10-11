#pragma once

#include "Domain/Interfaces/IProvisioningPortal.h"
#include "Infrastructure/Provisioning/ble_provisioning.h"
#include "Infrastructure/Provisioning/WifiApProvisioningPortal.h"

namespace relay::infra {

/**
 * @brief Portal BLE (NimBLE): permite recibir tramas JSON o características GATT individuales
 * para configurar credenciales WiFi y MQTT e identificar el dispositivo.
 */
class BleProvisioningPortal : public domain::IProvisioningPortal {
public:
    bool run(unsigned long timeoutMs) override {
        return runBLEProvisioning(timeoutMs);
    }
};

/**
 * @brief Portal SoftAP / Cautivo (WiFiManager): configuración vía interfaz web WiFi.
 */
class WifiApProvisioningPortalWrapper : public domain::IProvisioningPortal {
public:
    bool run(unsigned long timeoutMs) override {
        WifiApProvisioningPortal::startPortal(timeoutMs / 1000);
        return true;
    }
};

} // namespace relay::infra
