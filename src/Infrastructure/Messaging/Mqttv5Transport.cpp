#include "Infrastructure/Messaging/Mqttv5Transport.h"
#include "Infrastructure/Messaging/mqtt_client.h"

namespace relay::infra {

bool Mqttv5Transport::connect() {
    return connectMQTT();
}

void Mqttv5Transport::disconnect() {
    disconnectMQTT();
}

bool Mqttv5Transport::isConnected() const {
    return isMQTTConnected();
}

bool Mqttv5Transport::publish(const std::string& topic, const std::string& payload) {
    return publishMQTT(String(topic.c_str()), String(payload.c_str()), 1, false);
}

void Mqttv5Transport::subscribe(const std::string& topic) {
    subscribeMQTT(String(topic.c_str()), 1);
}

void Mqttv5Transport::onMessage(MessageHandler handler) {
    handler_ = std::move(handler);
    setMqttMessageSink([this](const String& topic, const String& payload) {
        if (handler_) {
            handler_(topic.c_str(), payload.c_str());
        }
    });
}

void Mqttv5Transport::onConnect(std::function<void()> cb) {
    onConnectCb_ = std::move(cb);
    setMqttConnectCallback([this]() {
        if (onConnectCb_) {
            onConnectCb_();
        }
    });
}

void Mqttv5Transport::loop() {
    mqttLoop();
}

}
