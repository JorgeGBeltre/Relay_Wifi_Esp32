#include "Infrastructure/Config/nvs_config.h"
#include "Infrastructure/Config/config.h"
#include <esp_system.h>

AppConfig config;
static Preferences preferences;

static String getDefaultChipId() {
    uint64_t chipId = ESP.getEfuseMac();
    char buf[16];
    snprintf(buf, sizeof(buf), "%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
    return String(buf);
}

// Genera una clave criptográfica de hardware única y permanente para el dispositivo (estilo Sonoff devicekey)
static String getDefaultDeviceKey() {
    uint64_t mac = ESP.getEfuseMac();
    char buf[37];
    snprintf(buf, sizeof(buf), "%08X-%04X-4%03X-%04X-%012llX",
             (uint32_t)(mac & 0xFFFFFFFF),
             (uint16_t)((mac >> 32) & 0xFFFF),
             (uint16_t)((mac >> 16) & 0x0FFF),
             (uint16_t)(((mac >> 48) & 0x3FFF) | 0x8000),
             (unsigned long long)(mac ^ 0x5A5A5A5A5A5AULL));
    return String(buf);
}

void initConfig(const char* defaultDeviceId) {
    loadConfig();
    if (config.device_id.length() == 0) {
        if (defaultDeviceId && strlen(defaultDeviceId) > 0) {
            config.device_id = defaultDeviceId;
        } else {
            config.device_id = getDefaultChipId();
        }
    }
    if (config.device_key.length() == 0) {
        config.device_key = getDefaultDeviceKey();
    }
}

bool isProvisioned() {
    preferences.begin("relay-config", true);
    bool ok = preferences.getBool("provisioned", false);
    preferences.end();
    return ok;
}

void loadConfig() {
    preferences.begin("relay-config", true);
    config.wifi_ssid    = preferences.getString("wifi_ssid", "");
    config.wifi_pass    = preferences.getString("wifi_pass", "");
    config.mqtt_broker  = preferences.getString("mqtt_broker", MQTT_SERVER);
    config.mqtt_port    = preferences.getInt("mqtt_port", MQTT_PORT);
    config.mqtt_user    = preferences.getString("mqtt_user", API_KEY);
    config.mqtt_pass    = preferences.getString("mqtt_pass", "");
    config.device_id    = preferences.getString("device_id", "");
    config.device_key   = preferences.getString("device_key", "");
    config.bind_token   = preferences.getString("bind_token", "");
    config.ota_hmac_key = preferences.getString("ota_hmac", "");
    preferences.end();

    if (config.device_key.length() == 0) {
        config.device_key = getDefaultDeviceKey();
    }
}

void saveConfig() {
    if (config.device_key.length() == 0) {
        config.device_key = getDefaultDeviceKey();
    }
    preferences.begin("relay-config", false);
    preferences.putString("wifi_ssid",   config.wifi_ssid);
    preferences.putString("wifi_pass",   config.wifi_pass);
    preferences.putString("mqtt_broker", config.mqtt_broker);
    preferences.putInt("mqtt_port",      config.mqtt_port);
    preferences.putString("mqtt_user",   config.mqtt_user);
    preferences.putString("mqtt_pass",   config.mqtt_pass);
    preferences.putString("device_id",   config.device_id);
    preferences.putString("device_key",  config.device_key);
    preferences.putString("bind_token",  config.bind_token);
    preferences.putString("ota_hmac",    config.ota_hmac_key);
    preferences.putBool("provisioned",   true);
    preferences.end();
}

void resetConfig() {
    preferences.begin("relay-config", false);
    preferences.clear();
    preferences.end();
}

