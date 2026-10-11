#include "Infrastructure/Messaging/mqtt_client.h"
#include "Infrastructure/Net/NetworkClient.h"
#include "Infrastructure/Ota/ota_manager.h"

ESP32_MQTTv5 mqtt;

namespace relay::infra {

static String g_server;
static uint16_t g_port = 8883;
static String g_clientId;
static String g_apiKey;
static String g_firmwareVersion;

static std::function<void(const String&, const String&)> g_messageSink;
static std::function<void()> g_onConnectCallback;

void setMqttMessageSink(std::function<void(const String&, const String&)> sink) {
    g_messageSink = std::move(sink);
}

void setMqttConnectCallback(std::function<void()> cb) {
    g_onConnectCallback = std::move(cb);
}

void initMQTT(const char* server, uint16_t port,
              const char* clientId, const char* apiKey,
              const char* rootCa, const char* firmwareVersion) {
    g_server = server;
    g_port = port;
    g_clientId = clientId;
    g_apiKey = apiKey;
    g_firmwareVersion = firmwareVersion;

    configureNetworkClient(rootCa);

    Serial.printf("[MQTT5] Inicializando broker %s:%d con cliente %s...\n",
                  server, port, clientId);

    mqtt.begin(getActiveNetworkClient(), server, port);
    mqtt.setBufferSize(4096);
    mqtt.setKeepAlive(60);
    mqtt.setCleanStart(false);
    mqtt.setAutoReconnect(true, 1000, 30000);
    mqtt.setSessionExpiry(3600);

    MQTTProperties willProps;
    willProps.setContentType("application/json");
    willProps.setWillDelay(5);
    String willTopic = "status";
    String willPayload = "{\"status\":\"offline\",\"device_id\":\"" + g_clientId + "\"}";
    mqtt.setWill(willTopic, willPayload, 1, true, &willProps);

    mqtt.onConnect([]() {
        Serial.println("[MQTT5] ¡Conectado al broker exitosamente!");

        initOTA(g_clientId, g_firmwareVersion);

        if (g_onConnectCallback) {
            g_onConnectCallback();
        }
    });

    mqtt.onDisconnect([](MQTTReasonCode rc) {
        Serial.printf("[MQTT5] Desconectado (Código: 0x%02X)\n", (uint8_t)rc);
    });

    mqtt.onError([](MQTTReasonCode rc, const String& msg) {
        Serial.printf("[MQTT5] Error 0x%02X: %s\n", (uint8_t)rc, msg.c_str());
    });

    mqtt.onMessage([](const MQTTMessage& msg) {
        Serial.printf("[MQTT5] Mensaje recibido en [%s]\n", msg.topic.c_str());
        if (g_messageSink) {
            g_messageSink(msg.topic, msg.payload);
        }
    });
}

bool connectMQTT() {
    if (mqtt.connected()) return true;

    Serial.printf("[MQTT5] Conectando a %s:%d como %s...\n",
                  g_server.c_str(), g_port, g_clientId.c_str());

    return mqtt.connect(g_clientId, g_apiKey, "");
}

void disconnectMQTT() {
    mqtt.disconnect();
}

void mqttLoop() {
    mqtt.loop();
}

bool isMQTTConnected() {
    return mqtt.connected();
}

bool publishMQTT(const String& topic, const String& payload, uint8_t qos, bool retain) {
    if (!mqtt.connected()) return false;
    return mqtt.publish(topic, payload, qos, retain);
}

bool subscribeMQTT(const String& topic, uint8_t qos) {
    if (!mqtt.connected()) return false;
    return mqtt.subscribe(topic, qos);
}

}
