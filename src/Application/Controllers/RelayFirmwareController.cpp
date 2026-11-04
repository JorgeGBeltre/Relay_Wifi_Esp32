#include "Application/Controllers/RelayFirmwareController.h"
#include "Application/Services/TimerJsonCodec.h"
#include <ArduinoJson.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
inline uint32_t readMicrosSafe() {
#ifdef ARDUINO
    return static_cast<uint32_t>(micros());
#else
    return 0U;
#endif
}
} // namespace

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
    if (!topics_.timersCommand.empty()) {
        connectivity_.transport().subscribe(topics_.timersCommand);
    }
    if (!topics_.wifiCommand.empty()) {
        connectivity_.transport().subscribe(topics_.wifiCommand);
    }

    for (const auto& r : relays_.getAllStates()) {
        connectivity_.transport().subscribe(topics_.relay(r.topic, "/state"));
        connectivity_.transport().subscribe(topics_.relay(r.topic, "/command"));
    }

    relays_.setInitialSyncCompleted(true);
    telemetry_.publishStatus();
    telemetry_.publishEvent("MQTT_CONNECTED", "Conexión MQTT establecida", "", relays_.getLastSource());
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
        telemetry_.publishRelayState(expiredPulseDeviceId);
        telemetry_.publishStatus();
        telemetry_.publishEvent("PULSE_AUTO_OFF", "Apagado automático por modo pulso (inching)", "off", "PULSE_AUTO_OFF");
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
            bool timersMutated = false;
            const size_t triggered = timerScheduler_->evaluate(epochSec, now, &timersMutated);
            if (triggered > 0) {
                telemetry_.publishAllRelayStates();
                telemetry_.publishStatus();
                if (timersMutated) {
                    telemetry_.publishTimersEvent("TIMER_TRIGGER", "Conmutación ejecutada por temporizador programado", *timerScheduler_);
                } else {
                    telemetry_.publishEvent("TIMER_TRIGGER", "Conmutación ejecutada por temporizador programado", "", "AUTOMATION_TIMER");
                }
                if (onStateChangeCb_) onStateChangeCb_();
            } else if (timersMutated) {
                telemetry_.publishTimersEvent(
                    "TIMERS_UPDATED",
                    "Temporizadores vencidos deshabilitados sin accionar relé",
                    *timerScheduler_);
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
            relays_.toggle(btn.targetDeviceId, now, "PHYSICAL_BUTTON");

            const auto* state = relays_.getState(btn.targetDeviceId);
            std::string stateStr = (state && state->currentState) ? "on" : "off";
            std::string desc = "Cambio por botón físico - " + btn.targetDeviceId + ": " + (state && state->currentState ? "ENCENDIDO" : "APAGADO");

            telemetry_.publishRelayState(btn.targetDeviceId);
            telemetry_.publishStatus();
            telemetry_.publishEvent("BUTTON_PRESS", desc, stateStr, "PHYSICAL_BUTTON");
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
    const bool containsSensitiveWifi =
        (!topics_.wifiCommand.empty() && topic == topics_.wifiCommand) ||
        payload.find("\"password\"") != std::string::npos ||
        payload.find("\"wifi_pass\"") != std::string::npos;

    if (containsSensitiveWifi) {
        log_.info("[CTRL] Mensaje recibido en [" + topic + "]: [WIFI_CREDENTIALS_REDACTED]");
    } else {
        log_.info("[CTRL] Mensaje recibido en [" + topic + "]: " + payload);
    }

    if (!topics_.timersCommand.empty() && topic == topics_.timersCommand) {
        handleTimersPayload(payload);
        return;
    }

    if (!topics_.wifiCommand.empty() && topic == topics_.wifiCommand) {
        handleWifiPayload(payload);
        return;
    }

    if (topic == topics_.commands || topic == topics_.deviceEvents) {
        processCommand(payload);
        return;
    }

    for (const auto& r : relays_.getAllStates()) {
        std::string setTopic = topics_.relay(r.topic, "/state");
        std::string cmdTopic = topics_.relay(r.topic, "/command");
        std::string legacySetTopic = r.topic + "/state";
        std::string legacyCmdTopic = r.topic + "/command";

        if (topic == setTopic || topic == cmdTopic || topic == legacySetTopic || topic == legacyCmdTopic) {
            bool turnOn = (payload == "on" || payload == "1" || payload == "true");
            bool turnOff = (payload == "off" || payload == "0" || payload == "false");
            bool doToggle = (payload == "toggle");

            if (turnOn) {
                relays_.setDesiredState(r.deviceId, true);
                relays_.applyDesiredState(r.deviceId, now, "REMOTE_COMMAND");
            } else if (turnOff) {
                relays_.setDesiredState(r.deviceId, false);
                relays_.applyDesiredState(r.deviceId, now, "REMOTE_COMMAND");
            } else if (doToggle) {
                relays_.toggle(r.deviceId, now, "REMOTE_COMMAND");
            } else {
                return;
            }

            std::string stateStr = relays_.getState(r.deviceId)->currentState ? "on" : "off";
            telemetry_.publishRelayState(r.deviceId);
            telemetry_.publishStatus();
            telemetry_.publishEvent("DEVICE_SET_COMMAND", "Comando recibido: " + payload, stateStr, "REMOTE_COMMAND");
            if (onStateChangeCb_) onStateChangeCb_();
            return;
        }
    }
}

bool RelayFirmwareController::findProcessedCommand(const std::string& commandId,
                                                   ProcessedCommandEntry& outEntry) const {
    if (commandId.empty()) return false;
    for (size_t i = 0; i < CMD_DEDUP_CAPACITY; ++i) {
        if (recentCommands_[i].commandId[0] != '\0' &&
            commandId == recentCommands_[i].commandId) {
            outEntry = recentCommands_[i];
            return true;
        }
    }
    return false;
}

void RelayFirmwareController::recordProcessedCommand(const std::string& commandId,
                                                     uint64_t sequence,
                                                     int channel,
                                                     bool reportedState) {
    if (commandId.empty()) return;
    ProcessedCommandEntry& slot = recentCommands_[recentCommandHead_];
    std::snprintf(slot.commandId, sizeof(slot.commandId), "%s", commandId.c_str());
    slot.sequence = sequence;
    slot.stateVersion = relays_.getStateVersion();
    slot.channel = channel;
    slot.reportedState = reportedState;
    recentCommandHead_ = (recentCommandHead_ + 1U) % CMD_DEDUP_CAPACITY;
}

void RelayFirmwareController::processCommand(const std::string& rawCmd) {
    const unsigned long now = clock_.nowMs();
    const uint32_t startUs = readMicrosSafe();
    std::string cmd = rawCmd;
    std::string commandId;
    uint64_t sequence = 0;
    int parsedChannel = 1;

    if (!cmd.empty() && cmd.front() == '{') {
        DynamicJsonDocument doc(1536);
        if (deserializeJson(doc, cmd) == DeserializationError::Ok) {
            const char* rawCmdId = doc["commandId"] | (doc["command_id"] | "");
            if (rawCmdId && rawCmdId[0] != '\0') {
                commandId = rawCmdId;
            }
            if (doc.containsKey("sequence")) {
                sequence = doc["sequence"].as<uint64_t>();
            }

            const char* actionStr = doc["action"] | (doc["command"] | "");
            if (std::strcmp(actionStr, "set_timers") == 0 || doc["timers"].is<JsonArray>()) {
                handleTimersPayload(rawCmd);
                return;
            }
            if (std::strcmp(actionStr, "set_wifi") == 0) {
                handleWifiPayload(rawCmd);
                return;
            }

            JsonVariantConst payloadVar = doc["payload"];
            JsonObjectConst payloadObj;
            DynamicJsonDocument nestedPayloadDoc(512);
            if (payloadVar.is<JsonObjectConst>()) {
                payloadObj = payloadVar.as<JsonObjectConst>();
            } else if (payloadVar.is<const char*>()) {
                const char* pStr = payloadVar.as<const char*>();
                if (pStr && pStr[0] == '{' && deserializeJson(nestedPayloadDoc, pStr) == DeserializationError::Ok) {
                    payloadObj = nestedPayloadDoc.as<JsonObjectConst>();
                }
            }

            if (doc.containsKey("channel")) {
                int ch = doc["channel"].as<int>();
                if (ch >= 1 && ch <= 16) parsedChannel = ch;
            } else if (!payloadObj.isNull() && payloadObj.containsKey("channel")) {
                int ch = payloadObj["channel"].as<int>();
                if (ch >= 1 && ch <= 16) parsedChannel = ch;
            } else if (doc.containsKey("outlet")) {
                int outlet = doc["outlet"].as<int>();
                if (outlet >= 0 && outlet < 16) parsedChannel = outlet + 1;
            } else if (!payloadObj.isNull() && payloadObj.containsKey("outlet")) {
                int outlet = payloadObj["outlet"].as<int>();
                if (outlet >= 0 && outlet < 16) parsedChannel = outlet + 1;
            }

            // 1. Idempotencia estricta por commandId
            ProcessedCommandEntry prev{};
            if (!commandId.empty() && findProcessedCommand(commandId, prev)) {
                log_.info("[CTRL] Comando duplicado ignorado por idempotencia: " + commandId);
                telemetry_.publishCommandAck(commandId, prev.sequence, "APPLIED", "REMOTE_COMMAND", 0U, prev.channel, true);
                return;
            }

            // 2. Ordenamiento monótono por sequence: jamás permitir que un comando viejo pise uno más nuevo
            const uint64_t effectiveLastSeq = relays_.getLastCommandSequence() > lastCommandSequence_
                ? relays_.getLastCommandSequence()
                : lastCommandSequence_;
            if (sequence > 0 && effectiveLastSeq > 0 && sequence < effectiveLastSeq) {
                log_.warn("[CTRL] Comando obsoleto rechazado (seq=" + std::to_string(sequence) +
                          " < last=" + std::to_string(effectiveLastSeq) + ")");
                telemetry_.publishCommandAck(
                    commandId,
                    sequence,
                    "STALE_REJECTED",
                    "REMOTE_COMMAND",
                    0U,
                    parsedChannel,
                    false,
                    "Out-of-order stale command sequence rejected");
                return;
            }

            // 3. Si el campo "command" ya trae un comando canónico (ej. "switch1:off", "startup:on", "pulse:on,500", "sled:on")
            if (doc["command"].is<const char*>()) {
                std::string explicitCmd = doc["command"].as<const char*>();
                if (explicitCmd.rfind("switch", 0) == 0 ||
                    explicitCmd.rfind("startup:", 0) == 0 ||
                    explicitCmd.rfind("pulse:", 0) == 0 ||
                    explicitCmd.rfind("sled:", 0) == 0 ||
                    explicitCmd == "all:on" ||
                    explicitCmd == "all:off" ||
                    explicitCmd == "toggle_all" ||
                    explicitCmd == "reset" ||
                    explicitCmd == "status") {
                    cmd = explicitCmd;
                }
            }

            if (cmd.front() == '{') {
                std::string actLower = actionStr;
                for (char& ch : actLower) {
                    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
                }
                const bool isAll = (actLower == "set_power_all" || (doc["all"] | false) || (!payloadObj.isNull() && (payloadObj["all"] | false)));

                auto resolveBoolState = [&](bool& outHasBool, bool& outVal, bool& outToggle) {
                    outHasBool = false;
                    outVal = false;
                    outToggle = false;

                    auto inspectVar = [&](JsonVariantConst v) -> bool {
                        if (v.isNull()) return false;
                        if (v.is<bool>()) {
                            outHasBool = true;
                            outVal = v.as<bool>();
                            return true;
                        }
                        if (v.is<int>()) {
                            outHasBool = true;
                            outVal = (v.as<int>() != 0);
                            return true;
                        }
                        if (v.is<const char*>()) {
                            const char* s = v.as<const char*>();
                            if (!s) return false;
                            if (std::strcmp(s, "on") == 0 || std::strcmp(s, "ON") == 0 || std::strcmp(s, "true") == 0 || std::strcmp(s, "1") == 0) {
                                outHasBool = true;
                                outVal = true;
                                return true;
                            }
                            if (std::strcmp(s, "off") == 0 || std::strcmp(s, "OFF") == 0 || std::strcmp(s, "false") == 0 || std::strcmp(s, "0") == 0) {
                                outHasBool = true;
                                outVal = false;
                                return true;
                            }
                            if (std::strcmp(s, "toggle") == 0 || std::strcmp(s, "TOGGLE") == 0) {
                                outToggle = true;
                                return true;
                            }
                        }
                        return false;
                    };

                    if (inspectVar(doc["desiredState"])) return;
                    if (inspectVar(doc["switch"])) return;
                    if (inspectVar(doc["state"])) return;
                    if (!payloadObj.isNull()) {
                        if (inspectVar(payloadObj["desiredState"])) return;
                        if (inspectVar(payloadObj["switch"])) return;
                        if (inspectVar(payloadObj["state"])) return;
                    }
                    if (payloadVar.is<const char*>()) {
                        inspectVar(payloadVar);
                    }
                };

                bool hasState = false;
                bool desiredOn = false;
                bool isToggle = (actLower == "toggle" || actLower == "toggle_switch" || actLower == "toggle_all");
                resolveBoolState(hasState, desiredOn, isToggle);

                if (actLower == "reset" || actLower == "reboot") {
                    cmd = "reset";
                } else if (actLower == "toggle_all" || (isAll && isToggle)) {
                    cmd = "toggle_all";
                } else if (hasState) {
                    cmd = isAll
                        ? (desiredOn ? "all:on" : "all:off")
                        : ("switch" + std::to_string(parsedChannel) + (desiredOn ? ":on" : ":off"));
                } else if (isToggle) {
                    cmd = "switch" + std::to_string(parsedChannel) + ":toggle";
                }
            }
        }
    }

    if (sequence > 0) {
        lastCommandSequence_ = sequence;
        relays_.setLastCommandSequence(sequence);
    }

    auto emitAckIfNeeded = [&](int ackCh, bool reported) {
        if (!commandId.empty()) {
            const uint32_t execUs = readMicrosSafe() - startUs;
            recordProcessedCommand(commandId, sequence, ackCh, reported);
            telemetry_.publishCommandAck(commandId, sequence, "APPLIED", "REMOTE_COMMAND", execUs, ackCh, false);
        }
    };

    if (cmd == "reset") {
        emitAckIfNeeded(1, false);
        telemetry_.publishEvent("SYSTEM_RESET", "Reinicio del sistema solicitado", "", "REMOTE_COMMAND");
        clock_.sleepMs(500);
        system_.reboot();
    } else if (cmd == "status") {
        telemetry_.publishStatus();
        const auto states = relays_.getAllStates();
        emitAckIfNeeded(1, !states.empty() && states.front().currentState);
    } else if (cmd == "toggle_all") {
        relays_.toggleAll(now, "REMOTE_COMMAND");
        const auto states = relays_.getAllStates();
        const bool firstOn = (!states.empty() && states.front().currentState);
        const std::string newStateStr = firstOn ? "on" : "off";
        emitAckIfNeeded(1, firstOn);
        telemetry_.publishAllRelayStates();
        telemetry_.publishStatus();
        telemetry_.publishEvent("RELAY_TOGGLE_ALL", "Todos los relays cambiados de estado", newStateStr, "REMOTE_COMMAND");
        if (onStateChangeCb_) onStateChangeCb_();
    } else if (cmd == "all:on" || cmd == "all:off") {
        const bool targetOn = (cmd == "all:on");
        for (const auto& r : relays_.getAllStates()) {
            relays_.setDesiredState(r.deviceId, targetOn);
            relays_.applyDesiredState(r.deviceId, now, "REMOTE_COMMAND");
        }
        const auto states = relays_.getAllStates();
        const bool verifiedOn = (!states.empty() && states.front().currentState);
        emitAckIfNeeded(1, verifiedOn);
        telemetry_.publishAllRelayStates();
        telemetry_.publishStatus();
        telemetry_.publishEvent("DEVICE_COMMAND", targetOn ? "Comando all:on" : "Comando all:off", verifiedOn ? "on" : "off", "REMOTE_COMMAND");
        if (onStateChangeCb_) onStateChangeCb_();
    } else if (cmd == "sync_states") {
        telemetry_.publishStatus();
        const auto states = relays_.getAllStates();
        emitAckIfNeeded(1, !states.empty() && states.front().currentState);
    } else if (cmd.rfind("switch", 0) == 0 && cmd.find(':') != std::string::npos) {
        const size_t colonPos = cmd.find(':');
        const std::string chStr = cmd.substr(6, colonPos - 6);
        const size_t chOneBased = chStr.empty() ? 1U : static_cast<size_t>(std::strtoul(chStr.c_str(), nullptr, 10));
        const size_t targetIdx = (chOneBased > 0) ? (chOneBased - 1U) : 0U;
        const std::string subcmd = cmd.substr(colonPos + 1);
        const auto allRelays = relays_.getAllStates();
        if (!allRelays.empty() && targetIdx < allRelays.size()) {
            const std::string targetId = allRelays[targetIdx].deviceId;
            bool recognized = false;
            if (subcmd == "on" || subcmd == "1" || subcmd == "true") {
                relays_.setDesiredState(targetId, true);
                relays_.applyDesiredState(targetId, now, "REMOTE_COMMAND");
                recognized = true;
            } else if (subcmd == "off" || subcmd == "0" || subcmd == "false") {
                relays_.setDesiredState(targetId, false);
                relays_.applyDesiredState(targetId, now, "REMOTE_COMMAND");
                recognized = true;
            } else if (subcmd == "toggle") {
                relays_.toggle(targetId, now, "REMOTE_COMMAND");
                recognized = true;
            }
            if (recognized) {
                relays_.verifyHardwareState(targetId);
                const auto* updated = relays_.getState(targetId);
                const bool verifiedOn = (updated && updated->currentState);
                const std::string stateStr = verifiedOn ? "on" : "off";
                emitAckIfNeeded(static_cast<int>(targetIdx + 1U), verifiedOn);
                telemetry_.publishRelayState(targetId);
                telemetry_.publishStatus();
                telemetry_.publishEvent(
                    "DEVICE_COMMAND",
                    "Comando switch" + std::to_string(targetIdx + 1U) + ": " + subcmd,
                    stateStr,
                    "REMOTE_COMMAND");
                if (onStateChangeCb_) onStateChangeCb_();
            }
        }
    } else if (cmd.rfind("startup:", 0) == 0) {
        std::string modeStr = cmd.substr(8);
        domain::StartupMode mode = domain::startupModeFromString(modeStr);
        relays_.setStartupMode(mode);
        const auto states = relays_.getAllStates();
        emitAckIfNeeded(1, !states.empty() && states.front().currentState);
        telemetry_.publishStatus();
        telemetry_.publishEvent("STARTUP_MODE_CHANGED", "StartupMode configurado a " + std::string(domain::startupModeToString(mode)), "", "REMOTE_COMMAND");
        if (onStateChangeCb_) onStateChangeCb_();
    } else if (cmd.rfind("pulse:", 0) == 0) {
        std::string sub = cmd.substr(6);
        if (sub == "off" || sub == "0") {
            relays_.configurePulse(false, relays_.getPulseWidthMs(), now);
            telemetry_.publishEvent("PULSE_CONFIG_CHANGED", "Modo pulso desactivado", "", "REMOTE_COMMAND");
        } else if (sub.rfind("on,", 0) == 0) {
            uint32_t widthMs = static_cast<uint32_t>(std::strtoul(sub.substr(3).c_str(), nullptr, 10));
            if (relays_.configurePulse(true, widthMs, now)) {
                telemetry_.publishEvent("PULSE_CONFIG_CHANGED", "Modo pulso activado (" + std::to_string(widthMs) + " ms)", "", "REMOTE_COMMAND");
            }
        }
        const auto states = relays_.getAllStates();
        emitAckIfNeeded(1, !states.empty() && states.front().currentState);
        telemetry_.publishStatus();
        if (onStateChangeCb_) onStateChangeCb_();
    } else if (cmd.rfind("sled:", 0) == 0) {
        std::string sub = cmd.substr(5);
        bool en = (sub == "on" || sub == "1" || sub == "true");
        indicator_.setSledOnline(en);
        if (configRepo_) {
            configRepo_->saveSledOnline(en);
        }
        const auto states = relays_.getAllStates();
        emitAckIfNeeded(1, !states.empty() && states.front().currentState);
        telemetry_.publishStatus();
        telemetry_.publishEvent("SLED_CONFIG_CHANGED", std::string("Indicador LED online: ") + (en ? "on" : "off"), "", "REMOTE_COMMAND");
        if (onStateChangeCb_) onStateChangeCb_();
    } else if (!commandId.empty()) {
        telemetry_.publishCommandAck(commandId, sequence, "FAILED", "REMOTE_COMMAND", 0U, parsedChannel, false, "Unsupported command");
    }
}

void RelayFirmwareController::handleTimersPayload(const std::string& payload) {
    if (!timerScheduler_) {
        return;
    }

    DynamicJsonDocument doc(2560);
    if (deserializeJson(doc, payload) != DeserializationError::Ok) {
        log_.warn("[CTRL] Payload de temporizadores inválido (JSON malformado)");
        telemetry_.publishEvent("TIMERS_UPDATE_FAILED", "Payload JSON de temporizadores inválido");
        return;
    }

    JsonArrayConst timersArr;
    if (doc.is<JsonArray>()) {
        timersArr = doc.as<JsonArrayConst>();
    } else if (doc["timers"].is<JsonArray>()) {
        timersArr = doc["timers"].as<JsonArrayConst>();
    } else if (doc["payload"]["timers"].is<JsonArray>()) {
        timersArr = doc["payload"]["timers"].as<JsonArrayConst>();
    } else if (doc["payload"].is<const char*>()) {
        DynamicJsonDocument innerDoc(2560);
        if (deserializeJson(innerDoc, doc["payload"].as<const char*>()) == DeserializationError::Ok) {
            if (innerDoc.is<JsonArray>()) {
                TimerJsonCodec::applyTimersFromJsonArray(innerDoc.as<JsonArrayConst>(), *timerScheduler_, timeProvider_.epochSeconds());
                telemetry_.publishTimersEvent(
                    "TIMERS_UPDATED",
                    "Temporizadores sincronizados por MQTT5: " + std::to_string(timerScheduler_->getActiveTimerCount()) + " activos",
                    *timerScheduler_);
                if (onStateChangeCb_) onStateChangeCb_();
                return;
            } else if (innerDoc["timers"].is<JsonArray>()) {
                TimerJsonCodec::applyTimersFromJsonArray(innerDoc["timers"].as<JsonArrayConst>(), *timerScheduler_, timeProvider_.epochSeconds());
                telemetry_.publishTimersEvent(
                    "TIMERS_UPDATED",
                    "Temporizadores sincronizados por MQTT5: " + std::to_string(timerScheduler_->getActiveTimerCount()) + " activos",
                    *timerScheduler_);
                if (onStateChangeCb_) onStateChangeCb_();
                return;
            }
        }
        telemetry_.publishEvent("TIMERS_UPDATE_FAILED", "No se encontró el arreglo timers");
        return;
    } else {
        telemetry_.publishEvent("TIMERS_UPDATE_FAILED", "No se encontró el arreglo timers");
        return;
    }

    // GARANTÍA DE AISLAMIENTO: actualizar o eliminar schedules/timers modifica ÚNICAMENTE
    // la tabla de automatizaciones y jamás acciona ni restaura el estado del relé.
    TimerJsonCodec::applyTimersFromJsonArray(timersArr, *timerScheduler_, timeProvider_.epochSeconds());
    telemetry_.publishTimersEvent(
        "TIMERS_UPDATED",
        "Temporizadores sincronizados por MQTT5: " + std::to_string(timerScheduler_->getActiveTimerCount()) + " activos",
        *timerScheduler_);
    if (onStateChangeCb_) onStateChangeCb_();
}

void RelayFirmwareController::handleWifiPayload(const std::string& payload) {
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) {
        log_.warn("[CTRL] Solicitud MQTT5 de cambio Wi-Fi inválida");
        telemetry_.publishEvent("WIFI_UPDATE_FAILED", "Solicitud JSON de cambio Wi-Fi inválida");
        return;
    }

    JsonObjectConst root = doc.as<JsonObjectConst>();
    if (doc["payload"].is<JsonObject>()) {
        root = doc["payload"].as<JsonObjectConst>();
    }

    const char* rawSsid = root["ssid"] | (root["wifi_ssid"] | "");
    const char* rawPass = root["password"] | (root["wifi_pass"] | "");
    const char* rawReqId = root["request_id"] | (root["requestId"] | "");

    if (!rawSsid || std::strlen(rawSsid) == 0 || std::strlen(rawSsid) > 32 ||
        (rawPass && std::strlen(rawPass) > 64)) {
        log_.warn("[CTRL] Parámetros SSID/longitud inválidos en solicitud Wi-Fi MQTT5");
        telemetry_.publishEvent("WIFI_UPDATE_FAILED", "SSID inválido o fuera de rango (1..32 bytes)");
        return;
    }

    log_.info(std::string("[CTRL] Solicitud MQTT5 de adopción Wi-Fi aceptada hacia SSID: ") + rawSsid);
    if (onWifiChangeRequestCb_) {
        onWifiChangeRequestCb_(
            std::string(rawSsid),
            rawPass ? std::string(rawPass) : std::string(),
            rawReqId ? std::string(rawReqId) : std::string());
    }
}

}
