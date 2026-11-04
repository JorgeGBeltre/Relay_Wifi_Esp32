#include "Application/Controllers/RelayFirmwareController.h"
#include <cstdlib>
#include <cstring>

namespace relay::app {

RelayFirmwareController::RelayFirmwareController(
    RelayService& relays,
    ConnectivityService& connectivity,
    TelemetryService& telemetry,
    domain::IOtaService& ota,
    domain::IIndicator& indicator,
    domain::ISystemControl& system,
    domain::ITimeProvider& timeProvider,
    domain::IClock& clock,
    domain::ILogger& logger,
    const MqttTopics& topics,
    ApTriggerCallback apCallback,
    TimerSchedulerService* timerScheduler,
    domain::IConfigRepository* configRepo)
    : relays_(relays), connectivity_(connectivity), telemetry_(telemetry),
      ota_(ota), indicator_(indicator), system_(system), timeProvider_(timeProvider),
      clock_(clock), log_(logger), topics_(topics), apCallback_(std::move(apCallback)),
      timerScheduler_(timerScheduler), configRepo_(configRepo) {}

void RelayFirmwareController::registerButton(domain::IButtonHandler& button,
                                             const std::string& targetDeviceId) {
    button.init();
    buttons_.push_back(ButtonBinding{&button, targetDeviceId});
}

void RelayFirmwareController::updateIndicatorForConnectivity() {
    if (ota_.isInProgress()) {
        indicator_.show(domain::IndicatorStatus::OtaProgress);
    } else if (!connectivity_.network().isConnected()) {
        indicator_.show(domain::IndicatorStatus::Disconnected);
    } else if (!connectivity_.transport().isConnected()) {
        indicator_.show(domain::IndicatorStatus::Connecting);
    } else {
        indicator_.show(domain::IndicatorStatus::Operational);
    }
}

void RelayFirmwareController::setup(bool relaysAlreadyInitialized) {
    if (!relaysAlreadyInitialized) {
        log_.info("[CTRL] Inicializando sistema de relés...");
        relays_.init();
    }

    if (timerScheduler_) {
        timerScheduler_->loadFromStorage();
    }

    indicator_.show(domain::IndicatorStatus::Disconnected);

    connectivity_.transport().onMessage([this](const std::string& topic, const std::string& payload) {
        this->handleIncomingMessage(topic, payload);
    });

    connectivity_.transport().onConnect([this]() {
        this->onMqttConnected();
    });

    if (connectivity_.connect()) {
        state_ = State::Operational;
        indicator_.show(domain::IndicatorStatus::Operational);
        log_.info("[CTRL] Sistema OPERACIONAL");
    } else {
        state_ = State::Connecting;
        updateIndicatorForConnectivity();
    }
}

void RelayFirmwareController::onMqttConnected() {
    log_.info("[CTRL] MQTT conectado: suscribiendo a tópicos...");

    connectivity_.transport().subscribe(topics_.commands);
    connectivity_.transport().subscribe(topics_.deviceEvents);

    for (const auto& r : relays_.getAllStates()) {
        connectivity_.transport().subscribe(topics_.relay(r.topic, "/state"));
        connectivity_.transport().subscribe(topics_.relay(r.topic, "/command"));
        connectivity_.transport().subscribe(topics_.relay(r.topic, "_state"));
    }

    telemetry_.publishEvent("MQTT_CONNECTED", "Conexión MQTT establecida");
    telemetry_.requestCurrentStates();
    telemetry_.publishStatus();
    lastStatusPublishMs_ = clock_.nowMs();
}

void RelayFirmwareController::tick() {
    const unsigned long now = clock_.nowMs();

    // 1. Actualizar patrón del indicador LED (base de 100 ms)
    indicator_.update();

    // 2. El botón físico siempre es prioritario (Offline-First, incluso durante OTA o reconexión)
    handleButtonEvents();

    // 3. Ejecutar guardado diferido en NVS (150 ms tras última conmutación si StartupMode::Stay)
    relays_.flushDeferredSave(now);

    // 4. Evaluar temporizador one-shot de Modo Pulso (Inching)
    std::string expiredPulseDeviceId;
    if (relays_.updatePulseTimer(now, expiredPulseDeviceId)) {
        log_.info("[PULSE] Modo pulso expiró -> apagando automáticamente " + expiredPulseDeviceId);
        telemetry_.publishEvent("PULSE_AUTO_OFF", "Apagado automático por modo pulso (inching)", "off");
        telemetry_.publishRelayState(expiredPulseDeviceId);
        if (onStateChangeCb_) onStateChangeCb_();
    }

    // 5. Sincronizar NTP apenas haya enlace Wi-Fi y evaluar hasta 8 temporizadores programados una vez por segundo
    if (connectivity_.network().isConnected() &&
        (lastTimeSyncMs_ == 0 || (now - lastTimeSyncMs_) >= TIME_SYNC_INTERVAL_MS)) {
        lastTimeSyncMs_ = now;
        timeProvider_.sync();
    }

    if (timerScheduler_) {
        const uint32_t epochSec = timeProvider_.epochSeconds();
        if (epochSec > 1000000000U && epochSec != lastEvaluatedEpochSec_) {
            lastEvaluatedEpochSec_ = epochSec;
            if (timerScheduler_->evaluate(epochSec, now) > 0) {
                telemetry_.publishEvent("TIMER_TRIGGER", "Conmutación ejecutada por temporizador programado");
                telemetry_.publishAllRelayStates();
                if (onStateChangeCb_) onStateChangeCb_();
            }
        }
    }

    // 6. Gestión de OTA por chunks sobre MQTT 5.0 (MQTTOTAv5)
    if (ota_.isInProgress()) {
        indicator_.show(domain::IndicatorStatus::OtaProgress);
        ota_.handle();
        clock_.sleepMs(10);
        return;
    }

    ota_.handle();

    // 7. Máquina de estados de conectividad Wi-Fi + MQTT 5.0 no bloqueante
    if (state_ == State::Connecting) {
        updateIndicatorForConnectivity();
        if (lastConnectAttemptMs_ == 0 || (now - lastConnectAttemptMs_) >= RECONNECT_BACKOFF_MS) {
            lastConnectAttemptMs_ = now;
            if (connectivity_.ensureConnected()) {
                state_ = State::Operational;
                indicator_.show(domain::IndicatorStatus::Operational);
                log_.info("[CTRL] Estado -> OPERATIONAL");
            }
        }
    } else if (state_ == State::Operational) {
        if (!connectivity_.isOnline()) {
            updateIndicatorForConnectivity();
            state_ = State::Connecting;
            lastConnectAttemptMs_ = now;
            return;
        }

        indicator_.show(domain::IndicatorStatus::Operational);
        connectivity_.transport().loop();

        if (now - lastStatusPublishMs_ >= STATUS_INTERVAL_MS) {
            lastStatusPublishMs_ = now;
            telemetry_.publishStatus();
        }
    }
}

void RelayFirmwareController::handleButtonEvents() {
    const unsigned long now = clock_.nowMs();
    for (auto& btn : buttons_) {
        if (!btn.handler) continue;
        auto ev = btn.handler->update();

        if (ev == domain::ButtonEvent::ShortPress) {
            log_.info("[BUTTON] Pulsación corta detectada para " + btn.targetDeviceId);
            relays_.toggle(btn.targetDeviceId, now);

            const auto* state = relays_.getState(btn.targetDeviceId);
            std::string stateStr = (state && state->currentState) ? "on" : "off";
            std::string desc = "Cambio por botón físico - " + btn.targetDeviceId + ": " + (state && state->currentState ? "ENCENDIDO" : "APAGADO");

            telemetry_.publishEvent("BUTTON_PRESS", desc, stateStr);
            telemetry_.publishRelayState(btn.targetDeviceId);
            if (onStateChangeCb_) onStateChangeCb_();
        } else if (ev == domain::ButtonEvent::LongPress) {
            log_.info("[BUTTON] Pulsación larga (5s) detectada -> Activando Quick Pairing (BLE + SmartConfig / SoftAP VESIS)");
            telemetry_.publishEvent("PAIRING_MODE_ACTIVATED", "Modo emparejamiento activado por botón físico");
            telemetry_.publishStatus();

            if (apCallback_) {
                apCallback_();
            } else {
                system_.reboot();
            }
        }
    }
}

void RelayFirmwareController::handleIncomingMessage(const std::string& topic, const std::string& payload) {
    const unsigned long now = clock_.nowMs();
    log_.info("[CTRL] Mensaje recibido en [" + topic + "]: " + payload);

    if (topic == topics_.commands || topic == topics_.deviceEvents) {
        processCommand(payload);
        return;
    }

    for (const auto& r : relays_.getAllStates()) {
        std::string setTopic = topics_.relay(r.topic, "/state");
        std::string cmdTopic = topics_.relay(r.topic, "/command");
        std::string stateTopic = topics_.relay(r.topic, "_state");
        std::string legacySetTopic = r.topic + "/state";
        std::string legacyCmdTopic = r.topic + "/command";
        std::string legacyStateTopic = r.topic + "_state";

        if (topic == setTopic || topic == cmdTopic || topic == legacySetTopic || topic == legacyCmdTopic) {
            bool turnOn = (payload == "on" || payload == "1" || payload == "true");
            bool turnOff = (payload == "off" || payload == "0" || payload == "false");
            bool doToggle = (payload == "toggle");

            if (turnOn) {
                relays_.setDesiredState(r.deviceId, true);
                relays_.applyDesiredState(r.deviceId, now);
            } else if (turnOff) {
                relays_.setDesiredState(r.deviceId, false);
                relays_.applyDesiredState(r.deviceId, now);
            } else if (doToggle) {
                relays_.toggle(r.deviceId, now);
            }

            std::string stateStr = relays_.getState(r.deviceId)->currentState ? "on" : "off";
            telemetry_.publishEvent("DEVICE_SET_COMMAND", "Comando recibido: " + payload, stateStr);
            telemetry_.publishRelayState(r.deviceId);
            if (onStateChangeCb_) onStateChangeCb_();
            return;
        } else if (topic == stateTopic || topic == legacyStateTopic) {
            bool serverState = (payload == "on" || payload == "1" || payload == "true");
            relays_.setDesiredState(r.deviceId, serverState);
            relays_.applyDesiredState(r.deviceId, now);

            if (!relays_.isInitialSyncCompleted()) {
                relays_.setInitialSyncCompleted(true);
                telemetry_.publishEvent("INITIAL_STATE", "Estado inicial desde servidor", serverState ? "on" : "off");
            } else {
                telemetry_.publishEvent("STATE_UPDATE", "Actualización de estado desde servidor", serverState ? "on" : "off");
            }
            if (onStateChangeCb_) onStateChangeCb_();
            return;
        }
    }
}

void RelayFirmwareController::processCommand(const std::string& rawCmd) {
    const unsigned long now = clock_.nowMs();
    std::string cmd = rawCmd;

    // Compatibilidad defensiva con sobres JSON del servidor (p.ej. {"command":"...","payload":...})
    if (!cmd.empty() && cmd.front() == '{') {
        auto toLower = [](std::string s) {
            for (char& ch : s) {
                if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
            }
            return s;
        };
        std::string lower = toLower(cmd);
        auto extractChannelFromJson = [&](const std::string& s) -> int {
            for (const char* key : {"\"channel\":", "\\\"channel\\\":", "\"outlet\":", "\\\"outlet\\\":", "\"relay\":", "\\\"relay\\\":"}) {
                size_t kpos = s.find(key);
                if (kpos != std::string::npos) {
                    size_t vpos = kpos + std::strlen(key);
                    while (vpos < s.size() && (s[vpos] == ' ' || s[vpos] == '"' || s[vpos] == '\\')) ++vpos;
                    if (vpos < s.size() && s[vpos] >= '0' && s[vpos] <= '9') {
                        int parsed = std::atoi(s.c_str() + vpos);
                        const bool isOutlet = (std::strstr(key, "outlet") != nullptr);
                        int ch = isOutlet ? (parsed + 1) : (parsed <= 0 ? 1 : parsed);
                        if (ch >= 1 && ch <= 16) return ch;
                    }
                }
            }
            return 1;
        };
        const bool isAllCmd = (lower.find("\"set_power_all\"") != std::string::npos ||
                               lower.find("\"all\":true") != std::string::npos);
        const int targetCh = extractChannelFromJson(lower);

        if (lower.find("\"reset\"") != std::string::npos || lower.find("\"reboot\"") != std::string::npos) {
            cmd = "reset";
        } else if (lower.find("\"toggle_all\"") != std::string::npos || (isAllCmd && lower.find("\"toggle\"") != std::string::npos)) {
            cmd = "toggle_all";
        } else if (lower.find("\"switch\":\"on\"") != std::string::npos ||
                   lower.find("\\\"switch\\\":\\\"on\\\"") != std::string::npos ||
                   lower.find("\"state\":\"on\"") != std::string::npos ||
                   lower.find("\\\"state\\\":\\\"on\\\"") != std::string::npos ||
                   lower.find("\"state\":true") != std::string::npos ||
                   lower.find("\\\"state\\\":true") != std::string::npos ||
                   lower.find("\"state\":1") != std::string::npos ||
                   lower.find("\\\"state\\\":1") != std::string::npos ||
                   lower.find("\"payload\":\"on\"") != std::string::npos) {
            cmd = isAllCmd ? "all:on" : ("switch" + std::to_string(targetCh) + ":on");
        } else if (lower.find("\"switch\":\"off\"") != std::string::npos ||
                   lower.find("\\\"switch\\\":\\\"off\\\"") != std::string::npos ||
                   lower.find("\"state\":\"off\"") != std::string::npos ||
                   lower.find("\\\"state\\\":\\\"off\\\"") != std::string::npos ||
                   lower.find("\"state\":false") != std::string::npos ||
                   lower.find("\\\"state\\\":false") != std::string::npos ||
                   lower.find("\"state\":0") != std::string::npos ||
                   lower.find("\\\"state\\\":0") != std::string::npos ||
                   lower.find("\"payload\":\"off\"") != std::string::npos) {
            cmd = isAllCmd ? "all:off" : ("switch" + std::to_string(targetCh) + ":off");
        } else if (lower.find("\"toggle\"") != std::string::npos) {
            cmd = "switch" + std::to_string(targetCh) + ":toggle";
        }
    }

    if (cmd == "reset") {
        telemetry_.publishEvent("SYSTEM_RESET", "Reinicio del sistema solicitado");
        clock_.sleepMs(1000);
        system_.reboot();
    } else if (cmd == "status") {
        telemetry_.publishStatus();
    } else if (cmd == "toggle_all") {
        relays_.toggleAll(now);
        const auto states = relays_.getAllStates();
        const std::string newStateStr = (!states.empty() && states.front().currentState) ? "on" : "off";
        telemetry_.publishEvent("RELAY_TOGGLE_ALL", "Todos los relays cambiados de estado", newStateStr);
        telemetry_.publishStatus();
        telemetry_.publishAllRelayStates();
        if (onStateChangeCb_) onStateChangeCb_();
    } else if (cmd == "all:on" || cmd == "all:off") {
        const bool targetOn = (cmd == "all:on");
        for (const auto& r : relays_.getAllStates()) {
            relays_.setDesiredState(r.deviceId, targetOn);
            relays_.applyDesiredState(r.deviceId, now);
        }
        telemetry_.publishEvent("DEVICE_COMMAND", targetOn ? "Comando all:on" : "Comando all:off", targetOn ? "on" : "off");
        telemetry_.publishStatus();
        telemetry_.publishAllRelayStates();
        if (onStateChangeCb_) onStateChangeCb_();
    } else if (cmd == "sync_states") {
        telemetry_.publishEvent("STATE_SYNC", "Sincronización de estados solicitada");
        relays_.setInitialSyncCompleted(false);
        telemetry_.requestCurrentStates();
    } else if (cmd.rfind("switch", 0) == 0 && cmd.find(':') != std::string::npos) {
        const size_t colonPos = cmd.find(':');
        const std::string chStr = cmd.substr(6, colonPos - 6);
        const size_t chOneBased = chStr.empty() ? 1U : static_cast<size_t>(std::strtoul(chStr.c_str(), nullptr, 10));
        const size_t targetIdx = (chOneBased > 0) ? (chOneBased - 1U) : 0U;
        const std::string subcmd = cmd.substr(colonPos + 1);
        const auto allRelays = relays_.getAllStates();
        if (!allRelays.empty() && targetIdx < allRelays.size()) {
            const std::string targetId = allRelays[targetIdx].deviceId;
            bool applied = false;
            if (subcmd == "on" || subcmd == "1" || subcmd == "true") {
                relays_.setDesiredState(targetId, true);
                relays_.applyDesiredState(targetId, now);
                applied = true;
            } else if (subcmd == "off" || subcmd == "0" || subcmd == "false") {
                relays_.setDesiredState(targetId, false);
                relays_.applyDesiredState(targetId, now);
                applied = true;
            } else if (subcmd == "toggle") {
                relays_.toggle(targetId, now);
                applied = true;
            }
            if (applied) {
                const auto* updated = relays_.getState(targetId);
                const std::string stateStr = (updated && updated->currentState) ? "on" : "off";
                telemetry_.publishRelayState(targetId);
                telemetry_.publishStatus();
                telemetry_.publishEvent("DEVICE_COMMAND", "Comando switch" + std::to_string(targetIdx + 1U) + ": " + subcmd, stateStr);
                if (onStateChangeCb_) onStateChangeCb_();
            }
        }
    } else if (cmd.rfind("startup:", 0) == 0) {
        std::string modeStr = cmd.substr(8);
        domain::StartupMode mode = domain::startupModeFromString(modeStr);
        relays_.setStartupMode(mode);
        telemetry_.publishEvent("STARTUP_MODE_CHANGED", "StartupMode configurado a " + std::string(domain::startupModeToString(mode)));
        if (onStateChangeCb_) onStateChangeCb_();
    } else if (cmd.rfind("pulse:", 0) == 0) {
        std::string sub = cmd.substr(6);
        if (sub == "off" || sub == "0") {
            relays_.configurePulse(false, relays_.getPulseWidthMs(), now);
            telemetry_.publishEvent("PULSE_CONFIG_CHANGED", "Modo pulso desactivado");
        } else if (sub.rfind("on,", 0) == 0) {
            uint32_t widthMs = static_cast<uint32_t>(std::strtoul(sub.substr(3).c_str(), nullptr, 10));
            if (relays_.configurePulse(true, widthMs, now)) {
                telemetry_.publishEvent("PULSE_CONFIG_CHANGED", "Modo pulso activado (" + std::to_string(widthMs) + " ms)");
            }
        }
        if (onStateChangeCb_) onStateChangeCb_();
    } else if (cmd.rfind("sled:", 0) == 0) {
        std::string sub = cmd.substr(5);
        bool en = (sub == "on" || sub == "1" || sub == "true");
        indicator_.setSledOnline(en);
        if (configRepo_) {
            configRepo_->saveSledOnline(en);
        }
        telemetry_.publishEvent("SLED_CONFIG_CHANGED", std::string("Indicador LED online: ") + (en ? "on" : "off"));
        if (onStateChangeCb_) onStateChangeCb_();
    }
}

}
