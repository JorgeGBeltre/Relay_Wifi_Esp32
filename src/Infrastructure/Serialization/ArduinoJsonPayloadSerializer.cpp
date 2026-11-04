#include "Infrastructure/Serialization/ArduinoJsonPayloadSerializer.h"
#include "Infrastructure/Config/nvs_config.h"
#include "Domain/Entities/DeviceConfig.h"
#include <ArduinoJson.h>

namespace relay::infra {

std::string ArduinoJsonPayloadSerializer::formatStatus(
    const domain::DeviceIdentity& id,
    const domain::SystemMetrics& metrics,
    const std::vector<domain::RelayState>& relays,
    const std::string& timestamp,
    bool syncCompleted,
    uint32_t stateVersion,
    const std::string& lastSource) {

    DynamicJsonDocument doc(2560);

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
    details["state_version"] = stateVersion;
    if (!lastSource.empty()) {
        details["source"] = lastSource;
    }
    details["devices_count"] = relays.size();
    details["channel_count"] = relays.size();
    details["wifi_status"] = metrics.wifiConnected ? "connected" : "disconnected";
    details["rssi"] = metrics.rssi;
    details["free_heap"] = metrics.freeHeap;
    details["uptime"] = metrics.uptime;
    if (!metrics.wifiSsid.empty()) {
        details["wifi_ssid"] = metrics.wifiSsid;
    } else if (config.wifi_ssid.length() > 0) {
        details["wifi_ssid"] = config.wifi_ssid.c_str();
    }
    if (!metrics.wifiBssid.empty()) {
        details["wifi_bssid"] = metrics.wifiBssid;
    }
    if (!metrics.localIp.empty()) {
        details["local_ip"] = metrics.localIp;
    }
    details["startup"] = domain::startupModeToString(static_cast<domain::StartupMode>(config.startup_mode));
    details["pulse"] = config.pulse_enabled ? "on" : "off";
    details["pulseWidth"] = config.pulse_width_ms;
    details["sledOnline"] = config.sled_online ? "on" : "off";

    JsonObject devicesObj = details.createNestedObject("devices");
    JsonArray switchesArr = details.createNestedArray("switches");
    for (size_t i = 0; i < relays.size(); ++i) {
        const auto& r = relays[i];
        JsonObject rObj = devicesObj.createNestedObject(r.deviceId);
        rObj["state"] = r.currentState ? "on" : "off";
        rObj["desired_state"] = r.desiredState ? "on" : "off";
        rObj["pin"] = r.pin;

        JsonObject swObj = switchesArr.createNestedObject();
        swObj["outlet"] = static_cast<int>(i);
        swObj["switch"] = r.currentState ? "on" : "off";
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
    const std::string& timestamp,
    uint32_t stateVersion) {

    DynamicJsonDocument doc(1792);

    JsonObject dev = doc.createNestedObject("Device");
    dev["status"] = metrics.mqttConnected ? "connected" : "disconnected";
    dev["ChipId"] = id.chipId;
    dev["MacAddress"] = id.macAddress;
    dev["ChipType"] = id.chipType;
    dev["FirmwareVersion"] = id.firmwareVersion;

    doc["Timestamp"] = timestamp;

    JsonObject details = doc.createNestedObject("Details");
    details["state_version"] = stateVersion;
    details["rssi"] = metrics.rssi;
    details["free_heap"] = metrics.freeHeap;
    details["uptime"] = metrics.uptime;
    if (!metrics.wifiSsid.empty()) {
        details["wifi_ssid"] = metrics.wifiSsid;
    }
    if (!metrics.localIp.empty()) {
        details["local_ip"] = metrics.localIp;
    }

    details["devices_count"] = relays.size();
    details["channel_count"] = relays.size();
    JsonObject relaysObj = details.createNestedObject("relays");
    for (const auto& r : relays) {
        relaysObj[r.deviceId] = r.currentState ? "on" : "off";
    }

    String output;
    serializeJson(doc, output);
    return output.c_str();
}

}
