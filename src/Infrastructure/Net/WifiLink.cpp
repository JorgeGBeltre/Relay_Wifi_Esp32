#include "Infrastructure/Net/WifiLink.h"
#include <Arduino.h>
#include <WiFi.h>

namespace relay::infra {

bool WifiLink::connect() {
    if (WiFi.status() == WL_CONNECTED) return true;

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
