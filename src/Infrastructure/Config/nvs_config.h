#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <cstddef>
#include <cstdint>
#include "Domain/Entities/ScheduledTimer.h"

struct AppConfig {
    String   wifi_ssid;
    String   wifi_pass;
    String   mqtt_broker;
    int      mqtt_port      = 8883;
    String   mqtt_user;       // Usuario / API Key / Cuenta
    String   mqtt_pass;       // Clave o Token de sesión
    String   device_id;       // Identificador único (Chip ID / Device ID)
    String   device_key;      // Clave privada de hardware / Device Key de fábrica
    String   bind_token;      // Token de registro / reclamo emitido por la app móvil para enlazar la cuenta
    String   ota_hmac_key;    // Clave HMAC para OTA segura

    uint8_t  startup_mode     = 0;     // 0=Off, 1=On, 2=Stay
    bool     last_relay_state = false; // Último estado persistido para StartupMode::Stay
    bool     pulse_enabled    = false; // Modo pulso (Inching)
    uint32_t pulse_width_ms   = 500;   // Ancho de pulso en ms (500..36000000, paso 500)
    bool     sled_online      = true;  // Indicador LED de estado en línea (sledOnline)
    String   last_wifi_error;          // Último motivo verificable de fallo Wi-Fi (auth_failed, ap_not_found, etc.)
};

extern AppConfig config;

void initConfig(const char* defaultDeviceId = nullptr);
bool isProvisioned();
bool hasValidWifiCredentials();
bool hasValidBrokerCredentials();
const char* resolveDeviceConfigState(bool wifiConnected, bool mqttConnected);
void saveConfig();
void loadConfig();
void resetConfig();
bool saveLastWifiErrorNvs(const char* err);

bool saveLastRelayStateNvs(bool state);
bool saveRelayMaskNvs(uint8_t mask);
uint8_t loadRelayMaskNvs(bool fallbackFirstChannel = false);
uint32_t reserveStateVersionBlockNvs(uint32_t blockSize = 64U);
bool saveStartupModeNvs(uint8_t mode);
bool savePulseConfigNvs(bool enabled, uint32_t widthMs);
bool saveSledOnlineNvs(bool enabled);
bool saveTimersNvs(const relay::domain::ScheduledTimer* timers, size_t count);
size_t loadTimersNvs(relay::domain::ScheduledTimer* timers, size_t maxCount);
