#pragma once

#include <string>
#include "Domain/Interfaces/INetworkLink.h"
#include "Domain/Entities/DeviceConfig.h"

namespace relay::infra {

class WifiLink : public domain::INetworkLink {
public:
    explicit WifiLink(unsigned long connectTimeoutMs = 15000)
        : connectTimeoutMs_(connectTimeoutMs) {}

    explicit WifiLink(const domain::DeviceConfig& config, unsigned long connectTimeoutMs = 15000)
        : ssid_(config.wifiSsid), pass_(config.wifiPass), connectTimeoutMs_(connectTimeoutMs) {}

    void setCredentials(const std::string& ssid, const std::string& pass) {
        ssid_ = ssid;
        pass_ = pass;
    }

    bool connect() override;
    bool isConnected() override;
    int  rssi() override;
    std::string ipAddress() override;
    void loop() override;

private:
    std::string ssid_;
    std::string pass_;
    unsigned long connectTimeoutMs_;
    unsigned long lastReconnectAttempt_ = 0;
};

} // namespace relay::infra
