#include "Infrastructure/Config/nvs_config.h"
#include "Infrastructure/Config/config.h"
#include <cstdio>
#include <cstring>
#include <esp_system.h>
#include <mbedtls/sha256.h>

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

bool isAccountBound() {
    return config.account_bound;
}

bool hasValidWifiCredentials() {
    return config.wifi_ssid.length() > 0;
}

bool hasValidBrokerCredentials() {
    return config.mqtt_broker.length() > 0 &&
           config.mqtt_broker != MQTT_SERVER &&
           config.mqtt_broker != "tu-broker.hivemq.cloud" &&
           config.mqtt_broker != "127.0.0.1" &&
           config.mqtt_user.length() > 0 &&
           config.mqtt_user != API_KEY &&
           config.mqtt_user != "iotk_REEMPLAZAR";
}

void computeUnlinkProofHex(const char* deviceKey, const char* chipId, char outHex[33]) {
    outHex[0] = '\0';
    if (!deviceKey || !chipId || deviceKey[0] == '\0' || chipId[0] == '\0') {
        return;
    }
    char inputBuf[192];
    const int n = std::snprintf(inputBuf, sizeof(inputBuf), "%s:UNLINK:%s", deviceKey, chipId);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(inputBuf)) {
        return;
    }
    uint8_t digest[32] = {0};
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, reinterpret_cast<const unsigned char*>(inputBuf), static_cast<size_t>(n));
    mbedtls_sha256_finish_ret(&ctx, digest);
    mbedtls_sha256_free(&ctx);

    for (size_t i = 0; i < 16; ++i) {
        std::snprintf(outHex + (i * 2), 3, "%02x", digest[i]);
    }
    outHex[32] = '\0';
}

bool verifyUnlinkProof(const char* deviceKey, const char* chipId, const char* providedHex) {
    if (!providedHex || std::strlen(providedHex) != 32) {
        return false;
    }
    char expectedHex[33] = {0};
    computeUnlinkProofHex(deviceKey, chipId, expectedHex);
    if (expectedHex[0] == '\0') {
        return false;
    }
    uint8_t diff = 0;
    for (size_t i = 0; i < 32; ++i) {
        char a = providedHex[i];
        if (a >= 'A' && a <= 'F') {
            a = static_cast<char>(a - 'A' + 'a');
        }
        diff |= static_cast<uint8_t>(a ^ expectedHex[i]);
    }
    return diff == 0;
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
    const bool hasAcctBoundKey = preferences.isKey("acct_bound");
    const bool storedAcctBound = preferences.getBool("acct_bound", false);
    preferences.end();

    config.account_bound = hasAcctBoundKey ? storedAcctBound : hasValidBrokerCredentials();

    if (config.device_key.length() == 0) {
        config.device_key = getDefaultDeviceKey();
    }
}

void saveConfig() {
    if (config.device_key.length() == 0) {
        config.device_key = getDefaultDeviceKey();
    }
    if (hasValidBrokerCredentials()) {
        config.account_bound = true;
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
    preferences.putBool("acct_bound",    config.account_bound);
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
    config = AppConfig{};
    config.device_id = getDefaultChipId();
    config.device_key = getDefaultDeviceKey();
}

bool saveLastRelayStateNvs(bool state) {
    config.last_relay_state = state;
    if (!preferences.begin("relay-config", false)) return false;
    preferences.putBool("last_rly", state);
    uint8_t mask = preferences.getUChar("rly_mask", 0);
    if (state) mask |= 0x01U;
    else mask &= static_cast<uint8_t>(~0x01U);
    preferences.putUChar("rly_mask", mask);
    preferences.end();
    return true;
}

bool saveRelayMaskNvs(uint8_t mask) {
    config.last_relay_state = (mask & 0x01U) != 0;
    if (!preferences.begin("relay-config", false)) return false;
    preferences.putBool("last_rly", config.last_relay_state);
    preferences.putUChar("rly_mask", mask);
    preferences.end();
    return true;
}

uint8_t loadRelayMaskNvs(bool fallbackFirstChannel) {
    if (!preferences.begin("relay-config", true)) {
        return fallbackFirstChannel ? 0x01U : 0x00U;
    }
    const bool hasMask = preferences.isKey("rly_mask");
    const uint8_t mask = hasMask
        ? preferences.getUChar("rly_mask", 0)
        : (preferences.getBool("last_rly", fallbackFirstChannel) ? 0x01U : 0x00U);
    preferences.end();
    return mask;
}

uint32_t reserveStateVersionBlockNvs(uint32_t blockSize) {
    const uint32_t step = (blockSize == 0U) ? 64U : blockSize;
    if (!preferences.begin("relay-config", false)) {
        return 1U;
    }
    const uint32_t currentFloor = preferences.getUInt("st_ver_flr", 0U);
    const uint32_t nextBase = (currentFloor == 0U) ? 1U : currentFloor;
    const uint32_t nextFloor = nextBase + step;
    preferences.putUInt("st_ver_flr", nextFloor);
    preferences.end();
    return nextBase;
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
    preferences.putUChar("tmr_ver", 2);
    preferences.putUChar("tmr_cnt", static_cast<uint8_t>(safeCount));
    if (safeCount > 0 && timers) {
        preferences.putBytes("tmr_blob", timers, safeCount * sizeof(relay::domain::ScheduledTimer));
    } else {
        preferences.remove("tmr_blob");
    }
    preferences.end();
    return true;
}

size_t loadTimersNvs(relay::domain::ScheduledTimer* timers, size_t maxCount) {
    if (!timers || maxCount == 0) return 0;
    if (!preferences.begin("relay-config", true)) return 0;
    const uint8_t storedVer = preferences.getUChar("tmr_ver", 0);
    if (storedVer != 2) {
        preferences.end();
        return 0;
    }
    const uint8_t storedCount = preferences.getUChar("tmr_cnt", 0);
    const size_t safeCount = (storedCount > maxCount) ? maxCount : storedCount;
    const size_t expectedBytes = safeCount * sizeof(relay::domain::ScheduledTimer);
    if (safeCount > 0) {
        if (preferences.getBytesLength("tmr_blob") != expectedBytes) {
            preferences.end();
            return 0;
        }
        preferences.getBytes("tmr_blob", timers, expectedBytes);
    }
    preferences.end();
    return safeCount;
}
