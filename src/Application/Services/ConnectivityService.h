#pragma once

#include "Domain/Interfaces/ILogger.h"
#include "Domain/Interfaces/INetworkLink.h"
#include "Domain/Interfaces/ITelemetryTransport.h"

namespace relay::app {

class ConnectivityService {
public:
    ConnectivityService(domain::INetworkLink& wifi,
                        domain::ITelemetryTransport& transport,
                        domain::ILogger& log)
        : wifi_(wifi), transport_(transport), log_(log) {}

    bool connect();
    bool isOnline();
    bool ensureConnected();

    domain::ITelemetryTransport& transport() { return transport_; }
    domain::INetworkLink& network() { return wifi_; }

    int activeRssi();
    std::string activeIp();

private:
    domain::INetworkLink&        wifi_;
    domain::ITelemetryTransport& transport_;
    domain::ILogger&             log_;
};

}
