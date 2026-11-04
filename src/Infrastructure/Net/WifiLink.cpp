#include "Infrastructure/Net/WifiLink.h"
#include "Infrastructure/Config/nvs_config.h"
#include <Arduino.h>
#include <WiFi.h>

namespace relay::infra {

static bool s_wifiEventRegistered = false;
static void registerRuntimeWifiReasonTracker() {
    if (s_wifiEventRegistered) return;
    s_wifiEventRegistered = true;
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            saveLastWifiErrorNvs("");
        } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            uint8_t reason = info.wifi_sta_disconnected.reason;
            if (reason == 201) {
                saveLastWifiErrorNvs("ap_not_found");
            } else if (reason == 2 || reason == 15 || reason == 202 || reason == 203 || reason == 204) {
                saveLastWifiErrorNvs("auth_failed");
            }
        }
    });
}

bool WifiLink::connect() {
    registerRuntimeWifiReasonTracker();
    if (WiFi.status() == WL_CONNECTED) {
        saveLastWifiErrorNvs("");
        return true;
    }

    if (ssid_.empty()) {
        return false;
    }

    Serial.printf("[WIFI] Iniciando conexión WiFi a SSID: %s\n", ssid_.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid_.c_str(), pass_.c_str());
    lastReconnectAttempt_ = millis();

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 1500) {
        delay(25);
    }

    if (WiFi.status() == WL_CONNECTED) {
        saveLastWifiErrorNvs("");
        Serial.printf("[WIFI] ¡Conectado! IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        return true;
    }

    Serial.println("[WIFI] Asociación en progreso en segundo plano (Offline-First activo)");
    return false;
}

bool WifiLink::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

int WifiLink::rssi() {
    return WiFi.RSSI();
}

std::string WifiLink::ipAddress() {
    return isConnected() ? WiFi.localIP().toString().c_str() : "0.0.0.0";
}

void WifiLink::loop() {
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt_ > 10000) {
            lastReconnectAttempt_ = now;
            if (!ssid_.empty()) {
                Serial.printf("[WIFI] Reintentando conexión a %s...\n", ssid_.c_str());
                WiFi.mode(WIFI_STA);
                WiFi.begin(ssid_.c_str(), pass_.c_str());
            }
        }
    }
}

} // namespace relay::infra
