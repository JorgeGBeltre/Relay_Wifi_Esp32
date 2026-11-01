#pragma once

namespace relay::domain {

enum class IndicatorStatus {
    Disconnected,   // Buscando Wi-Fi (1000 ms ON / 1000 ms OFF)
    Connecting,     // Wi-Fi conectado, conectando a MQTTv5 (200 ms ON / 1800 ms OFF)
    QuickPairing,   // Modo Quick Pairing BLE + SmartConfig (2 destellos rápidos + pausa: 100/100/100/700 ms)
    CompatibleAp,   // Modo SoftAP VESIS-<ChipID> (Parpadeo rápido continuo: 100 ms ON / 100 ms OFF)
    OtaProgress,    // Descarga OTA en curso (3 destellos rápidos + pausa)
    Operational,    // Operacional (Encendido fijo si sledOnline=true, Apagado si sledOnline=false)
    Error
};

class IIndicator {
public:
    virtual ~IIndicator() = default;
    virtual void show(IndicatorStatus status) = 0;
    virtual void update() = 0;
    virtual void setSledOnline(bool enabled) { (void)enabled; }
    virtual bool isSledOnline() const { return true; }
};

}
