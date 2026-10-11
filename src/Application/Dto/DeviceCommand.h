#pragma once

#include <string>

namespace relay::app {

enum class CommandType {
    None,
    TurnOn,
    TurnOff,
    Toggle,
    ToggleAll,
    Status,
    Reset,
    SyncStates
};

struct DeviceCommand {
    CommandType type = CommandType::None;
    std::string targetDeviceId;
    std::string rawPayload;
};

}
