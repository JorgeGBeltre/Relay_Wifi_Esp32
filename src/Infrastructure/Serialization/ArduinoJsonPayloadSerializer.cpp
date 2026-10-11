#include "Infrastructure/Serialization/ArduinoJsonPayloadSerializer.h"
#include <ArduinoJson.h>

namespace relay::infra {

std::string ArduinoJsonPayloadSerializer::formatStatus(
    const domain::DeviceIdentity& id,
    const domain::SystemMetrics& metrics,
    const std::vector<domain::RelayState>& relays,
    const std::string& timestamp,
    bool syncCompleted) {

    DynamicJsonDocument doc(2048);

    JsonObject dev = doc.createNestedObject("Device");
    dev["status"] = metrics.mqttConnected ? "connected" : "disconnected";
    dev["ChipId"] = id.chipId;
    dev["MacAddress"] = id.macAddress;
    dev["ChipType"] = id.chipType;
    dev["FirmwareVersion"] = id.firmwareVersion;

    doc["Timestamp"] = timestamp;

    JsonObject details = doc.createNestedObject("Details");
    details["chipId"] = id.chipId;
    details["sync_completed"] = syncCompleted;
    details["devices_count"] = relays.size();
    details["wifi_status"] = metrics.wifiConnected ? "connected" : "disconnected";
    details["rssi"] = metrics.rssi;
    details["free_heap"] = metrics.freeHeap;
    details["uptime"] = metrics.uptime;

    JsonObject devicesObj = details.createNestedObject("devices");
    for (const auto& r : relays) {
        JsonObject rObj = devicesObj.createNestedObject(r.deviceId);
        rObj["state"] = r.currentState ? "on" : "off";
        rObj["desired_state"] = r.desiredState ? "on" : "off";
        rObj["pin"] = r.pin;
    }

    String output;
    serializeJson(doc, output);
    return output.c_str();
}

std::string ArduinoJsonPayloadSerializer::formatEvent(
    const domain::DeviceIdentity& id,
    const domain::DeviceEvent& event,
    const std::string& timestamp,
    bool isConnected) {

    DynamicJsonDocument doc(1536);

    JsonObject dev = doc.createNestedObject("Device");
    dev["status"] = isConnected ? "connected" : "disconnected";
    dev["ChipId"] = id.chipId;
    dev["MacAddress"] = id.macAddress;
    dev["ChipType"] = id.chipType;
    dev["FirmwareVersion"] = id.firmwareVersion;

    doc["Timestamp"] = timestamp;

    JsonObject details = doc.createNestedObject("Details");
    details["chipId"] = id.chipId;

    JsonObject info = details.createNestedObject("info");
    info["event"] = event.eventName;
    info["description"] = event.description;
    if (!event.state.empty()) {
        info["state"] = event.state;
    }
    info["send_interval"] = 30;
    info["timestamp"] = millis();

    String output;
    serializeJson(doc, output);
    return output.c_str();
}

std::string ArduinoJsonPayloadSerializer::formatTelemetry(
    const domain::DeviceIdentity& id,
    const domain::SystemMetrics& metrics,
    const std::vector<domain::RelayState>& relays,
    const std::string& timestamp) {

    DynamicJsonDocument doc(1536);

    JsonObject dev = doc.createNestedObject("Device");
    dev["status"] = metrics.mqttConnected ? "connected" : "disconnected";
    dev["ChipId"] = id.chipId;
    dev["MacAddress"] = id.macAddress;
    dev["ChipType"] = id.chipType;
    dev["FirmwareVersion"] = id.firmwareVersion;

    doc["Timestamp"] = timestamp;

    JsonObject details = doc.createNestedObject("Details");
    details["rssi"] = metrics.rssi;
    details["free_heap"] = metrics.freeHeap;
    details["uptime"] = metrics.uptime;

    JsonObject relaysObj = details.createNestedObject("relays");
    for (const auto& r : relays) {
        relaysObj[r.deviceId] = r.currentState ? "on" : "off";
    }

    String output;
    serializeJson(doc, output);
    return output.c_str();
}

}
