#include "Application/Services/RelayService.h"

namespace relay::app {

void RelayService::registerRelay(const std::string& deviceId,
                                 domain::IRelayController& controller,
                                 const std::string& topic) {
    domain::RelayState state;
    state.deviceId = deviceId;
    state.pin = controller.getPin();
    state.currentState = false;
    state.desiredState = false;
    state.topic = topic;
    state.stateChanged = false;

    relays_[deviceId] = ManagedRelay{state, &controller};
}

void RelayService::init() {
    log_.info("[RELAY] Inicializando relés en estado APAGADO...");
    for (auto& pair : relays_) {
        auto& managed = pair.second;
        if (managed.controller) {
            managed.controller->init();
            managed.controller->setState(false);
            managed.state.currentState = false;
            managed.state.desiredState = false;
            managed.state.stateChanged = false;
        }
    }
}

bool RelayService::setDesiredState(const std::string& deviceId, bool desired) {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return false;

    it->second.state.desiredState = desired;
    return true;
}

bool RelayService::applyDesiredState(const std::string& deviceId) {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return false;

    auto& managed = it->second;
    if (managed.state.currentState != managed.state.desiredState) {
        managed.state.currentState = managed.state.desiredState;
        if (managed.controller) {
            managed.controller->setState(managed.state.currentState);
        }
        managed.state.stateChanged = true;
        log_.info("[RELAY] " + deviceId + " -> " + (managed.state.currentState ? "ENCENDIDO" : "APAGADO"));
        return true;
    }
    return false;
}

void RelayService::applyAllDesiredStates() {
    for (auto& pair : relays_) {
        applyDesiredState(pair.first);
    }
}

bool RelayService::toggle(const std::string& deviceId) {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return false;

    bool nextState = !it->second.state.currentState;
    it->second.state.desiredState = nextState;
    return applyDesiredState(deviceId);
}

void RelayService::toggleAll() {
    for (auto& pair : relays_) {
        bool nextState = !pair.second.state.currentState;
        pair.second.state.desiredState = nextState;
        applyDesiredState(pair.first);
    }
}

bool RelayService::isStateChanged(const std::string& deviceId) const {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return false;
    return it->second.state.stateChanged;
}

void RelayService::clearStateChanged(const std::string& deviceId) {
    auto it = relays_.find(deviceId);
    if (it != relays_.end()) {
        it->second.state.stateChanged = false;
    }
}

const std::vector<domain::RelayState> RelayService::getAllStates() const {
    std::vector<domain::RelayState> list;
    list.reserve(relays_.size());
    for (const auto& pair : relays_) {
        list.push_back(pair.second.state);
    }
    return list;
}

const domain::RelayState* RelayService::getState(const std::string& deviceId) const {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return nullptr;
    return &it->second.state;
}

}
