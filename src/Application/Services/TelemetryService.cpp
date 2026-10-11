#include "Application/Services/TelemetryService.h"

namespace relay::app {

void TelemetryService::publishStatus() {
    if (!connectivity_.isOnline()) return;

    auto id = deviceRepo_.identity();
    auto metrics = deviceRepo_.metrics();
    metrics.wifiConnected = connectivity_.network().isConnected();
    metrics.mqttConnected = connectivity_.transport().isConnected();
    metrics.rssi = connectivity_.activeRssi();

    std::string timestamp = timeProvider_.isoTimestamp();
    std::string payload = serializer_.formatStatus(id, metrics, relays_.getAllStates(),
                                                  timestamp, relays_.isInitialSyncCompleted());

    connectivity_.transport().publish(topics_.status, payload);
    log_.info("[TELEM] Estado publicado en " + topics_.status);
}

void TelemetryService::publishEvent(const std::string& name,
                                    const std::string& description,
                                    const std::string& state) {
    if (!connectivity_.transport().isConnected()) return;

    auto id = deviceRepo_.identity();
    domain::DeviceEvent ev{name, description, state};
    std::string timestamp = timeProvider_.isoTimestamp();

    std::string payload = serializer_.formatEvent(id, ev, timestamp, true);
    connectivity_.transport().publish(topics_.events, payload);
    log_.info("[EVENT] " + name + ": " + description);
}

void TelemetryService::publishRelayState(const std::string& deviceId) {
    if (!connectivity_.transport().isConnected()) return;

    const auto* relay = relays_.getState(deviceId);
    if (!relay) return;

    std::string stateStr = relay->currentState ? "on" : "off";
    std::string topic = relay->topic + "_state";

    connectivity_.transport().publish(topic, stateStr);
    log_.info("[TELEM] Estado de relé " + deviceId + " publicado en " + topic + " -> " + stateStr);
}

void TelemetryService::publishAllRelayStates() {
    for (const auto& r : relays_.getAllStates()) {
        publishRelayState(r.deviceId);
    }
}

void TelemetryService::publishTelemetry() {
    if (!connectivity_.isOnline()) return;

    auto id = deviceRepo_.identity();
    auto metrics = deviceRepo_.metrics();
    metrics.wifiConnected = connectivity_.network().isConnected();
    metrics.mqttConnected = connectivity_.transport().isConnected();
    metrics.rssi = connectivity_.activeRssi();

    std::string timestamp = timeProvider_.isoTimestamp();
    std::string payload = serializer_.formatTelemetry(id, metrics, relays_.getAllStates(), timestamp);

    connectivity_.transport().publish(topics_.telemetry, payload);
    log_.info("[TELEM] Telemetría periódica publicada en " + topics_.telemetry);
}

void TelemetryService::requestCurrentStates() {
    if (!connectivity_.transport().isConnected()) return;

    for (const auto& r : relays_.getAllStates()) {
        std::string reqTopic = r.topic + "/get";
        connectivity_.transport().publish(reqTopic, "status");
        log_.info("[TELEM] Solicitando estado del servidor en " + reqTopic);
    }
}

}
