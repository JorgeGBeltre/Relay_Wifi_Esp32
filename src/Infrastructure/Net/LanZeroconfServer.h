#pragma once

#include <WebServer.h>
#include <cstdint>
#include <string>
#include "Application/Services/RelayService.h"
#include "Application/Services/TelemetryService.h"
#include "Application/Services/TimerSchedulerService.h"
#include "Domain/Interfaces/IConfigRepository.h"
#include "Domain/Interfaces/IIndicator.h"

namespace relay::infra {

/**
 * @brief Servidor HTTP LAN local en el puerto 8081 (/zeroconf/*) + mDNS
 * Soporta control en red local (texto claro JSON y sobre cifrado AES-128-CBC con MD5(deviceKey)),
 * sincronizado en tiempo real con RelayService y TelemetryService (MQTT 5.0).
 */
class LanZeroconfServer {
public:
    LanZeroconfServer(app::RelayService& relays,
                      app::TelemetryService& telemetry,
                      app::TimerSchedulerService& timerScheduler,
                      domain::IIndicator& indicator,
                      domain::IConfigRepository& configRepo,
                      const std::string& activeDeviceId,
                      const std::string& relayDeviceId,
                      uint16_t port = 8081);

    void loop(bool wifiConnected);
    void notifyMdnsStateChange();

private:
    void startServer();
    void stopServer();
    void registerEndpoint(const char* uri);
    void dispatchZeroconf(const char* uri, const String& rawBody, char* respBuf, size_t respBufSize);

    app::RelayService& relays_;
    app::TelemetryService& telemetry_;
    app::TimerSchedulerService& timerScheduler_;
    domain::IIndicator& indicator_;
    domain::IConfigRepository& configRepo_;
    std::string activeDeviceId_;
    std::string relayDeviceId_;
    uint16_t port_;

    WebServer server_;
    bool running_ = false;
    bool otaUnlocked_ = false;
    uint32_t sequence_ = 1;
};

} // namespace relay::infra

