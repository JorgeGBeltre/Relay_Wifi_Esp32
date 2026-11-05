#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include "Application/Messaging/MqttTopics.h"
#include "Application/Services/ConnectivityService.h"
#include "Application/Services/RelayService.h"
#include "Application/Services/TelemetryService.h"
#include "Application/Services/TimerSchedulerService.h"
#include "Domain/Interfaces/IButtonHandler.h"
#include "Domain/Interfaces/IClock.h"
#include "Domain/Interfaces/IConfigRepository.h"
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
    using StateChangedNotifyCallback = std::function<void()>;
    using WifiChangeRequestCallback = std::function<void(const std::string& ssid, const std::string& password, const std::string& requestId)>;
    using AccountUnlinkRequestCallback = std::function<void(const std::string& source, const std::string& unlinkProof, const std::string& requestId)>;

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
                            ApTriggerCallback apCallback = nullptr,
                            TimerSchedulerService* timerScheduler = nullptr,
                            domain::IConfigRepository* configRepo = nullptr);

    void registerButton(domain::IButtonHandler& button, const std::string& targetDeviceId);
    void setOnLocalStateChangeCallback(StateChangedNotifyCallback cb) { onStateChangeCb_ = std::move(cb); }
    void setOnWifiChangeRequestCallback(WifiChangeRequestCallback cb) { onWifiChangeRequestCb_ = std::move(cb); }
    void setOnAccountUnlinkRequestCallback(AccountUnlinkRequestCallback cb) { onAccountUnlinkCb_ = std::move(cb); }

    void setup(bool relaysAlreadyInitialized = false);
    void tick();

    void onMqttConnected();
    void handleIncomingMessage(const std::string& topic, const std::string& payload);

    State getState() const { return state_; }

private:
    struct ButtonBinding {
        domain::IButtonHandler* handler;
        std::string targetDeviceId;
    };

    struct ProcessedCommandEntry {
        char commandId[48]{};
        uint64_t sequence = 0;
        uint32_t stateVersion = 0;
        int channel = 1;
        bool reportedState = false;
    };

    static constexpr size_t CMD_DEDUP_CAPACITY = 16;

    bool findProcessedCommand(const std::string& commandId, ProcessedCommandEntry& outEntry) const;
    void recordProcessedCommand(const std::string& commandId,
                                uint64_t sequence,
                                int channel,
                                bool reportedState);

    void handleButtonEvents();
    void processCommand(const std::string& command);
    void handleTimersPayload(const std::string& payload);
    void handleWifiPayload(const std::string& payload);
    void handleUnlinkPayload(const std::string& payload);
    void updateIndicatorForConnectivity();

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
    TimerSchedulerService*         timerScheduler_ = nullptr;
    domain::IConfigRepository*     configRepo_ = nullptr;
    StateChangedNotifyCallback     onStateChangeCb_;
    WifiChangeRequestCallback      onWifiChangeRequestCb_;
    AccountUnlinkRequestCallback   onAccountUnlinkCb_;

    std::vector<ButtonBinding>     buttons_;
    State                          state_ = State::Connecting;

    unsigned long                  lastStatusPublishMs_ = 0;
    unsigned long                  lastTimeSyncMs_ = 0;
    unsigned long                  lastConnectAttemptMs_ = 0;
    uint32_t                       lastEvaluatedEpochSec_ = 0;
    uint64_t                       lastCommandSequence_ = 0;
    ProcessedCommandEntry          recentCommands_[CMD_DEDUP_CAPACITY]{};
    size_t                         recentCommandHead_ = 0;
    static constexpr unsigned long STATUS_INTERVAL_MS = 60000;
    static constexpr unsigned long TIME_SYNC_INTERVAL_MS = 3600000;
    static constexpr unsigned long RECONNECT_BACKOFF_MS = 5000;
};

}
