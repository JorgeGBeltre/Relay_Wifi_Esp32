#pragma once

#include <Arduino.h>
#include <cstdint>
#include <functional>
#include "Domain/Interfaces/IButtonHandler.h"
#include "Domain/Interfaces/IIndicator.h"

enum class QuickPairingResult : uint8_t {
    Provisioned       = 0, // Credenciales recibidas por BLE GATT o por SmartConfig (ESP-Touch/AirKiss)
    SwitchToSoftAp    = 1, // 2ª pulsación larga de 5.0 s -> Pasar a SoftAP Compatible (VESIS-<ChipID>)
    CancelledByButton = 2, // Pulsación corta -> Salir de modo emparejamiento
    Timeout           = 3  // Tiempo agotado sin recibir configuración
};

using OfflinePairingTickCallback = std::function<void(bool isPrimaryShortPress)>;

void initBLEProvisioning();
bool runBLEProvisioning(unsigned long timeoutMs = 60000);

QuickPairingResult runQuickPairing(unsigned long timeoutMs = 180000,
                                   relay::domain::IIndicator* indicator = nullptr,
                                   relay::domain::IButtonHandler* button = nullptr,
                                   OfflinePairingTickCallback offlineTickCb = nullptr);
