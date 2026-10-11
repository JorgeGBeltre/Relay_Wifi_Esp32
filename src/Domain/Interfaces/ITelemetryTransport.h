#pragma once

#include <functional>
#include <string>

namespace relay::domain {

class ITelemetryTransport {
public:
    using MessageHandler =
        std::function<void(const std::string& topic, const std::string& payload)>;

    virtual ~ITelemetryTransport() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    virtual bool publish(const std::string& topic, const std::string& payload) = 0;
    virtual void subscribe(const std::string& topic) = 0;
    virtual void onMessage(MessageHandler handler) = 0;
    virtual void onConnect(std::function<void()> cb) = 0;

    virtual void loop() = 0;
};

}
