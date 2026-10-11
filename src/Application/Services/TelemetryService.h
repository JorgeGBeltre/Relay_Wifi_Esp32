#pragma once

#include <string>
#include "Application/Messaging/MqttTopics.h"
#include "Application/Serialization/IPayloadSerializer.h"
#include "Application/Services/ConnectivityService.h"
#include "Application/Services/RelayService.h"
#include "Domain/Entities/DeviceEvent.h"
#include "Domain/Interfaces/IClock.h"
#include "Domain/Interfaces/IDeviceRepository.h"
#include "Domain/Interfaces/ILogger.h"
#include "Domain/Interfaces/ITimeProvider.h"

namespace relay::app {

class TelemetryService {
public:
    TelemetryService(RelayService& relays,
                     ConnectivityService& connectivity,
                     domain::IDeviceRepository& deviceRepo,
                     domain::ITimeProvider& timeProvider,
                     domain::IClock& clock,
                     IPayloadSerializer& serializer,
                     const MqttTopics& topics,
                     domain::ILogger& logger)
        : relays_(relays), connectivity_(connectivity), deviceRepo_(deviceRepo),
          timeProvider_(timeProvider), clock_(clock), serializer_(serializer),
          topics_(topics), log_(logger) {}

    void publishStatus();
    void publishEvent(const std::string& name, const std::string& description, const std::string& state = "");
    void publishRelayState(const std::string& deviceId);
    void publishAllRelayStates();
    void publishTelemetry();
    void requestCurrentStates();

private:
    RelayService&               relays_;
    ConnectivityService&        connectivity_;
    domain::IDeviceRepository&  deviceRepo_;
    domain::ITimeProvider&      timeProvider_;
    domain::IClock&             clock_;
    IPayloadSerializer&         serializer_;
    const MqttTopics            topics_;
    domain::ILogger&            log_;
};

}
