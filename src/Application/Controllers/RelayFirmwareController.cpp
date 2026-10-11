#include "Application/Controllers/RelayFirmwareController.h"

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
    ApTriggerCallback apCallback)
    : relays_(relays), connectivity_(connectivity), telemetry_(telemetry),
      ota_(ota), indicator_(indicator), system_(system), timeProvider_(timeProvider),
      clock_(clock), log_(logger), topics_(topics), apCallback_(std::move(apCallback)) {}

void RelayFirmwareController::registerButton(domain::IButtonHandler& button,
                                             const std::string& targetDeviceId) {
    button.init();
    buttons_.push_back(ButtonBinding{&button, targetDeviceId});
}

void RelayFirmwareController::setup() {
    log_.info("[CTRL] Inicializando sistema de relés...");
    relays_.init();

    indicator_.show(domain::IndicatorStatus::Connecting);

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
        indicator_.show(domain::IndicatorStatus::Connecting);
    }
}

void RelayFirmwareController::onMqttConnected() {
    log_.info("[CTRL] MQTT conectado: suscribiendo a tópicos...");

    connectivity_.transport().subscribe(topics_.commands);
    connectivity_.transport().subscribe(topics_.deviceEvents);
    connectivity_.transport().subscribe(topics_.ota);

    for (const auto& r : relays_.getAllStates()) {
        connectivity_.transport().subscribe(r.topic + "/state");
        connectivity_.transport().subscribe(r.topic + "/command");
        connectivity_.transport().subscribe(r.topic + "_state");
    }

    telemetry_.publishEvent("MQTT_CONNECTED", "Conexión MQTTS segura establecida");
    telemetry_.requestCurrentStates();
    telemetry_.publishStatus();
}

void RelayFirmwareController::tick() {
    indicator_.update();

    if (ota_.isInProgress()) {
        ota_.handle();
        clock_.sleepMs(10);
        return;
    }

    ota_.handle();
    handleButtonEvents();

    if (state_ == State::Connecting) {
        if (connectivity_.ensureConnected()) {
            state_ = State::Operational;
            indicator_.show(domain::IndicatorStatus::Operational);
            log_.info("[CTRL] Estado -> OPERATIONAL");
        }
    } else if (state_ == State::Operational) {
        if (!connectivity_.isOnline()) {
            indicator_.show(domain::IndicatorStatus::Connecting);
            if (!connectivity_.ensureConnected()) {
                state_ = State::Connecting;
                return;
            }
        }

        connectivity_.transport().loop();

        unsigned long now = clock_.nowMs();
        if (now - lastStatusPublishMs_ >= STATUS_INTERVAL_MS) {
            lastStatusPublishMs_ = now;
            telemetry_.publishStatus();
        }

        if (now - lastTimeSyncMs_ >= TIME_SYNC_INTERVAL_MS) {
            lastTimeSyncMs_ = now;
            timeProvider_.sync();
        }
    }
}

void RelayFirmwareController::handleButtonEvents() {
    for (auto& btn : buttons_) {
        if (!btn.handler) continue;
        auto ev = btn.handler->update();

        if (ev == domain::ButtonEvent::ShortPress) {
            log_.info("[BUTTON] Pulsación corta detectada para " + btn.targetDeviceId);
            relays_.toggle(btn.targetDeviceId);

            const auto* state = relays_.getState(btn.targetDeviceId);
            std::string stateStr = (state && state->currentState) ? "on" : "off";
            std::string desc = "Cambio por botón físico - " + btn.targetDeviceId + ": " + (state && state->currentState ? "ENCENDIDO" : "APAGADO");

            telemetry_.publishEvent("BUTTON_PRESS", desc, stateStr);
            telemetry_.publishRelayState(btn.targetDeviceId);
        } else if (ev == domain::ButtonEvent::LongPress) {
            log_.info("[BUTTON] Pulsación larga (>3s) detectada -> Activando modo AP");
            telemetry_.publishEvent("AP_MODE_ACTIVATED", "Modo Access Point activado por botón físico");
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
    log_.info("[CTRL] Mensaje recibido en [" + topic + "]: " + payload);

    if (topic == topics_.commands) {
        processCommand(payload);
        return;
    }

    for (const auto& r : relays_.getAllStates()) {
        std::string setTopic = r.topic + "/state";
        std::string cmdTopic = r.topic + "/command";
        std::string stateTopic = r.topic + "_state";

        if (topic == setTopic || topic == cmdTopic) {
            bool turnOn = (payload == "on" || payload == "1" || payload == "true");
            bool turnOff = (payload == "off" || payload == "0" || payload == "false");
            bool doToggle = (payload == "toggle");

            if (turnOn) {
                relays_.setDesiredState(r.deviceId, true);
                relays_.applyDesiredState(r.deviceId);
            } else if (turnOff) {
                relays_.setDesiredState(r.deviceId, false);
                relays_.applyDesiredState(r.deviceId);
            } else if (doToggle) {
                relays_.toggle(r.deviceId);
            }

            std::string stateStr = relays_.getState(r.deviceId)->currentState ? "on" : "off";
            telemetry_.publishEvent("DEVICE_SET_COMMAND", "Comando recibido: " + payload, stateStr);
            telemetry_.publishRelayState(r.deviceId);
            return;
        } else if (topic == stateTopic) {
            bool serverState = (payload == "on" || payload == "1" || payload == "true");
            relays_.setDesiredState(r.deviceId, serverState);
            relays_.applyDesiredState(r.deviceId);

            if (!relays_.isInitialSyncCompleted()) {
                relays_.setInitialSyncCompleted(true);
                telemetry_.publishEvent("INITIAL_STATE", "Estado inicial desde servidor", serverState ? "on" : "off");
            } else {
                telemetry_.publishEvent("STATE_UPDATE", "Actualización de estado desde servidor", serverState ? "on" : "off");
            }
            return;
        }
    }
}

void RelayFirmwareController::processCommand(const std::string& cmd) {
    if (cmd == "reset") {
        telemetry_.publishEvent("SYSTEM_RESET", "Reinicio del sistema solicitado");
        clock_.sleepMs(1000);
        system_.reboot();
    } else if (cmd == "status") {
        telemetry_.publishStatus();
    } else if (cmd == "toggle_all") {
        telemetry_.publishEvent("RELAY_TOGGLE_ALL", "Todos los relays cambiados de estado");
        relays_.toggleAll();
        telemetry_.publishStatus();
        telemetry_.publishAllRelayStates();
    } else if (cmd == "sync_states") {
        telemetry_.publishEvent("STATE_SYNC", "Sincronización de estados solicitada");
        relays_.setInitialSyncCompleted(false);
        telemetry_.requestCurrentStates();
    } else if (cmd.rfind("switch1:", 0) == 0) {
        std::string subcmd = cmd.substr(8);
        telemetry_.publishEvent("DEVICE_COMMAND", "Comando switch1: " + subcmd);
        if (subcmd == "on" || subcmd == "1" || subcmd == "true") {
            for (const auto& r : relays_.getAllStates()) {
                relays_.setDesiredState(r.deviceId, true);
                relays_.applyDesiredState(r.deviceId);
                telemetry_.publishRelayState(r.deviceId);
                break;
            }
        } else if (subcmd == "off" || subcmd == "0" || subcmd == "false") {
            for (const auto& r : relays_.getAllStates()) {
                relays_.setDesiredState(r.deviceId, false);
                relays_.applyDesiredState(r.deviceId);
                telemetry_.publishRelayState(r.deviceId);
                break;
            }
        } else if (subcmd == "toggle") {
            for (const auto& r : relays_.getAllStates()) {
                relays_.toggle(r.deviceId);
                telemetry_.publishRelayState(r.deviceId);
                break;
            }
        }
    }
}

}
