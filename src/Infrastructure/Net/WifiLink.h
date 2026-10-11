#pragma once

#include <string>
#include "Domain/Interfaces/INetworkLink.h"

namespace relay::infra {

class WifiLink : public domain::INetworkLink {
public:
    explicit WifiLink(unsigned long connectTimeoutMs = 5000)
        : connectTimeoutMs_(connectTimeoutMs) {}

    bool connect() override;
    bool isConnected() override;
    int  rssi() override;
    std::string ipAddress() override;
    void loop() override;

private:
    unsigned long connectTimeoutMs_;
    unsigned long lastReconnectAttempt_ = 0;
};

}
