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
    initWithStartupConfig(startupMode_, false, pulseEnabled_, pulseWidthMs_, 0, true, 0xFFU);
}

void RelayService::initWithStartupConfig(domain::StartupMode startupMode,
                                         bool lastSavedState,
                                         bool pulseEnabled,
                                         uint32_t pulseWidthMs,
                                         unsigned long nowMs,
                                         bool isPowerOnReset,
                                         uint8_t savedRelayMask) {
    startupMode_ = startupMode;
    pulseEnabled_ = pulseEnabled;
    if (pulseWidthMs >= PULSE_MIN_MS && pulseWidthMs <= PULSE_MAX_MS && (pulseWidthMs % PULSE_STEP_MS) == 0) {
        pulseWidthMs_ = pulseWidthMs;
    }

    if (configRepo_) {
        const uint32_t base = configRepo_->reserveStateVersionBlock(STATE_VER_BLOCK);
        stateVersion_ = base;
        reservedStateVersionCeiling_ = base + STATE_VER_BLOCK;
    }

    uint8_t effectiveMask = savedRelayMask;
    if (effectiveMask == 0xFFU) {
        effectiveMask = configRepo_
            ? configRepo_->loadRelayMask(lastSavedState)
            : (lastSavedState ? 0x01U : 0x00U);
    }

    lastSource_ = isPowerOnReset ? "POWER_RESTORE" : "SOFTWARE_RESET_RESTORE";
    log_.info(std::string("[RELAY] Inicializando relés (StartupMode=") +
              domain::startupModeToString(startupMode_) +
              ", PowerOnReset=" + (isPowerOnReset ? "true" : "false") +
              ", StateVer=" + std::to_string(stateVersion_) + ")...");

    size_t idx = 0;
    for (auto& pair : relays_) {
        auto& managed = pair.second;
        bool initialOn = false;
        if (!isPowerOnReset) {
            initialOn = ((effectiveMask >> idx) & 0x01U) != 0;
        } else if (startupMode_ == domain::StartupMode::On) {
            initialOn = true;
        } else if (startupMode_ == domain::StartupMode::Stay) {
            initialOn = ((effectiveMask >> idx) & 0x01U) != 0;
        }

        if (managed.controller) {
            managed.controller->init();
            managed.controller->setState(initialOn);
            const bool verified = managed.controller->readHardwareState();
            managed.state.currentState = verified;
            managed.state.desiredState = verified;
            managed.state.stateChanged = false;
            if (verified && pulseEnabled_) {
                pulseArmed_ = true;
                pulseStartMs_ = nowMs;
                pulseTargetDeviceId_ = pair.first;
            }
        }
        ++idx;
    }
    initialSyncCompleted_ = true;
    if (configRepo_) {
        configRepo_->saveRelayMask(getRelayMask());
    }
}

uint32_t RelayService::bumpStateVersion(const std::string& source) {
    if (!source.empty()) {
        lastSource_ = source;
    }
    ++stateVersion_;
    if (configRepo_ && stateVersion_ >= reservedStateVersionCeiling_) {
        const uint32_t nextBase = configRepo_->reserveStateVersionBlock(STATE_VER_BLOCK);
        if (nextBase > stateVersion_) {
            stateVersion_ = nextBase;
        }
        reservedStateVersionCeiling_ = stateVersion_ + STATE_VER_BLOCK;
    }
    return stateVersion_;
}

uint8_t RelayService::getRelayMask() const {
    uint8_t mask = 0;
    size_t idx = 0;
    for (const auto& pair : relays_) {
        if (idx < 8U && pair.second.state.currentState) {
            mask |= static_cast<uint8_t>(1U << idx);
        }
        ++idx;
    }
    return mask;
}

bool RelayService::verifyHardwareState(const std::string& deviceId) {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return false;
    auto& managed = it->second;
    if (managed.controller) {
        managed.state.currentState = managed.controller->readHardwareState();
    }
    return managed.state.currentState;
}

bool RelayService::setDesiredState(const std::string& deviceId, bool desired) {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return false;

    it->second.state.desiredState = desired;
    return true;
}

void RelayService::onRelayHardwareTransition(const std::string& deviceId,
                                             bool newState,
                                             unsigned long nowMs,
                                             const std::string& source) {
    bumpStateVersion(source);

    // 1. Persistir máscara real en NVS de forma diferida (150 ms) para preservar estado ante reinicios por software y Stay
    if (configRepo_) {
        deferredStateToSave_ = newState;
        deferredMaskToSave_ = getRelayMask();
        if (nowMs == 0) {
            configRepo_->saveRelayMask(deferredMaskToSave_);
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

bool RelayService::applyDesiredState(const std::string& deviceId,
                                     unsigned long nowMs,
                                     const std::string& source) {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return false;

    auto& managed = it->second;
    if (managed.controller) {
        managed.state.currentState = managed.controller->readHardwareState();
    }
    if (managed.state.currentState != managed.state.desiredState) {
        if (managed.controller) {
            managed.controller->setState(managed.state.desiredState);
            managed.state.currentState = managed.controller->readHardwareState();
        } else {
            managed.state.currentState = managed.state.desiredState;
        }
        managed.state.stateChanged = true;
        onRelayHardwareTransition(deviceId, managed.state.currentState, nowMs, source);
        log_.info("[RELAY] " + deviceId + " -> " +
                  (managed.state.currentState ? "ENCENDIDO" : "APAGADO") +
                  " (src=" + lastSource_ + ", ver=" + std::to_string(stateVersion_) + ")");
        return true;
    }
    return false;
}

void RelayService::applyAllDesiredStates(unsigned long nowMs, const std::string& source) {
    for (auto& pair : relays_) {
        applyDesiredState(pair.first, nowMs, source);
    }
}

bool RelayService::toggle(const std::string& deviceId,
                          unsigned long nowMs,
                          const std::string& source) {
    auto it = relays_.find(deviceId);
    if (it == relays_.end()) return false;

    if (it->second.controller) {
        it->second.state.currentState = it->second.controller->readHardwareState();
    }
    bool nextState = !it->second.state.currentState;
    it->second.state.desiredState = nextState;
    return applyDesiredState(deviceId, nowMs, source);
}

void RelayService::toggleAll(unsigned long nowMs, const std::string& source) {
    for (auto& pair : relays_) {
        if (pair.second.controller) {
            pair.second.state.currentState = pair.second.controller->readHardwareState();
        }
        bool nextState = !pair.second.state.currentState;
        pair.second.state.desiredState = nextState;
        applyDesiredState(pair.first, nowMs, source);
    }
}

bool RelayService::flushDeferredSave(unsigned long nowMs) {
    if (!pendingDeferredSave_ || !configRepo_) {
        return false;
    }
    if ((nowMs - lastToggleMs_) >= DEFERRED_SAVE_DELAY_MS) {
        pendingDeferredSave_ = false;
        configRepo_->saveRelayMask(deferredMaskToSave_);
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
        return applyDesiredState(pulseTargetDeviceId_, nowMs, "PULSE_AUTO_OFF");
    }
    return false;
}

void RelayService::setStartupMode(domain::StartupMode mode) {
    startupMode_ = mode;
    if (configRepo_) {
        configRepo_->saveStartupMode(mode);
        if (mode == domain::StartupMode::Stay) {
            configRepo_->saveRelayMask(getRelayMask());
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
