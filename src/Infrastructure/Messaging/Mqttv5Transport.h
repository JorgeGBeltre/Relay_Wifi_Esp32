#pragma once

#include "Domain/Interfaces/ITelemetryTransport.h"

namespace relay::infra {

class Mqttv5Transport : public domain::ITelemetryTransport {
public:
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    bool publish(const std::string& topic, const std::string& payload) override;
    void subscribe(const std::string& topic) override;
    void onMessage(MessageHandler handler) override;
    void onConnect(std::function<void()> cb) override;
    void loop() override;

private:
    MessageHandler handler_;
    std::function<void()> onConnectCb_;
};

}
