#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include "Infrastructure/Config/config.h"
#include "Infrastructure/Config/NvsConfigRepository.h"

SET_LOOP_TASK_STACK_SIZE(16384);

RTC_NOINIT_ATTR static uint32_t g_rtcRelayMagic;
RTC_NOINIT_ATTR static uint8_t  g_rtcRelayMask;
static constexpr uint32_t RTC_RELAY_MAGIC = 0x524C5931U;

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

struct PendingWifiAdoption {
    bool pending = false;
    std::string targetSsid;
    std::string targetPass;
    std::string requestId;
    unsigned long executeAfterMs = 0;
};

struct PendingFactoryUnlink {
    bool pending = false;
    std::string reason;
    unsigned long executeAfterMs = 0;
};

static PendingWifiAdoption g_pendingWifiAdoption;
static PendingFactoryUnlink g_pendingFactoryUnlink;

static void scheduleFactoryUnlink(const std::string& reason, unsigned long delayMs = 200UL) {
    if (g_pendingFactoryUnlink.pending) {
        return;
    }
    g_pendingFactoryUnlink.pending = true;
    g_pendingFactoryUnlink.reason = reason;
    g_pendingFactoryUnlink.executeAfterMs = millis() + delayMs;
    Serial.printf("[UNLINK] Desvinculación de cuenta verificada (%s). Restauración a Quick Pairing programada en %lu ms...\n",
                  reason.c_str(), delayMs);
}

static void executeFactoryUnlinkToQuickPairing(const std::string& reason) {
    Serial.printf("[UNLINK] Ejecutando desvinculación de fábrica (%s): apagando relés, borrando credenciales y entrando en Quick Pairing...\n",
                  reason.c_str());

    if (g_relayService) {
        const unsigned long now = millis();
        for (const auto& r : g_relayService->getAllStates()) {
            g_relayService->setDesiredState(r.deviceId, false);
            g_relayService->applyDesiredState(r.deviceId, now, "ACCOUNT_UNLINK");
        }
    }
    g_rtcRelayMagic = 0U;
    g_rtcRelayMask = 0U;

    if (g_lanServer) {
        g_lanServer->loop(false);
    }
    infra::disconnectMQTT();

    g_configRepo.erase();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(150);

    ESP.restart();
}

static void executeHotWifiAdoption(const std::string& targetSsid,
                                   const std::string& targetPass,
                                   const std::string& requestId) {
    domain::DeviceConfig currentCfg;
    g_configRepo.load(currentCfg);

    const std::string prevSsid = currentCfg.wifiSsid;
    const std::string prevPass = currentCfg.wifiPass;

    Serial.printf("[WIFI-MQTT5] Probando adopción de nueva red Wi-Fi '%s'...\n", targetSsid.c_str());

    infra::disconnectMQTT();
    if (g_lanServer) {
        g_lanServer->loop(false);
    }

    WiFi.disconnect(false, false);
    delay(120);
    WiFi.mode(WIFI_STA);
    if (targetPass.empty()) {
        WiFi.begin(targetSsid.c_str());
    } else {
        WiFi.begin(targetSsid.c_str(), targetPass.c_str());
    }

    const unsigned long startMs = millis();
    wl_status_t lastStatus = WL_IDLE_STATUS;
    while ((millis() - startMs) < 15000UL) {
        lastStatus = WiFi.status();
        if (lastStatus == WL_CONNECTED) {
            break;
        }
        g_indicator.show(domain::IndicatorStatus::Connecting);
        g_indicator.update();
        delay(150);
    }

    const char* broker = !currentCfg.mqttBroker.empty() ? currentCfg.mqttBroker.c_str() : MQTT_SERVER;
    int port = currentCfg.mqttPort > 0 ? currentCfg.mqttPort : MQTT_PORT;
    const char* apiKey = !currentCfg.mqttUser.empty() ? currentCfg.mqttUser.c_str() : API_KEY;
    const char* devId = !g_activeDeviceId.empty()
        ? g_activeDeviceId.c_str()
        : (!currentCfg.deviceId.empty() ? currentCfg.deviceId.c_str() : "RELAY");

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI-MQTT5] Conectado con éxito a '%s' (IP: %s). Guardando en NVS...\n",
                      targetSsid.c_str(), WiFi.localIP().toString().c_str());
        currentCfg.wifiSsid = targetSsid;
        currentCfg.wifiPass = targetPass;
        g_configRepo.save(currentCfg);
        saveLastWifiErrorNvs("");
        g_wifi.setCredentials(targetSsid, targetPass);

        infra::initMQTT(broker, port, devId, apiKey, root_ca_pem, FIRMWARE_VERSION);
        if (hasValidBrokerCredentials() && infra::connectMQTT()) {
            g_timeProvider.sync();
            if (g_telemetry) {
                std::string desc = "Red Wi-Fi actualizada por MQTT5 a '" + targetSsid + "'";
                if (!requestId.empty()) {
                    desc += " (req=" + requestId + ")";
                }
                g_telemetry->publishEvent("WIFI_UPDATED", desc);
                g_telemetry->publishStatus();
            }
        }
        return;
    }

    const char* failReason = "timeout";
    if (lastStatus == WL_NO_SSID_AVAIL) {
        failReason = "ap_not_found";
    } else if (lastStatus == WL_CONNECT_FAILED) {
        failReason = "auth_failed";
    }

    Serial.printf("[WIFI-MQTT5] Falló conexión a '%s' (%s). Revirtiendo a '%s'...\n",
                  targetSsid.c_str(), failReason, prevSsid.c_str());

    WiFi.disconnect(false, false);
    delay(120);
    g_wifi.setCredentials(prevSsid, prevPass);
    if (!prevSsid.empty()) {
        g_wifi.connect();
    }

    infra::initMQTT(broker, port, devId, apiKey, root_ca_pem, FIRMWARE_VERSION);
    if (WiFi.status() == WL_CONNECTED && hasValidBrokerCredentials() && infra::connectMQTT()) {
        if (g_telemetry) {
            std::string desc = "No se pudo adoptar la red '" + targetSsid + "' (" + failReason + "); se mantuvo '" + prevSsid + "'";
            g_telemetry->publishEvent("WIFI_UPDATE_FAILED", desc);
            g_telemetry->publishStatus();
        }
    }
}

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
            reloadRuntimeConnectivity();
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
            if (!isInitialUnprovisionedBoot) {
                reloadRuntimeConnectivity();
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
    const esp_reset_reason_t resetReason = esp_reset_reason();
    const bool isPowerOnReset =
        (resetReason == ESP_RST_POWERON ||
         resetReason == ESP_RST_BROWNOUT ||
         resetReason == ESP_RST_EXT ||
         resetReason == ESP_RST_UNKNOWN);
    const uint8_t savedNvsMask = g_configRepo.loadRelayMask(devConfig.lastRelayState);
    const uint8_t effectiveSavedMask = (!isPowerOnReset && g_rtcRelayMagic == RTC_RELAY_MAGIC)
        ? g_rtcRelayMask
        : savedNvsMask;

    relayService.initWithStartupConfig(
        devConfig.startupMode,
        devConfig.lastRelayState,
        devConfig.pulseEnabled,
        devConfig.pulseWidthMs,
        millis(),
        isPowerOnReset,
        effectiveSavedMask
    );
    g_rtcRelayMagic = RTC_RELAY_MAGIC;
    g_rtcRelayMask = relayService.getRelayMask();
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
    Serial.printf("[INIT] AccountBound:  %s (NVS=%s)\n",
                  isAccountBound() ? "true" : "false",
                  isProv ? "provisioned" : "unconfigured");

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
    infra::setMqttAuthRejectedUnlinkCallback([](const String& proof) {
        if (verifyUnlinkProof(config.device_key.c_str(), g_activeDeviceId.c_str(), proof.c_str())) {
            scheduleFactoryUnlink("OFFLINE_TOMBSTONE_CONNACK", 120UL);
        } else {
            Serial.println("[UNLINK] CONNACK ACCOUNT_UNLINKED ignorado: unlink_proof inválido.");
        }
    });

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
    lanServer.setUnlinkRequestCallback([](const std::string& reason) {
        scheduleFactoryUnlink(reason, 200UL);
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
        if (g_relayService) {
            g_rtcRelayMagic = RTC_RELAY_MAGIC;
            g_rtcRelayMask = g_relayService->getRelayMask();
        }
        if (g_lanServer) {
            g_lanServer->notifyMdnsStateChange();
        }
    });
    controller.setOnWifiChangeRequestCallback(
        [](const std::string& ssid, const std::string& password, const std::string& requestId) {
            g_pendingWifiAdoption.pending = true;
            g_pendingWifiAdoption.targetSsid = ssid;
            g_pendingWifiAdoption.targetPass = password;
            g_pendingWifiAdoption.requestId = requestId;
            g_pendingWifiAdoption.executeAfterMs = millis() + 250UL;
        });
    controller.setOnAccountUnlinkRequestCallback(
        [](const std::string& source, const std::string& unlinkProof, const std::string&) {
            if (verifyUnlinkProof(config.device_key.c_str(), g_activeDeviceId.c_str(), unlinkProof.c_str())) {
                scheduleFactoryUnlink(source, 200UL);
            } else {
                Serial.println("[UNLINK] Solicitud MQTT de desvinculación rechazada: unlink_proof inválido.");
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
    if (g_pendingFactoryUnlink.pending &&
        static_cast<long>(millis() - g_pendingFactoryUnlink.executeAfterMs) >= 0) {
        const std::string reason = g_pendingFactoryUnlink.reason;
        g_pendingFactoryUnlink = PendingFactoryUnlink{};
        executeFactoryUnlinkToQuickPairing(reason);
        return;
    }
    if (g_pendingWifiAdoption.pending &&
        static_cast<long>(millis() - g_pendingWifiAdoption.executeAfterMs) >= 0) {
        PendingWifiAdoption req = g_pendingWifiAdoption;
        g_pendingWifiAdoption = PendingWifiAdoption{};
        executeHotWifiAdoption(req.targetSsid, req.targetPass, req.requestId);
    }
    if (g_relayService) {
        g_rtcRelayMagic = RTC_RELAY_MAGIC;
        g_rtcRelayMask = g_relayService->getRelayMask();
    }
    delay(1);
}
