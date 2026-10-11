#include "Infrastructure/Net/WifiLink.h"
#include <Arduino.h>
#include <WiFi.h>

namespace relay::infra {

bool WifiLink::connect() {
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.println("[WIFI] Iniciando conexión WiFi...");
    WiFi.mode(WIFI_STA);

    if (!ssid_.empty()) {
        Serial.printf("[WIFI] Conectando a red SSID: %s\n", ssid_.c_str());
        WiFi.begin(ssid_.c_str(), pass_.c_str());
    } else {
        Serial.println("[WIFI] Conectando con credenciales previas guardadas...");
        WiFi.begin();
    }

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < connectTimeoutMs_) {
        delay(150);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] ¡Conectado! IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        return true;
    }

    Serial.println("[WIFI] No se pudo conectar a la red WiFi");
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
            Serial.println("[WIFI] Intentando reconexión...");
            if (!ssid_.empty()) {
                WiFi.begin(ssid_.c_str(), pass_.c_str());
            } else {
                WiFi.begin();
            }
        }
    }
}

} // namespace relay::infra
