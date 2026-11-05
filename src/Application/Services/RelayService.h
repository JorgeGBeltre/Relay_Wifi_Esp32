#pragma once

#include "Domain/Entities/DeviceConfig.h"
#include "Domain/Entities/RelayState.h"
#include "Domain/Interfaces/IConfigRepository.h"
#include "Domain/Interfaces/ILogger.h"
#include "Domain/Interfaces/IRelayController.h"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace relay::app {

class RelayService {
public:
  static constexpr unsigned long DEFERRED_SAVE_DELAY_MS = 150;
  static constexpr uint32_t PULSE_MIN_MS = 500;
  static constexpr uint32_t PULSE_MAX_MS = 36000000;
  static constexpr uint32_t PULSE_STEP_MS = 500;

  struct ManagedRelay {
    domain::RelayState state;
    domain::IRelayController *controller;
  };

  explicit RelayService(domain::ILogger &logger,
                        domain::IConfigRepository *configRepo = nullptr)
      : log_(logger), configRepo_(configRepo) {}

  void attachConfigRepository(domain::IConfigRepository &repo) {
    configRepo_ = &repo;
  }

  void registerRelay(const std::string &deviceId,
                     domain::IRelayController &controller,
                     const std::string &topic);

  void init();
  void initWithStartupConfig(domain::StartupMode startupMode,
                             bool lastSavedState, bool pulseEnabled,
                             uint32_t pulseWidthMs, unsigned long nowMs = 0,
                             bool isPowerOnReset = true,
                             uint8_t savedRelayMask = 0xFFU);

  bool setDesiredState(const std::string &deviceId, bool desired);
  bool applyDesiredState(const std::string &deviceId, unsigned long nowMs = 0,
                         const std::string &source = "REMOTE_COMMAND");
  void applyAllDesiredStates(unsigned long nowMs = 0,
                             const std::string &source = "REMOTE_COMMAND");

  bool toggle(const std::string &deviceId, unsigned long nowMs = 0,
              const std::string &source = "PHYSICAL_BUTTON");
  void toggleAll(unsigned long nowMs = 0,
                 const std::string &source = "REMOTE_COMMAND");

  uint32_t getStateVersion() const { return stateVersion_; }
  uint32_t bumpStateVersion(const std::string &source);
  const std::string &getLastSource() const { return lastSource_; }
  uint8_t getRelayMask() const;
  bool verifyHardwareState(const std::string &deviceId);

  // Guardado diferido en NVS (150 ms tras última conmutación cuando
  // StartupMode::Stay)
  bool flushDeferredSave(unsigned long nowMs);
  bool hasPendingDeferredSave() const { return pendingDeferredSave_; }

  // Modo Pulso / Inching (500 ms .. 36,000,000 ms en pasos de 500 ms)
  bool configurePulse(bool enabled, uint32_t widthMs, unsigned long nowMs = 0);
  bool updatePulseTimer(unsigned long nowMs, std::string &outExpiredDeviceId);
  bool isPulseEnabled() const { return pulseEnabled_; }
  uint32_t getPulseWidthMs() const { return pulseWidthMs_; }
  bool isPulseActive() const { return pulseArmed_; }

  void setStartupMode(domain::StartupMode mode);
  domain::StartupMode getStartupMode() const { return startupMode_; }

  bool isStateChanged(const std::string &deviceId) const;
  void clearStateChanged(const std::string &deviceId);

  const std::vector<domain::RelayState> getAllStates() const;
  const domain::RelayState *getState(const std::string &deviceId) const;

  bool isInitialSyncCompleted() const { return initialSyncCompleted_; }
  void setInitialSyncCompleted(bool synced) { initialSyncCompleted_ = synced; }

private:
  void onRelayHardwareTransition(const std::string &deviceId, bool newState,
                                 unsigned long nowMs,
                                 const std::string &source);

  domain::ILogger &log_;
  domain::IConfigRepository *configRepo_ = nullptr;
  std::map<std::string, ManagedRelay> relays_;
  bool initialSyncCompleted_ = true;

  domain::StartupMode startupMode_ = domain::StartupMode::Off;
  bool pendingDeferredSave_ = false;
  bool deferredStateToSave_ = false;
  uint8_t deferredMaskToSave_ = 0;
  unsigned long lastToggleMs_ = 0;

  static constexpr uint32_t STATE_VER_BLOCK = 64U;
  uint32_t stateVersion_ = 1U;
  uint32_t reservedStateVersionCeiling_ = 0U;
  std::string lastSource_ = "BOOT";

  bool pulseEnabled_ = false;
  uint32_t pulseWidthMs_ = 500;
  bool pulseArmed_ = false;
  unsigned long pulseStartMs_ = 0;
  std::string pulseTargetDeviceId_;
};

} // namespace relay::app
