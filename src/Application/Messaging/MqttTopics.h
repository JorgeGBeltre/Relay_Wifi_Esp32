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
    std::string switchSet;
    std::string switchCommand;
    std::string switchState;
    std::string switchGet;

    static MqttTopics forDevice(const std::string& chipId, const std::string& baseSwitch = "device_switch1") {
        MqttTopics t;
        t.telemetry     = "telemetry";
        t.events        = "events";
        t.commands      = "commands";
        t.ota           = "ota";
        t.status        = "status";
        t.deviceEvents  = "event/" + chipId;
        t.switchSet     = baseSwitch + "/state";
        t.switchCommand = baseSwitch + "/command";
        t.switchState   = baseSwitch + "_state";
        t.switchGet     = baseSwitch + "/get";
        return t;
    }
};

}
