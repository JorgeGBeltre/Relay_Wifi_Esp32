#pragma once

#include <string>

namespace relay::domain {

class INetworkLink {
public:
    virtual ~INetworkLink() = default;
    virtual bool connect() = 0;
    virtual bool isConnected() = 0;
    virtual int  rssi() = 0;
    virtual std::string ipAddress() = 0;
    virtual void loop() = 0;
};

}
