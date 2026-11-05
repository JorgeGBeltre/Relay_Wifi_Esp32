#pragma once

#include "Domain/Interfaces/IConfigRepository.h"
#include "Infrastructure/Config/nvs_config.h"

namespace relay::infra {

/**
 * @brief Adaptador IConfigRepository sobre Preferences / NVS.
 * Convierte de forma transparente entre la entidad pura DeviceConfig y AppConfig.
 */
class NvsConfigRepository : public domain::IConfigRepository {
public:
    void begin() override {
        initConfig();
    }

    void begin(const char* defaultDeviceId) {
        initConfig(defaultDeviceId);
    }

    bool load(domain::DeviceConfig& out) override {
        loadConfig();
        out = toDomain(::config);
        out.provisioned = ::isProvisioned();
        return out.provisioned;
    }

    bool save(const domain::DeviceConfig& cfg) override {
        String prevErr = ::config.last_wifi_error;
        String prevSsid = ::config.wifi_ssid;
        ::config = fromDomain(cfg);
        if (::config.wifi_ssid == prevSsid) {
            ::config.last_wifi_error = prevErr;
        } else {
            ::config.last_wifi_error = "";
        }
        saveConfig();
        return true;
    }

    bool erase() override {
        resetConfig();
        return true;
    }

    bool isProvisioned() override {
        return ::isProvisioned();
    }

    bool saveLastRelayState(bool state) override {
        return ::saveLastRelayStateNvs(state);
    }

    bool saveRelayMask(uint8_t mask) override {
        return ::saveRelayMaskNvs(mask);
    }

    uint8_t loadRelayMask(bool fallbackFirstChannel = false) override {
        return ::loadRelayMaskNvs(fallbackFirstChannel);
    }

    uint32_t reserveStateVersionBlock(uint32_t blockSize = 64U) override {
        return ::reserveStateVersionBlockNvs(blockSize);
    }

    bool saveStartupMode(domain::StartupMode mode) override {
        return ::saveStartupModeNvs(static_cast<uint8_t>(mode));
    }

    bool savePulseConfig(bool enabled, uint32_t widthMs) override {
        return ::savePulseConfigNvs(enabled, widthMs);
    }

    bool saveSledOnline(bool enabled) override {
        return ::saveSledOnlineNvs(enabled);
    }

    bool saveTimers(const domain::ScheduledTimer* timers, size_t count) override {
        return ::saveTimersNvs(timers, count);
    }

    size_t loadTimers(domain::ScheduledTimer* timers, size_t maxCount) override {
        return ::loadTimersNvs(timers, maxCount);
    }

private:
    static domain::DeviceConfig toDomain(const AppConfig& c) {
        domain::DeviceConfig d;
        d.wifiSsid       = c.wifi_ssid.c_str();
        d.wifiPass       = c.wifi_pass.c_str();
        d.mqttBroker     = c.mqtt_broker.c_str();
        d.mqttPort       = c.mqtt_port;
        d.mqttUser       = c.mqtt_user.c_str();
        d.mqttPass       = c.mqtt_pass.c_str();
        d.deviceId       = c.device_id.c_str();
        d.deviceKey      = c.device_key.c_str();
        d.bindToken      = c.bind_token.c_str();
        d.otaHmacKey     = c.ota_hmac_key.c_str();
        d.startupMode    = (c.startup_mode <= 2) ? static_cast<domain::StartupMode>(c.startup_mode) : domain::StartupMode::Off;
        d.lastRelayState = c.last_relay_state;
        d.pulseEnabled   = c.pulse_enabled;
        d.pulseWidthMs   = c.pulse_width_ms;
        d.sledOnline     = c.sled_online;
        d.accountBound   = c.account_bound;
        return d;
    }

    static AppConfig fromDomain(const domain::DeviceConfig& d) {
        AppConfig c;
        c.wifi_ssid        = d.wifiSsid.c_str();
        c.wifi_pass        = d.wifiPass.c_str();
        c.mqtt_broker      = d.mqttBroker.c_str();
        c.mqtt_port        = d.mqttPort;
        c.mqtt_user        = d.mqttUser.c_str();
        c.mqtt_pass        = d.mqttPass.c_str();
        c.device_id        = d.deviceId.c_str();
        c.device_key       = d.deviceKey.c_str();
        c.bind_token       = d.bindToken.c_str();
        c.ota_hmac_key     = d.otaHmacKey.c_str();
        c.startup_mode     = static_cast<uint8_t>(d.startupMode);
        c.last_relay_state = d.lastRelayState;
        c.pulse_enabled    = d.pulseEnabled;
        c.pulse_width_ms   = d.pulseWidthMs;
        c.sled_online      = d.sledOnline;
        c.account_bound    = d.accountBound;
        return c;
    }
};

} // namespace relay::infra
