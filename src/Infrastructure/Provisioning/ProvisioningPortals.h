#pragma once

#include "Domain/Interfaces/IProvisioningPortal.h"
#include "Infrastructure/Provisioning/ble_provisioning.h"
#include "Infrastructure/Provisioning/WifiApProvisioningPortal.h"

namespace relay::infra {

/**
 * @brief Portal Quick Pairing (BLE GATT + SmartConfig ESP-Touch/AirKiss):
 * Permite recibir tramas JSON, características GATT individuales o paquetes SmartConfig,
 * con soporte de transición a SoftAP VESIS-<ChipID> en una 2ª pulsación larga de 5.0 s.
 */
class BleProvisioningPortal : public domain::IProvisioningPortal {
public:
    bool run(unsigned long timeoutMs) override {
        return runBLEProvisioning(timeoutMs);
    }

    QuickPairingResult runInteractive(unsigned long timeoutMs,
                                      domain::IIndicator& indicator,
                                      domain::IButtonHandler& button,
                                      OfflinePairingTickCallback offlineTickCb = nullptr) {
        return runQuickPairing(timeoutMs, &indicator, &button, std::move(offlineTickCb));
    }
};

/**
 * @brief Portal SoftAP / Cautivo (WiFiManager VESIS-<ChipID>): configuración vía interfaz web WiFi.
 */
class WifiApProvisioningPortalWrapper : public domain::IProvisioningPortal {
public:
    bool run(unsigned long timeoutMs) override {
        return WifiApProvisioningPortal::startPortal(timeoutMs / 1000);
    }
};

} // namespace relay::infra
