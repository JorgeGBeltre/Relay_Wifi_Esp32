#pragma once

#include <string>

namespace relay::app {

struct MqttTopics {
    std::string telemetry;
    std::string events;
    std::string commands;
    std::string ota;
    std::string status;
    std::string deviceEvents;
    std::string devicePrefix;
    std::string commandsAck;
    std::string timersCommand;
    std::string wifiCommand;
    std::string unlinkCommand;
    std::string switchSet;
    std::string switchCommand;
    std::string switchState;
    std::string switchGet;

    static MqttTopics forDevice(const std::string& chipId, const std::string& baseSwitch = "device_switch1") {
        MqttTopics t;
        t.telemetry     = "telemetry/" + chipId;
        t.events        = "events/" + chipId;
        t.commands      = "commands/" + chipId;
        t.ota           = "ota/" + chipId;
        t.status        = "status/" + chipId;
        t.deviceEvents  = "event/" + chipId;
        t.devicePrefix  = "devices/" + chipId;
        t.commandsAck   = t.devicePrefix + "/commands/ack";
        t.timersCommand = t.devicePrefix + "/timers/command";
        t.wifiCommand   = t.devicePrefix + "/wifi/command";
        t.unlinkCommand = t.devicePrefix + "/unlink/command";
        t.switchSet     = t.devicePrefix + "/" + baseSwitch + "/state";
        t.switchCommand = t.devicePrefix + "/" + baseSwitch + "/command";
        t.switchState   = t.devicePrefix + "/" + baseSwitch + "_state";
        t.switchGet     = t.devicePrefix + "/" + baseSwitch + "/get";
        return t;
    }

    std::string relay(const std::string& base, const std::string& suffix) const {
        if (devicePrefix.empty()) {
            return base + suffix;
        }
        return devicePrefix + "/" + base + suffix;
    }
};

}
