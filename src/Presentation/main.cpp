#include <Arduino.h>
#include "Infrastructure/Config/config.h"
#include "Infrastructure/Config/NvsConfigRepository.h"

#include "Application/Controllers/RelayFirmwareController.h"
#include "Application/Messaging/MqttTopics.h"
#include "Application/Services/ConnectivityService.h"
#include "Application/Services/RelayService.h"
#include "Application/Services/TelemetryService.h"

#include "Infrastructure/Actuators/GpioRelay.h"
#include "Infrastructure/Button/GpioButton.h"
#include "Infrastructure/Indicator/GpioIndicator.h"
#include "Infrastructure/Logging/SerialLogger.h"
#include "Infrastructure/Messaging/Mqttv5Transport.h"
#include "Infrastructure/Messaging/mqtt_client.h"
#include "Infrastructure/Net/WifiLink.h"
#include "Infrastructure/Ota/Mqttv5OtaService.h"
#include "Infrastructure/Provisioning/ProvisioningPortals.h"
#include "Infrastructure/Serialization/ArduinoJsonPayloadSerializer.h"
#include "Infrastructure/System/EspDeviceRepository.h"
#include "Infrastructure/System/EspSystemControl.h"
#include "Infrastructure/Time/ArduinoClock.h"
#include "Infrastructure/Time/NtpTimeProvider.h"

using namespace relay;

static infra::ArduinoClock                 g_clock;
static infra::SerialLogger                 g_logger;
static infra::EspSystemControl             g_system;
static infra::GpioIndicator                g_indicator{wifiLed, FAST_BLINK_INTERVAL, SLOW_BLINK_INTERVAL};
static infra::GpioRelay                    g_relay1{RelayPin1};
static infra::GpioButton                   g_button1{BUTTON_PIN1, DEBOUNCE_TIME, BUTTON_LONG_PRESS_TIME};
static infra::WifiLink                     g_wifi{WIFI_CONNECT_TIMEOUT};
static infra::Mqttv5Transport              g_transport;
static infra::Mqttv5OtaService             g_ota;
static infra::EspDeviceRepository          g_deviceRepo{FIRMWARE_VERSION};
static infra::NtpTimeProvider              g_timeProvider{gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2};
static infra::ArduinoJsonPayloadSerializer g_serializer;
static infra::NvsConfigRepository          g_configRepo;
static infra::BleProvisioningPortal        g_blePortal;

static app::RelayService*            g_relayService = nullptr;
static app::ConnectivityService*     g_connectivity = nullptr;
static app::TelemetryService*        g_telemetry = nullptr;
static app::RelayFirmwareController* g_controller = nullptr;

static bool shouldForceProvisioning() {
    pinMode(BUTTON_PIN1, INPUT_PULLUP);
    if (digitalRead(BUTTON_PIN1) == LOW) {
        const unsigned long start = millis();
        while (digitalRead(BUTTON_PIN1) == LOW) {
            if (millis() - start > BUTTON_LONG_PRESS_TIME) return true;
            delay(50);
        }
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=============================================");
    Serial.println("  Relay-Wifi Clean Architecture - Firmware   ");
    Serial.printf("  Versión: %s\n", FIRMWARE_VERSION);
    Serial.println("=============================================\n");

    auto identity = g_deviceRepo.identity();
    g_configRepo.begin(identity.chipId.c_str());

    domain::DeviceConfig devConfig;
    bool isProv = g_configRepo.load(devConfig);

    // Si el usuario mantiene pulsado el botón al iniciar o no está aprovisionado
    if (!isProv || shouldForceProvisioning()) {
        Serial.println("[MAIN] Dispositivo en modo aprovisionamiento...");
        g_indicator.show(domain::IndicatorStatus::Connecting);

        // Intento primario: Portal Bluetooth BLE
        bool bleOk = g_blePortal.run(60000);
        if (bleOk) {
            Serial.println("[MAIN] Aprovisionamiento BLE completado. Reiniciando...");
            delay(500);
            ESP.restart();
        } else if (!isProv) {
            // Fallback secundario: SoftAP si no está aprovisionado
            Serial.println("[MAIN] BLE timeout. Iniciando portal WiFi SoftAP de respaldo...");
            infra::WifiApProvisioningPortal::startPortal(180);
        }
    }

    std::string activeDeviceId = !devConfig.deviceId.empty() ? devConfig.deviceId : identity.chipId;
    std::string deviceId1 = activeDeviceId + "-01";

    Serial.printf("[INIT] Chip ID:       %s\n", identity.chipId.c_str());
    Serial.printf("[INIT] Device ID:     %s\n", activeDeviceId.c_str());
    Serial.printf("[INIT] MAC:           %s\n", identity.macAddress.c_str());
    Serial.printf("[INIT] Relay ID:      %s\n", deviceId1.c_str());

    g_wifi.setCredentials(devConfig.wifiSsid, devConfig.wifiPass);

    const char* broker = !devConfig.mqttBroker.empty() ? devConfig.mqttBroker.c_str() : MQTT_SERVER;
    int port = devConfig.mqttPort > 0 ? devConfig.mqttPort : MQTT_PORT;
    const char* apiKey = !devConfig.mqttUser.empty() ? devConfig.mqttUser.c_str() : API_KEY;

    app::MqttTopics topics = app::MqttTopics::forDevice(activeDeviceId, "device_switch1");

    infra::initMQTT(broker, port, activeDeviceId.c_str(),
                    apiKey, root_ca_pem, FIRMWARE_VERSION);

    static app::RelayService relayService{g_logger};
    relayService.registerRelay(deviceId1, g_relay1, "device_switch1");
    g_relayService = &relayService;

    static app::ConnectivityService connectivity{g_wifi, g_transport, g_logger};
    g_connectivity = &connectivity;

    static app::TelemetryService telemetry{
        relayService, connectivity, g_deviceRepo, g_timeProvider,
        g_clock, g_serializer, topics, g_logger};
    g_telemetry = &telemetry;

    auto provisioningCallback = []() {
        Serial.println("[PROVISION] Pulsación larga detectada (>3s) -> Iniciando portal BLE...");
        g_indicator.show(domain::IndicatorStatus::Connecting);
        bool bleOk = g_blePortal.run(60000);
        if (bleOk) {
            Serial.println("[PROVISION] Aprovisionamiento BLE completado. Reiniciando...");
            delay(500);
            ESP.restart();
        } else {
            Serial.println("[PROVISION] BLE timeout. Abriendo portal SoftAP de respaldo...");
            infra::WifiApProvisioningPortal::startPortal(180);
        }
    };

    static app::RelayFirmwareController controller{
        relayService, connectivity, telemetry, g_ota, g_indicator,
        g_system, g_timeProvider, g_clock, g_logger, topics, provisioningCallback};

    controller.registerButton(g_button1, deviceId1);

    controller.setup();
    g_controller = &controller;

    g_timeProvider.sync();
}

void loop() {
    if (g_controller) {
        g_controller->tick();
    }
    delay(10);
}
