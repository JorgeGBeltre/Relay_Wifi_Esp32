#include <Arduino.h>
#include <WiFi.h>
#include "Infrastructure/Config/config.h"
#include "Infrastructure/Config/NvsConfigRepository.h"

#include "Application/Controllers/RelayFirmwareController.h"
#include "Application/Messaging/MqttTopics.h"
#include "Application/Services/ConnectivityService.h"
#include "Application/Services/RelayService.h"
#include "Application/Services/TelemetryService.h"
#include "Application/Services/TimerSchedulerService.h"

#include "Infrastructure/Actuators/GpioRelay.h"
#include "Infrastructure/Button/GpioButton.h"
#include "Infrastructure/Indicator/GpioIndicator.h"
#include "Infrastructure/Logging/SerialLogger.h"
#include "Infrastructure/Messaging/Mqttv5Transport.h"
#include "Infrastructure/Messaging/mqtt_client.h"
#include "Infrastructure/Net/LanZeroconfServer.h"
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
#if RELAY_CHANNEL_COUNT >= 2
static infra::GpioRelay                    g_relay2{RelayPin2};
static infra::GpioButton                   g_button2{BUTTON_PIN2, DEBOUNCE_TIME, BUTTON_LONG_PRESS_TIME};
#endif
#if RELAY_CHANNEL_COUNT >= 3
static infra::GpioRelay                    g_relay3{RelayPin3};
static infra::GpioButton                   g_button3{BUTTON_PIN3, DEBOUNCE_TIME, BUTTON_LONG_PRESS_TIME};
#endif
#if RELAY_CHANNEL_COUNT >= 4
static infra::GpioRelay                    g_relay4{RelayPin4};
static infra::GpioButton                   g_button4{BUTTON_PIN4, DEBOUNCE_TIME, BUTTON_LONG_PRESS_TIME};
#endif
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
static app::TimerSchedulerService*   g_timerScheduler = nullptr;
static app::RelayFirmwareController* g_controller = nullptr;
static infra::LanZeroconfServer*     g_lanServer = nullptr;

static std::string g_activeDeviceId;

static void reloadRuntimeConnectivity() {
    domain::DeviceConfig updatedCfg;
    g_configRepo.load(updatedCfg);

    std::string prevSsid;
    // Si cambió el SSID respecto a la red actualmente asociada, reconectar Wi-Fi
    if (!updatedCfg.wifiSsid.empty() &&
        WiFi.status() == WL_CONNECTED &&
        std::string(WiFi.SSID().c_str()) != updatedCfg.wifiSsid) {
        WiFi.disconnect(false, false);
        delay(80);
    }

    g_wifi.setCredentials(updatedCfg.wifiSsid, updatedCfg.wifiPass);
    if (WiFi.status() != WL_CONNECTED && !updatedCfg.wifiSsid.empty()) {
        g_wifi.connect();
    }

    const char* broker = !updatedCfg.mqttBroker.empty() ? updatedCfg.mqttBroker.c_str() : MQTT_SERVER;
    int port = updatedCfg.mqttPort > 0 ? updatedCfg.mqttPort : MQTT_PORT;
    const char* apiKey = !updatedCfg.mqttUser.empty() ? updatedCfg.mqttUser.c_str() : API_KEY;
    const char* devId = !g_activeDeviceId.empty()
        ? g_activeDeviceId.c_str()
        : (!updatedCfg.deviceId.empty() ? updatedCfg.deviceId.c_str() : "RELAY");

    infra::disconnectMQTT();
    infra::initMQTT(broker, port, devId, apiKey, root_ca_pem, FIRMWARE_VERSION);
    if (WiFi.status() == WL_CONNECTED && hasValidBrokerCredentials()) {
        infra::connectMQTT();
    }
}

// Ejecuta el flujo de emparejamiento de 2 etapas:
// - Dispositivo sin credenciales (fábrica): permanece continuamente descubrible en Quick Pairing
//   (BLE GATT + SmartConfig ESP-Touch) hasta recibir configuración, permitiendo conmutar a SoftAP con 5s de botón.
// - Dispositivo ya configurado (pulsación larga de 5s en operación): ventana interactiva de recuperación/reconfiguración.
static void runPairingWorkflow(bool isInitialUnprovisionedBoot) {
    do {
        Serial.println("[PAIRING] Entrando en Etapa 1: Quick Pairing (BLE + SmartConfig)...");
        QuickPairingResult res = g_blePortal.runInteractive(
            180000UL,
            g_indicator,
            g_button1
        );

        if (res == QuickPairingResult::Provisioned) {
            Serial.println("[PAIRING] Credenciales recibidas y verificadas en Quick Pairing.");
            if (!isInitialUnprovisionedBoot) {
                reloadRuntimeConnectivity();
            }
            return;
        }

        if (res == QuickPairingResult::CancelledByButton && !isInitialUnprovisionedBoot) {
            Serial.println("[PAIRING] Emparejamiento cancelado por pulsación corta. Retomando operación normal.");
            return;
        }

        if (res == QuickPairingResult::SwitchToSoftAp) {
            Serial.println("[PAIRING] Entrando en Etapa 2: Compatible SoftAP (VESIS-<ChipID>)...");
            bool savedViaAp = infra::WifiApProvisioningPortal::startPortal(180, &g_indicator, &g_button1);
            if (savedViaAp) {
                if (!isInitialUnprovisionedBoot) {
                    reloadRuntimeConnectivity();
                }
                return;
            }
        }
    } while (isInitialUnprovisionedBoot && !g_configRepo.isProvisioned());
}

void setup() {
    Serial.begin(115200);

    // 1. Inicializar identidad y configuración NVS inmediatamente al arrancar
    auto identity = g_deviceRepo.identity();
    g_configRepo.begin(identity.chipId.c_str());

    domain::DeviceConfig devConfig;
    bool isProv = g_configRepo.load(devConfig);

    std::string activeDeviceId = !devConfig.deviceId.empty() ? devConfig.deviceId : identity.chipId;
    g_activeDeviceId = activeDeviceId;
    std::string deviceId1 = activeDeviceId + "-01";

    // 2. Restaurar estado del relé (< 20 ms, Offline-First) según StartupMode (Off, On, Stay) y pulso
    g_indicator.setSledOnline(devConfig.sledOnline);
    static app::RelayService relayService{g_logger, &g_configRepo};
    relayService.registerRelay(deviceId1, g_relay1, "device_switch1");
#if RELAY_CHANNEL_COUNT >= 2
    std::string deviceId2 = activeDeviceId + "-02";
    relayService.registerRelay(deviceId2, g_relay2, "device_switch2");
#endif
#if RELAY_CHANNEL_COUNT >= 3
    std::string deviceId3 = activeDeviceId + "-03";
    relayService.registerRelay(deviceId3, g_relay3, "device_switch3");
#endif
#if RELAY_CHANNEL_COUNT >= 4
    std::string deviceId4 = activeDeviceId + "-04";
    relayService.registerRelay(deviceId4, g_relay4, "device_switch4");
#endif
    relayService.initWithStartupConfig(
        devConfig.startupMode,
        devConfig.lastRelayState,
        devConfig.pulseEnabled,
        devConfig.pulseWidthMs,
        millis()
    );
    g_relayService = &relayService;

    Serial.println("\n=============================================");
    Serial.println("  Relay-Wifi Clean Architecture - Firmware   ");
    Serial.printf("  Versión: %s\n", FIRMWARE_VERSION);
    Serial.println("=============================================\n");
    Serial.printf("[INIT] Chip ID:       %s\n", identity.chipId.c_str());
    Serial.printf("[INIT] Device ID:     %s\n", activeDeviceId.c_str());
    Serial.printf("[INIT] MAC:           %s\n", identity.macAddress.c_str());
    Serial.printf("[INIT] Relay ID:      %s\n", deviceId1.c_str());
    Serial.printf("[INIT] StartupMode:   %s (Pulso=%s, %u ms, SLED=%s)\n",
                  domain::startupModeToString(devConfig.startupMode),
                  devConfig.pulseEnabled ? "on" : "off",
                  devConfig.pulseWidthMs,
                  devConfig.sledOnline ? "on" : "off");

    // 3. Si no está aprovisionado en el primer arranque, iniciar flujo de emparejamiento
    if (!isProv) {
        g_button1.init();
        runPairingWorkflow(true);
        g_configRepo.load(devConfig);
    }

    // 4. Configurar enlace Wi-Fi y cliente nativo MQTT 5.0 (ESP32_MQTTv5 + MQTTOTAv5)
    g_wifi.setCredentials(devConfig.wifiSsid, devConfig.wifiPass);

    const char* broker = !devConfig.mqttBroker.empty() ? devConfig.mqttBroker.c_str() : MQTT_SERVER;
    int port = devConfig.mqttPort > 0 ? devConfig.mqttPort : MQTT_PORT;
    const char* apiKey = !devConfig.mqttUser.empty() ? devConfig.mqttUser.c_str() : API_KEY;

    app::MqttTopics topics = app::MqttTopics::forDevice(activeDeviceId, "device_switch1");

    infra::initMQTT(broker, port, activeDeviceId.c_str(),
                    apiKey, root_ca_pem, FIRMWARE_VERSION);

    static app::ConnectivityService connectivity{g_wifi, g_transport, g_logger};
    g_connectivity = &connectivity;

    static app::TelemetryService telemetry{
        relayService, connectivity, g_deviceRepo, g_timeProvider,
        g_clock, g_serializer, topics, g_logger};
    g_telemetry = &telemetry;

    static app::TimerSchedulerService timerScheduler{
        relayService, deviceId1, &g_configRepo};
    g_timerScheduler = &timerScheduler;

    static infra::LanZeroconfServer lanServer{
        relayService, telemetry, timerScheduler, g_indicator,
        g_configRepo, activeDeviceId, deviceId1, LAN_HTTP_PORT};
    lanServer.setCredentialsReloadCallback([]() {
        Serial.println("[LAN] Credenciales actualizadas por /zeroconf -> recargando conectividad en caliente...");
        reloadRuntimeConnectivity();
    });
    g_lanServer = &lanServer;

    auto provisioningCallback = []() {
        runPairingWorkflow(false);
    };

    static app::RelayFirmwareController controller{
        relayService, connectivity, telemetry, g_ota, g_indicator,
        g_system, g_timeProvider, g_clock, g_logger, topics,
        provisioningCallback, &timerScheduler, &g_configRepo};

    controller.registerButton(g_button1, deviceId1);
#if RELAY_CHANNEL_COUNT >= 2
    controller.registerButton(g_button2, deviceId2);
#endif
#if RELAY_CHANNEL_COUNT >= 3
    controller.registerButton(g_button3, deviceId3);
#endif
#if RELAY_CHANNEL_COUNT >= 4
    controller.registerButton(g_button4, deviceId4);
#endif
    controller.setOnLocalStateChangeCallback([]() {
        if (g_lanServer) {
            g_lanServer->notifyMdnsStateChange();
        }
    });

    controller.setup(true);
    g_controller = &controller;

    if (g_wifi.isConnected()) {
        g_timeProvider.sync();
    }
}

void loop() {
    if (g_controller) {
        g_controller->tick();
    }
    if (g_lanServer) {
        g_lanServer->loop(g_wifi.isConnected());
    }
    delay(5);
}
