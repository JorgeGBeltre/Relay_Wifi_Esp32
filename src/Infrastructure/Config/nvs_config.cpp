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

// Genera una clave criptográfica de hardware única y permanente para el dispositivo (devicekey)
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
    bool flag = preferences.getBool("provisioned", false);
    String ssid = preferences.getString("wifi_ssid", "");
    preferences.end();
    return flag && ssid.length() > 0;
}

bool hasValidWifiCredentials() {
    return config.wifi_ssid.length() > 0;
}

bool hasValidBrokerCredentials() {
    return config.mqtt_broker.length() > 0 &&
           config.mqtt_broker != "tu-broker.hivemq.cloud" &&
           config.mqtt_broker != "127.0.0.1" &&
           config.mqtt_user.length() > 0 &&
           config.mqtt_user != "iotk_REEMPLAZAR";
}

const char* resolveDeviceConfigState(bool wifiConnected, bool mqttConnected) {
    if (!hasValidWifiCredentials()) {
        return "unconfigured";
    }
    if (!hasValidBrokerCredentials()) {
        return wifiConnected ? "wifi_connected" : "incomplete";
    }
    if (wifiConnected && mqttConnected) {
        return "online";
    }
    if (wifiConnected) {
        return "wifi_connected";
    }
    if (config.last_wifi_error.length() > 0) {
        return "connection_failed";
    }
    return "configured_disconnected";
}

void loadConfig() {
    preferences.begin("relay-config", true);
    config.wifi_ssid        = preferences.getString("wifi_ssid", "");
    config.wifi_pass        = preferences.getString("wifi_pass", "");
    config.mqtt_broker      = preferences.getString("mqtt_broker", MQTT_SERVER);
    config.mqtt_port        = preferences.getInt("mqtt_port", MQTT_PORT);
    config.mqtt_user        = preferences.getString("mqtt_user", API_KEY);
    config.mqtt_pass        = preferences.getString("mqtt_pass", "");
    config.device_id        = preferences.getString("device_id", "");
    config.device_key       = preferences.getString("device_key", "");
    config.bind_token       = preferences.getString("bind_token", "");
    config.ota_hmac_key     = preferences.getString("ota_hmac", "");
    config.startup_mode     = preferences.getUChar("startup", 0);
    config.last_relay_state = preferences.getBool("last_rly", false);
    config.pulse_enabled    = preferences.getBool("pulse_en", false);
    config.pulse_width_ms   = preferences.getUInt("pulse_w", 500);
    config.sled_online      = preferences.getBool("sled_on", true);
    config.last_wifi_error  = preferences.getString("wifi_err", "");
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
    preferences.putUChar("startup",      config.startup_mode);
    preferences.putBool("last_rly",      config.last_relay_state);
    preferences.putBool("pulse_en",      config.pulse_enabled);
    preferences.putUInt("pulse_w",       config.pulse_width_ms);
    preferences.putBool("sled_on",       config.sled_online);
    preferences.putString("wifi_err",    config.last_wifi_error);
    preferences.putBool("provisioned",   config.wifi_ssid.length() > 0);
    preferences.end();
}

bool saveLastWifiErrorNvs(const char* err) {
    String nextErr = err ? String(err) : String("");
    if (config.last_wifi_error == nextErr) {
        return true;
    }
    config.last_wifi_error = nextErr;
    if (!preferences.begin("relay-config", false)) return false;
    preferences.putString("wifi_err", config.last_wifi_error);
    preferences.end();
    return true;
}

void resetConfig() {
    preferences.begin("relay-config", false);
    preferences.clear();
    preferences.end();
}

bool saveLastRelayStateNvs(bool state) {
    config.last_relay_state = state;
    if (!preferences.begin("relay-config", false)) return false;
    preferences.putBool("last_rly", state);
    preferences.end();
    return true;
}

bool saveStartupModeNvs(uint8_t mode) {
    config.startup_mode = mode;
    if (!preferences.begin("relay-config", false)) return false;
    preferences.putUChar("startup", mode);
    preferences.end();
    return true;
}

bool savePulseConfigNvs(bool enabled, uint32_t widthMs) {
    config.pulse_enabled = enabled;
    config.pulse_width_ms = widthMs;
    if (!preferences.begin("relay-config", false)) return false;
    preferences.putBool("pulse_en", enabled);
    preferences.putUInt("pulse_w", widthMs);
    preferences.end();
    return true;
}

bool saveSledOnlineNvs(bool enabled) {
    config.sled_online = enabled;
    if (!preferences.begin("relay-config", false)) return false;
    preferences.putBool("sled_on", enabled);
    preferences.end();
    return true;
}

bool saveTimersNvs(const relay::domain::ScheduledTimer* timers, size_t count) {
    if (!preferences.begin("relay-config", false)) return false;
    const size_t safeCount = (count > 8) ? 8 : count;
    preferences.putUChar("tmr_cnt", static_cast<uint8_t>(safeCount));
    if (safeCount > 0 && timers) {
        preferences.putBytes("tmr_blob", timers, safeCount * sizeof(relay::domain::ScheduledTimer));
    }
    preferences.end();
    return true;
}

size_t loadTimersNvs(relay::domain::ScheduledTimer* timers, size_t maxCount) {
    if (!timers || maxCount == 0) return 0;
    if (!preferences.begin("relay-config", true)) return 0;
    const uint8_t storedCount = preferences.getUChar("tmr_cnt", 0);
    const size_t safeCount = (storedCount > maxCount) ? maxCount : storedCount;
    if (safeCount > 0) {
        preferences.getBytes("tmr_blob", timers, safeCount * sizeof(relay::domain::ScheduledTimer));
    }
    preferences.end();
    return safeCount;
}
