#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include "Application/Services/RelayService.h"
#include "Domain/Entities/ScheduledTimer.h"
#include "Domain/Interfaces/IConfigRepository.h"

namespace relay::app {

class TimerSchedulerService {
public:
    static constexpr size_t MAX_TIMERS = 8;

    TimerSchedulerService(RelayService& relays,
                          const std::string& targetDeviceId,
                          domain::IConfigRepository* configRepo = nullptr)
        : relays_(relays), targetDeviceId_(targetDeviceId), configRepo_(configRepo) {}

    void loadFromStorage();
    bool upsertTimer(const domain::ScheduledTimer& timer);
    bool replaceAllTimers(const domain::ScheduledTimer* entries, size_t count);
    void clearAllTimers();
    size_t getActiveTimerCount() const;
    const domain::ScheduledTimer* getTimers() const { return timers_; }

    // Evalúa los temporizadores una vez por segundo contra el epoch UTC actual
    size_t evaluate(uint32_t currentEpochSec, unsigned long nowMs = 0, bool* outTimersMutated = nullptr);

private:
    void saveToStorage();
    size_t applyActionToTarget(uint8_t channel, bool action, unsigned long nowMs, bool onlyIfChanged = false);

    RelayService& relays_;
    std::string targetDeviceId_;
    domain::IConfigRepository* configRepo_ = nullptr;
    domain::ScheduledTimer timers_[MAX_TIMERS]{};
};

} // namespace relay::app

