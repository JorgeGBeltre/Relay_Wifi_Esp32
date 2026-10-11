#pragma once

#include <vector>
#include <string>
#include <functional>
#include "Application/Messaging/MqttTopics.h"
#include "Application/Services/ConnectivityService.h"
#include "Application/Services/RelayService.h"
#include "Application/Services/TelemetryService.h"
#include "Domain/Interfaces/IButtonHandler.h"
#include "Domain/Interfaces/IClock.h"
#include "Domain/Interfaces/IIndicator.h"
#include "Domain/Interfaces/ILogger.h"
#include "Domain/Interfaces/IOtaService.h"
#include "Domain/Interfaces/ISystemControl.h"
#include "Domain/Interfaces/ITimeProvider.h"

namespace relay::app {

class RelayFirmwareController {
public:
    enum class State {
        Connecting,
        Operational
    };

    using ApTriggerCallback = std::function<void()>;

    RelayFirmwareController(RelayService& relays,
                            ConnectivityService& connectivity,
                            TelemetryService& telemetry,
                            domain::IOtaService& ota,
                            domain::IIndicator& indicator,
                            domain::ISystemControl& system,
                            domain::ITimeProvider& timeProvider,
                            domain::IClock& clock,
                            domain::ILogger& logger,
                            const MqttTopics& topics,
                            ApTriggerCallback apCallback = nullptr);

    void registerButton(domain::IButtonHandler& button, const std::string& targetDeviceId);

    void setup();
    void tick();

    void onMqttConnected();
    void handleIncomingMessage(const std::string& topic, const std::string& payload);

    State getState() const { return state_; }

private:
    struct ButtonBinding {
        domain::IButtonHandler* handler;
        std::string targetDeviceId;
    };

    void handleButtonEvents();
    void processCommand(const std::string& command);

    RelayService&                  relays_;
    ConnectivityService&           connectivity_;
    TelemetryService&              telemetry_;
    domain::IOtaService&           ota_;
    domain::IIndicator&            indicator_;
    domain::ISystemControl&        system_;
    domain::ITimeProvider&         timeProvider_;
    domain::IClock&                clock_;
    domain::ILogger&               log_;
    const MqttTopics               topics_;
    ApTriggerCallback              apCallback_;

    std::vector<ButtonBinding>     buttons_;
    State                          state_ = State::Connecting;

    unsigned long                  lastStatusPublishMs_ = 0;
    unsigned long                  lastTimeSyncMs_ = 0;
    static constexpr unsigned long STATUS_INTERVAL_MS = 3600000;
    static constexpr unsigned long TIME_SYNC_INTERVAL_MS = 3600000;
};

}
