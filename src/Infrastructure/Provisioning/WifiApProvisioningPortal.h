#pragma once

#include <Arduino.h>
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

        // Campos adicionales en el portal cautivo VESIS-<ChipID> para Broker MQTTv5 y Cuenta
        char portBuf[12];
        snprintf(portBuf, sizeof(portBuf), "%d", config.mqtt_port > 0 ? config.mqtt_port : 8883);

        WiFiManagerParameter customMqttServer("server", "Servidor MQTT 5.0", config.mqtt_broker.c_str(), 64);
        WiFiManagerParameter customMqttPort("port", "Puerto MQTT (8883 TLS)", portBuf, 6);
        WiFiManagerParameter customMqttUser("apikey", "Usuario / API Key", config.mqtt_user.c_str(), 64);
        WiFiManagerParameter customBindToken("token", "Bind Token (Opcional)", config.bind_token.c_str(), 64);

        wifiManager.addParameter(&customMqttServer);
        wifiManager.addParameter(&customMqttPort);
        wifiManager.addParameter(&customMqttUser);
        wifiManager.addParameter(&customBindToken);

        bool portalSaved = false;
        wifiManager.setSaveConfigCallback([&]() {
            portalSaved = true;
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

            // process() devuelve true cuando WiFiManager terminó de conectar y guardar credenciales
            if (wifiManager.process() || portalSaved) {
                break;
            }

            // En Sonoff 0x40233450: una pulsación corta en modo SoftAP cancela y sale del modo emparejamiento
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
            if (strlen(customBindToken.getValue()) > 0) {
                config.bind_token = customBindToken.getValue();
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
