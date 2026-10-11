#include <Arduino.h>
#include "Infrastructure/Config/config.h"

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
#include "Infrastructure/Provisioning/WifiApProvisioningPortal.h"
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

static app::RelayService*            g_relayService = nullptr;
static app::ConnectivityService*     g_connectivity = nullptr;
static app::TelemetryService*        g_telemetry = nullptr;
static app::RelayFirmwareController* g_controller = nullptr;

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n=============================================");
    Serial.println("  Relay-Wifi Clean Architecture - Firmware   ");
    Serial.printf("  Versión: %s\n", FIRMWARE_VERSION);
    Serial.println("=============================================\n");

    auto identity = g_deviceRepo.identity();
    std::string deviceId1 = identity.chipId + "-01";

    Serial.printf("[INIT] Chip ID:   %s\n", identity.chipId.c_str());
    Serial.printf("[INIT] MAC:       %s\n", identity.macAddress.c_str());
    Serial.printf("[INIT] Relay ID:  %s\n", deviceId1.c_str());

    app::MqttTopics topics = app::MqttTopics::forDevice(identity.chipId, "device_switch1");

    infra::initMQTT(MQTT_SERVER, MQTT_PORT, identity.chipId.c_str(),
                    API_KEY, root_ca_pem, FIRMWARE_VERSION);

    static app::RelayService relayService{g_logger};
    relayService.registerRelay(deviceId1, g_relay1, "device_switch1");
    g_relayService = &relayService;

    static app::ConnectivityService connectivity{g_wifi, g_transport, g_logger};
    g_connectivity = &connectivity;

    static app::TelemetryService telemetry{
        relayService, connectivity, g_deviceRepo, g_timeProvider,
        g_clock, g_serializer, topics, g_logger};
    g_telemetry = &telemetry;

    auto apPortalCallback = []() {
        infra::WifiApProvisioningPortal::startPortal(180);
    };

    static app::RelayFirmwareController controller{
        relayService, connectivity, telemetry, g_ota, g_indicator,
        g_system, g_timeProvider, g_clock, g_logger, topics, apPortalCallback};

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
