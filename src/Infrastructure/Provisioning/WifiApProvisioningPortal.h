#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <cstdlib>
#include "Domain/Interfaces/IButtonHandler.h"
#include "Domain/Interfaces/IIndicator.h"
#include "Infrastructure/Config/nvs_config.h"

namespace relay::infra {

class WifiApProvisioningPortal {
public:
    static bool startPortal(uint32_t timeoutSec = 180,
                            domain::IIndicator* indicator = nullptr,
                            domain::IButtonHandler* button = nullptr) {
        loadConfig();

        WiFiManager wifiManager;
        uint64_t chipID = ESP.getEfuseMac();
        // Preserva íntegramente el SSID VESIS-<ChipID> y contraseña "12345678"
        String apSSID = "VESIS-" + String((uint16_t)(chipID >> 32), DEC) +
                        String((uint32_t)chipID, DEC);
        const char* apPassword = "12345678";

        Serial.printf("[PROVISION] Iniciando Compatible SoftAP: %s (Clave: %s)\n",
                      apSSID.c_str(), apPassword);

        if (indicator) {
            indicator->show(domain::IndicatorStatus::CompatibleAp);
        }

        // Campos adicionales en el portal cautivo VESIS-<ChipID> para Broker MQTTv5, Cuenta y Arranque
        char portBuf[12];
        snprintf(portBuf, sizeof(portBuf), "%d", config.mqtt_port > 0 ? config.mqtt_port : 8883);
        const char* startupDefault = (config.startup_mode == 1) ? "on" : ((config.startup_mode == 2) ? "stay" : "off");

        WiFiManagerParameter customMqttServer("server", "Servidor MQTT 5.0", config.mqtt_broker.c_str(), 64);
        WiFiManagerParameter customMqttPort("port", "Puerto MQTT (8883 TLS)", portBuf, 6);
        WiFiManagerParameter customMqttUser("apikey", "Usuario / API Key", config.mqtt_user.c_str(), 64);
        WiFiManagerParameter customMqttPass("mqtt_pass", "Clave MQTT (Opcional)", config.mqtt_pass.c_str(), 64);
        WiFiManagerParameter customBindToken("token", "Bind Token (Opcional)", config.bind_token.c_str(), 64);
        WiFiManagerParameter customStartup("startup", "Modo Arranque (off/on/stay)", startupDefault, 8);

        wifiManager.addParameter(&customMqttServer);
        wifiManager.addParameter(&customMqttPort);
        wifiManager.addParameter(&customMqttUser);
        wifiManager.addParameter(&customMqttPass);
        wifiManager.addParameter(&customBindToken);
        wifiManager.addParameter(&customStartup);

        bool portalSaved = false;
        bool jsonApSaved = false;
        wifiManager.setSaveConfigCallback([&]() {
            portalSaved = true;
        });

        // Endpoints JSON de Compatible Pairing Mode (GET /device y POST /ap)
        wifiManager.setWebServerCallback([&]() {
            if (!wifiManager.server) return;

            wifiManager.server->on("/device", HTTP_GET, [&]() {
                char chipHex[16];
                snprintf(chipHex, sizeof(chipHex), "%04X%08X", (uint16_t)(chipID >> 32), (uint32_t)chipID);
                StaticJsonDocument<256> doc;
                doc["deviceid"]    = config.device_id.length() > 0 ? config.device_id.c_str() : chipHex;
                doc["apikey"]      = config.device_key.c_str();
                doc["chipid"]      = chipHex;
                doc["device_type"] = "Relay-Wifi";
                doc["accept"]      = "post";
                String out;
                serializeJson(doc, out);
                wifiManager.server->send(200, "application/json", out);
            });

            wifiManager.server->on("/ap", HTTP_POST, [&]() {
                String body = wifiManager.server->arg("plain");
                StaticJsonDocument<512> doc;
                if (deserializeJson(doc, body) != DeserializationError::Ok ||
                    (!doc.containsKey("ssid") && !doc.containsKey("wifi_ssid"))) {
                    wifiManager.server->send(400, "application/json", "{\"error\":400}");
                    return;
                }

                if (doc.containsKey("ssid")) config.wifi_ssid = doc["ssid"].as<String>();
                else if (doc.containsKey("wifi_ssid")) config.wifi_ssid = doc["wifi_ssid"].as<String>();

                if (doc.containsKey("password")) config.wifi_pass = doc["password"].as<String>();
                else if (doc.containsKey("pass")) config.wifi_pass = doc["pass"].as<String>();
                else if (doc.containsKey("wifi_pass")) config.wifi_pass = doc["wifi_pass"].as<String>();

                if (doc.containsKey("serverName")) config.mqtt_broker = doc["serverName"].as<String>();
                else if (doc.containsKey("mqtt_broker")) config.mqtt_broker = doc["mqtt_broker"].as<String>();
                else if (doc.containsKey("server")) config.mqtt_broker = doc["server"].as<String>();

                if (doc.containsKey("port")) config.mqtt_port = doc["port"].as<int>();
                else if (doc.containsKey("mqtt_port")) config.mqtt_port = doc["mqtt_port"].as<int>();

                if (doc.containsKey("apikey")) config.mqtt_user = doc["apikey"].as<String>();
                else if (doc.containsKey("mqtt_user")) config.mqtt_user = doc["mqtt_user"].as<String>();
                else if (doc.containsKey("account")) config.mqtt_user = doc["account"].as<String>();

                if (doc.containsKey("mqtt_pass")) config.mqtt_pass = doc["mqtt_pass"].as<String>();
                if (doc.containsKey("devicekey")) config.device_key = doc["devicekey"].as<String>();
                if (doc.containsKey("bind_token")) config.bind_token = doc["bind_token"].as<String>();
                else if (doc.containsKey("token")) config.bind_token = doc["token"].as<String>();

                if (doc.containsKey("startup")) {
                    String st = doc["startup"].as<String>();
                    st.toLowerCase();
                    config.startup_mode = (st == "on") ? 1 : ((st == "stay") ? 2 : 0);
                }
                if (doc.containsKey("pulse")) {
                    String pl = doc["pulse"].as<String>();
                    pl.toLowerCase();
                    config.pulse_enabled = (pl == "on" || pl == "1" || pl == "true");
                }
                if (doc.containsKey("pulseWidth")) {
                    uint32_t pw = doc["pulseWidth"].as<uint32_t>();
                    if (pw >= 500 && pw <= 36000000 && (pw % 500) == 0) {
                        config.pulse_width_ms = pw;
                    }
                }
                if (doc.containsKey("sledOnline")) {
                    String sl = doc["sledOnline"].as<String>();
                    sl.toLowerCase();
                    config.sled_online = (sl != "off" && sl != "0" && sl != "false");
                }

                wifiManager.server->send(200, "application/json", "{\"error\":0}");
                jsonApSaved = true;
                portalSaved = true;
            });
        });

        wifiManager.setConfigPortalBlocking(false);
        wifiManager.setConfigPortalTimeout(timeoutSec);

        // Esperar a que el usuario suelte el botón de la 2ª pulsación larga antes de escuchar pulsación corta de salida
        if (button) {
            unsigned long releaseStart = millis();
            while (digitalRead(button->getPin()) == LOW && (millis() - releaseStart) < 3000) {
                if (indicator) indicator->update();
                delay(20);
            }
            button->init();
        }

        wifiManager.startConfigPortal(apSSID.c_str(), apPassword);

        const unsigned long startMs = millis();
        const unsigned long timeoutMs = timeoutSec * 1000UL;
        bool cancelledByButton = false;

        while ((millis() - startMs) < timeoutMs) {
            if (indicator) {
                indicator->update();
            }

            if (wifiManager.process() || portalSaved) {
                break;
            }

            // Una pulsación corta en modo SoftAP cancela y sale del modo emparejamiento
            if (button) {
                auto ev = button->update();
                if (ev == domain::ButtonEvent::ShortPress) {
                    Serial.println("[PROVISION] Pulsación corta detectada -> Saliendo de portal SoftAP VESIS.");
                    cancelledByButton = true;
                    wifiManager.stopConfigPortal();
                    break;
                }
            }

            delay(20);
        }

        if (!cancelledByButton && (portalSaved || WiFi.status() == WL_CONNECTED || WiFi.SSID().length() > 0)) {
            if (!jsonApSaved) {
                if (WiFi.SSID().length() > 0) {
                    config.wifi_ssid = WiFi.SSID();
                }
                if (WiFi.psk().length() > 0) {
                    config.wifi_pass = WiFi.psk();
                }
                if (strlen(customMqttServer.getValue()) > 0) {
                    config.mqtt_broker = customMqttServer.getValue();
                }
                if (strlen(customMqttPort.getValue()) > 0) {
                    int p = atoi(customMqttPort.getValue());
                    if (p > 0 && p <= 65535) config.mqtt_port = p;
                }
                if (strlen(customMqttUser.getValue()) > 0) {
                    config.mqtt_user = customMqttUser.getValue();
                }
                if (strlen(customMqttPass.getValue()) > 0) {
                    config.mqtt_pass = customMqttPass.getValue();
                }
                if (strlen(customBindToken.getValue()) > 0) {
                    config.bind_token = customBindToken.getValue();
                }
                if (strlen(customStartup.getValue()) > 0) {
                    String st = customStartup.getValue();
                    st.toLowerCase();
                    config.startup_mode = (st == "on") ? 1 : ((st == "stay") ? 2 : 0);
                }
            }

            // Persistir en el namespace 'relay-config' de NVS para que isProvisioned() sea true tras reiniciar
            saveConfig();
            Serial.printf("[PROVISION] Credenciales guardadas en NVS (SSID: %s). Reiniciando...\n",
                          config.wifi_ssid.c_str());
            delay(200);
            ESP.restart();
            return true;
        }

        Serial.println("[PROVISION] Portal SoftAP finalizado sin cambios.");
        WiFi.mode(WIFI_STA);
        return false;
    }
};

} // namespace relay::infra
