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
    initWithStartupConfig(startupMode_, false, pulseEnabled_, pulseWidthMs_, 0);
}

void RelayService::initWithStartupConfig(domain::StartupMode startupMode,
                                         bool lastSavedState,
                                         bool pulseEnabled,
                                         uint32_t pulseWidthMs,
                                         unsigned long nowMs) {
    startupMode_ = startupMode;
    pulseEnabled_ = pulseEnabled;
    if (pulseWidthMs >= PULSE_MIN_MS && pulseWidthMs <= PULSE_MAX_MS && (pulseWidthMs % PULSE_STEP_MS) == 0) {
        pulseWidthMs_ = pulseWidthMs;
    }

    bool initialOn = false;
    if (startupMode_ == domain::StartupMode::On) {
        initialOn = true;
    } else if (startupMode_ == domain::StartupMode::Stay) {
        initialOn = lastSavedState;
    }

    log_.info(std::string("[RELAY] Inicializando relés (StartupMode=") +
              domain::startupModeToString(startupMode_) +
              ", Estado=" + (initialOn ? "ENCENDIDO" : "APAGADO") + ")...");

    for (auto& pair : relays_) {
        auto& managed = pair.second;
        if (managed.controller) {
            managed.controller->init();
            managed.controller->setState(initialOn);
            managed.state.currentState = initialOn;
            managed.state.desiredState = initialOn;
            managed.state.stateChanged = false;
            if (initialOn && pulseEnabled_) {
                pulseArmed_ = true;
                pulseStartMs_ = nowMs;
                pulseTargetDeviceId_ = pair.first;
            }
        }
    }
}

bool RelayService::setDesiredState(const std::string& deviceId, bool desired) {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return false;

    it->second.state.desiredState = desired;
    return true;
}

void RelayService::onRelayHardwareTransition(const std::string& deviceId, bool newState, unsigned long nowMs) {
    // 1. Si StartupMode == Stay, agendar guardado diferido de 150 ms (Sonoff 0x40232DDD)
    if (startupMode_ == domain::StartupMode::Stay && configRepo_) {
        deferredStateToSave_ = newState;
        if (nowMs == 0) {
            configRepo_->saveLastRelayState(newState);
            pendingDeferredSave_ = false;
        } else {
            lastToggleMs_ = nowMs;
            pendingDeferredSave_ = true;
        }
    }

    // 2. Armar o cancelar el temporizador one-shot de Modo Pulso (Inching)
    if (pulseEnabled_ && newState) {
        pulseArmed_ = true;
        pulseStartMs_ = nowMs;
        pulseTargetDeviceId_ = deviceId;
    } else if (!newState && pulseTargetDeviceId_ == deviceId) {
        pulseArmed_ = false;
    }
}

bool RelayService::applyDesiredState(const std::string& deviceId, unsigned long nowMs) {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return false;

    auto& managed = it->second;
    if (managed.state.currentState != managed.state.desiredState) {
        managed.state.currentState = managed.state.desiredState;
        if (managed.controller) {
            managed.controller->setState(managed.state.currentState);
        }
        managed.state.stateChanged = true;
        onRelayHardwareTransition(deviceId, managed.state.currentState, nowMs);
        log_.info("[RELAY] " + deviceId + " -> " + (managed.state.currentState ? "ENCENDIDO" : "APAGADO"));
        return true;
    }
    return false;
}

void RelayService::applyAllDesiredStates(unsigned long nowMs) {
    for (auto& pair : relays_) {
        applyDesiredState(pair.first, nowMs);
    }
}

bool RelayService::toggle(const std::string& deviceId, unsigned long nowMs) {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return false;

    bool nextState = !it->second.state.currentState;
    it->second.state.desiredState = nextState;
    return applyDesiredState(deviceId, nowMs);
}

void RelayService::toggleAll(unsigned long nowMs) {
    for (auto& pair : relays_) {
        bool nextState = !pair.second.state.currentState;
        pair.second.state.desiredState = nextState;
        applyDesiredState(pair.first, nowMs);
    }
}

bool RelayService::flushDeferredSave(unsigned long nowMs) {
    if (!pendingDeferredSave_ || !configRepo_) {
        return false;
    }
    if ((nowMs - lastToggleMs_) >= DEFERRED_SAVE_DELAY_MS) {
        pendingDeferredSave_ = false;
        configRepo_->saveLastRelayState(deferredStateToSave_);
        return true;
    }
    return false;
}

bool RelayService::configurePulse(bool enabled, uint32_t widthMs, unsigned long nowMs) {
    if (widthMs < PULSE_MIN_MS || widthMs > PULSE_MAX_MS || (widthMs % PULSE_STEP_MS) != 0) {
        return false;
    }
    pulseEnabled_ = enabled;
    pulseWidthMs_ = widthMs;
    if (!enabled) {
        pulseArmed_ = false;
    } else {
        for (const auto& pair : relays_) {
            if (pair.second.state.currentState) {
                pulseArmed_ = true;
                pulseStartMs_ = nowMs;
                pulseTargetDeviceId_ = pair.first;
                break;
            }
        }
    }
    if (configRepo_) {
        configRepo_->savePulseConfig(pulseEnabled_, pulseWidthMs_);
    }
    return true;
}

bool RelayService::updatePulseTimer(unsigned long nowMs, std::string& outExpiredDeviceId) {
    if (!pulseEnabled_ || !pulseArmed_) {
        return false;
    }
    if ((nowMs - pulseStartMs_) >= pulseWidthMs_) {
        pulseArmed_ = false;
        outExpiredDeviceId = pulseTargetDeviceId_;
        setDesiredState(pulseTargetDeviceId_, false);
        return applyDesiredState(pulseTargetDeviceId_, nowMs);
    }
    return false;
}

void RelayService::setStartupMode(domain::StartupMode mode) {
    startupMode_ = mode;
    if (configRepo_) {
        configRepo_->saveStartupMode(mode);
        if (mode == domain::StartupMode::Stay) {
            for (const auto& pair : relays_) {
                configRepo_->saveLastRelayState(pair.second.state.currentState);
                break;
            }
        }
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
