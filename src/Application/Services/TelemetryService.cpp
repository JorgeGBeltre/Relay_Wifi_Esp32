#include "Application/Services/TelemetryService.h"
#include "Application/Services/TimerJsonCodec.h"
#include "Application/Services/TimerSchedulerService.h"
#include <ArduinoJson.h>

namespace relay::app {

void TelemetryService::publishStatus() {
    if (!connectivity_.isOnline()) return;

    auto id = deviceRepo_.identity();
    auto metrics = deviceRepo_.metrics();
    metrics.wifiConnected = connectivity_.network().isConnected();
    metrics.mqttConnected = connectivity_.transport().isConnected();
    metrics.rssi = connectivity_.activeRssi();

    std::string timestamp = timeProvider_.isoTimestamp();
    std::string payload = serializer_.formatStatus(
        id,
        metrics,
        relays_.getAllStates(),
        timestamp,
        relays_.isInitialSyncCompleted(),
        relays_.getStateVersion(),
        relays_.getLastSource());

    // ÚNICO tópico con retain=true: disponibilidad + estado reportado verificado (sobrescribe LWT offline).
    connectivity_.transport().publish(topics_.status, payload, true);
    log_.info("[TELEM] Estado publicado en " + topics_.status);
}

void TelemetryService::publishEvent(const std::string& name,
                                    const std::string& description,
                                    const std::string& state,
                                    const std::string& source) {
    if (!connectivity_.transport().isConnected()) return;

    auto id = deviceRepo_.identity();
    std::string timestamp = timeProvider_.isoTimestamp();

    DynamicJsonDocument doc(1536);
    JsonObject dev = doc.createNestedObject("Device");
    dev["status"] = "connected";
    dev["ChipId"] = id.chipId;
    dev["MacAddress"] = id.macAddress;
    dev["ChipType"] = id.chipType;
    dev["FirmwareVersion"] = id.firmwareVersion;
    doc["Timestamp"] = timestamp;

    JsonObject details = doc.createNestedObject("Details");
    details["chipId"] = id.chipId;
    details["state_version"] = relays_.getStateVersion();
    const std::string effectiveSource = !source.empty() ? source : relays_.getLastSource();
    if (!effectiveSource.empty()) {
        details["source"] = effectiveSource;
    }

    const auto allStates = relays_.getAllStates();
    details["devices_count"] = allStates.size();
    details["channel_count"] = allStates.size();
    JsonArray switchesArr = details.createNestedArray("switches");
    for (size_t i = 0; i < allStates.size(); ++i) {
        JsonObject swObj = switchesArr.createNestedObject();
        swObj["outlet"] = static_cast<int>(i);
        swObj["switch"] = allStates[i].currentState ? "on" : "off";
    }

    JsonObject info = details.createNestedObject("info");
    info["event"] = name;
    info["description"] = description;
    if (!state.empty()) {
        info["state"] = state;
    }
    if (!effectiveSource.empty()) {
        info["source"] = effectiveSource;
    }
    info["state_version"] = relays_.getStateVersion();
    info["timestamp"] = clock_.nowMs();

    String output;
    serializeJson(doc, output);
    connectivity_.transport().publish(topics_.events, output.c_str(), false);
    log_.info("[EVENT] " + name + ": " + description);
}

void TelemetryService::publishTimersEvent(const std::string& name,
                                          const std::string& description,
                                          const TimerSchedulerService& scheduler) {
    if (!connectivity_.transport().isConnected()) return;

    auto id = deviceRepo_.identity();
    std::string timestamp = timeProvider_.isoTimestamp();

    DynamicJsonDocument doc(2048);
    JsonObject dev = doc.createNestedObject("Device");
    dev["status"] = "connected";
    dev["ChipId"] = id.chipId;
    dev["MacAddress"] = id.macAddress;
    dev["ChipType"] = id.chipType;
    dev["FirmwareVersion"] = id.firmwareVersion;
    doc["Timestamp"] = timestamp;

    JsonObject details = doc.createNestedObject("Details");
    details["chipId"] = id.chipId;
    details["state_version"] = relays_.getStateVersion();
    JsonArray timersArr = details.createNestedArray("timers");
    TimerJsonCodec::serializeTimersToJsonArray(scheduler, timersArr);

    JsonObject info = details.createNestedObject("info");
    info["event"] = name;
    info["description"] = description;
    info["state_version"] = relays_.getStateVersion();
    info["timestamp"] = clock_.nowMs();

    String output;
    serializeJson(doc, output);
    connectivity_.transport().publish(topics_.events, output.c_str(), false);
    log_.info("[EVENT] " + name + ": " + description);
}

void TelemetryService::publishCommandAck(const std::string& commandId,
                                         uint64_t sequence,
                                         const std::string& status,
                                         const std::string& source,
                                         uint32_t execUs,
                                         int channel,
                                         bool duplicate,
                                         const std::string& errorMsg) {
    if (!connectivity_.transport().isConnected() || topics_.commandsAck.empty()) return;

    auto id = deviceRepo_.identity();
    const auto allStates = relays_.getAllStates();
    const size_t idx = (channel > 1 && static_cast<size_t>(channel - 1) < allStates.size())
        ? static_cast<size_t>(channel - 1)
        : 0U;
    const bool reported = (!allStates.empty() && idx < allStates.size())
        ? allStates[idx].currentState
        : false;

    StaticJsonDocument<1024> doc;
    doc["commandId"] = commandId;
    doc["command_id"] = commandId;
    doc["deviceId"] = id.chipId;
    JsonObject dev = doc.createNestedObject("Device");
    dev["ChipId"] = id.chipId;
    dev["FirmwareVersion"] = id.firmwareVersion;
    doc["channel"] = channel > 0 ? channel : 1;
    doc["reportedState"] = reported;
    doc["state"] = reported ? "on" : "off";
    doc["sequence"] = sequence;
    doc["stateVersion"] = relays_.getStateVersion();
    doc["state_version"] = relays_.getStateVersion();
    doc["source"] = source;
    doc["execUs"] = execUs;
    doc["duplicate"] = duplicate;
    doc["status"] = status;
    doc["Timestamp"] = timeProvider_.isoTimestamp();
    if (!errorMsg.empty()) {
        doc["error"] = errorMsg;
    }

    doc["channel_count"] = allStates.size();
    JsonArray switchesArr = doc.createNestedArray("switches");
    for (size_t i = 0; i < allStates.size(); ++i) {
        JsonObject swObj = switchesArr.createNestedObject();
        swObj["outlet"] = static_cast<int>(i);
        swObj["switch"] = allStates[i].currentState ? "on" : "off";
    }

    String output;
    serializeJson(doc, output);
    connectivity_.transport().publish(topics_.commandsAck, output.c_str(), false);
    log_.info("[ACK] " + commandId + " -> " + status + " (ver=" + std::to_string(relays_.getStateVersion()) + ")");
}

void TelemetryService::publishRelayState(const std::string& deviceId) {
    if (!connectivity_.transport().isConnected()) return;

    const auto* relay = relays_.getState(deviceId);
    if (!relay) return;

    std::string stateStr = relay->currentState ? "on" : "off";
    std::string topic = topics_.relay(relay->topic, "/report");

    connectivity_.transport().publish(topic, stateStr, false);
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
    std::string payload = serializer_.formatTelemetry(
        id,
        metrics,
        relays_.getAllStates(),
        timestamp,
        relays_.getStateVersion());

    connectivity_.transport().publish(topics_.telemetry, payload, false);
    log_.info("[TELEM] Telemetría periódica publicada en " + topics_.telemetry);
}

void TelemetryService::requestCurrentStates() {
    // Eliminado intencionalmente: el dispositivo reporta su estado físico real (Reported State)
    // y jamás solicita ni acepta estados antiguos retenidos al reconectarse.
}

}
