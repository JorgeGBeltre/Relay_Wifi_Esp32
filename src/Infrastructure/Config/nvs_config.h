#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct AppConfig {
    String wifi_ssid;
    String wifi_pass;
    String mqtt_broker;
    int    mqtt_port;
    String mqtt_user;       // Usuario / API Key / Cuenta
    String mqtt_pass;       // Clave o Token de sesión
    String device_id;       // Identificador único (10 caracteres / chip ID estilo Sonoff)
    String device_key;      // Clave privada de hardware / Device Key de fábrica
    String bind_token;      // Token de registro / reclamo emitido por la app móvil para enlazar la cuenta
    String ota_hmac_key;    // Clave HMAC para OTA segura
};

extern AppConfig config;

void initConfig(const char* defaultDeviceId = nullptr);
bool isProvisioned();
void saveConfig();
void loadConfig();
void resetConfig();

