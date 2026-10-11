#include "Application/Services/ConnectivityService.h"

namespace relay::app {

bool ConnectivityService::connect() {
    if (!wifi_.isConnected()) {
        log_.info("[CONN] Conectando a WiFi...");
        if (!wifi_.connect()) {
            log_.warn("[CONN] Conexión WiFi falló");
            return false;
        }
        log_.info("[CONN] WiFi conectado exitosamente");
    }

    log_.info("[CONN] Conectando a MQTT 5.0...");
    if (!transport_.connect()) {
        log_.warn("[CONN] Conexión MQTT falló");
        return false;
    }

    log_.info("[CONN] Online (WiFi + MQTT 5.0)");
    return true;
}

bool ConnectivityService::isOnline() {
    return wifi_.isConnected() && transport_.isConnected();
}

int ConnectivityService::activeRssi() {
    return wifi_.isConnected() ? wifi_.rssi() : 0;
}

std::string ConnectivityService::activeIp() {
    return wifi_.isConnected() ? wifi_.ipAddress() : "0.0.0.0";
}

bool ConnectivityService::ensureConnected() {
    if (!wifi_.isConnected()) {
        wifi_.loop();
        return false;
    }

    if (!transport_.isConnected()) {
        log_.info("[CONN] Reconectando MQTT 5.0...");
        return transport_.connect();
    }

    return true;
}

}
