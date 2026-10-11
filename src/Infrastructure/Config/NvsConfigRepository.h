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
        ::config = fromDomain(cfg);
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

private:
    static domain::DeviceConfig toDomain(const AppConfig& c) {
        domain::DeviceConfig d;
        d.wifiSsid   = c.wifi_ssid.c_str();
        d.wifiPass   = c.wifi_pass.c_str();
        d.mqttBroker = c.mqtt_broker.c_str();
        d.mqttPort   = c.mqtt_port;
        d.mqttUser   = c.mqtt_user.c_str();
        d.mqttPass   = c.mqtt_pass.c_str();
        d.deviceId   = c.device_id.c_str();
        d.deviceKey  = c.device_key.c_str();
        d.bindToken  = c.bind_token.c_str();
        d.otaHmacKey = c.ota_hmac_key.c_str();
        return d;
    }

    static AppConfig fromDomain(const domain::DeviceConfig& d) {
        AppConfig c;
        c.wifi_ssid   = d.wifiSsid.c_str();
        c.wifi_pass   = d.wifiPass.c_str();
        c.mqtt_broker = d.mqttBroker.c_str();
        c.mqtt_port   = d.mqttPort;
        c.mqtt_user   = d.mqttUser.c_str();
        c.mqtt_pass   = d.mqttPass.c_str();
        c.device_id   = d.deviceId.c_str();
        c.device_key  = d.deviceKey.c_str();
        c.bind_token  = d.bindToken.c_str();
        c.ota_hmac_key = d.otaHmacKey.c_str();
        return c;
    }
};

} // namespace relay::infra

