#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Domain/Entities/RelayState.h"
#include "Domain/Interfaces/ILogger.h"
#include "Domain/Interfaces/IRelayController.h"

namespace relay::app {

class RelayService {
public:
    struct ManagedRelay {
        domain::RelayState state;
        domain::IRelayController* controller;
    };

    explicit RelayService(domain::ILogger& logger) : log_(logger) {}

    void registerRelay(const std::string& deviceId,
                       domain::IRelayController& controller,
                       const std::string& topic);

    void init();

    bool setDesiredState(const std::string& deviceId, bool desired);
    bool applyDesiredState(const std::string& deviceId);
    void applyAllDesiredStates();

    bool toggle(const std::string& deviceId);
    void toggleAll();

    bool isStateChanged(const std::string& deviceId) const;
    void clearStateChanged(const std::string& deviceId);

    const std::vector<domain::RelayState> getAllStates() const;
    const domain::RelayState* getState(const std::string& deviceId) const;

    bool isInitialSyncCompleted() const { return initialSyncCompleted_; }
    void setInitialSyncCompleted(bool synced) { initialSyncCompleted_ = synced; }

private:
    domain::ILogger& log_;
    std::map<std::string, ManagedRelay> relays_;
    bool initialSyncCompleted_ = false;
};

}
