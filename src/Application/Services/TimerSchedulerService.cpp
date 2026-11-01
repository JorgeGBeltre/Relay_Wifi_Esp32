#include "Application/Services/TimerSchedulerService.h"

namespace relay::app {

namespace {

void epochToUtcCalendar(uint32_t epochSec, uint8_t& outWeekday, uint8_t& outHour, uint8_t& outMinute, uint8_t& outSecond) {
    outSecond = static_cast<uint8_t>(epochSec % 60U);
    const uint32_t totalMinutes = epochSec / 60U;
    outMinute = static_cast<uint8_t>(totalMinutes % 60U);
    const uint32_t totalHours = totalMinutes / 60U;
    outHour = static_cast<uint8_t>(totalHours % 24U);
    const uint32_t totalDays = totalHours / 24U;
    // 1970-01-01 fue Jueves (4): 0=Dom, 1=Lun, 2=Mar, 3=Mié, 4=Jue, 5=Vie, 6=Sáb
    outWeekday = static_cast<uint8_t>((totalDays + 4U) % 7U);
}

} // namespace

void TimerSchedulerService::loadFromStorage() {
    if (!configRepo_) return;
    for (size_t i = 0; i < MAX_TIMERS; ++i) {
        timers_[i] = domain::ScheduledTimer{};
    }
    configRepo_->loadTimers(timers_, MAX_TIMERS);
}

void TimerSchedulerService::saveToStorage() {
    if (!configRepo_) return;
    configRepo_->saveTimers(timers_, MAX_TIMERS);
}

bool TimerSchedulerService::upsertTimer(const domain::ScheduledTimer& timer) {
    domain::ScheduledTimer entry = timer;
    if (entry.id != 0) {
        for (size_t i = 0; i < MAX_TIMERS; ++i) {
            if (timers_[i].id == entry.id) {
                timers_[i] = entry;
                saveToStorage();
                return true;
            }
        }
    }
    for (size_t i = 0; i < MAX_TIMERS; ++i) {
        if (timers_[i].id == 0) {
            if (entry.id == 0) {
                entry.id = static_cast<uint8_t>(i + 1);
            }
            timers_[i] = entry;
            saveToStorage();
            return true;
        }
    }
    return false;
}

void TimerSchedulerService::clearAllTimers() {
    for (size_t i = 0; i < MAX_TIMERS; ++i) {
        timers_[i] = domain::ScheduledTimer{};
    }
    saveToStorage();
}

size_t TimerSchedulerService::getActiveTimerCount() const {
    size_t count = 0;
    for (size_t i = 0; i < MAX_TIMERS; ++i) {
        if (timers_[i].enabled) ++count;
    }
    return count;
}

size_t TimerSchedulerService::evaluate(uint32_t currentEpochSec, unsigned long nowMs) {
    if (currentEpochSec == 0) return 0;

    uint8_t weekday = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    epochToUtcCalendar(currentEpochSec, weekday, hour, minute, second);
    const uint32_t currentEpochMinute = currentEpochSec / 60U;

    size_t triggeredCount = 0;
    bool statePersistedNeeded = false;

    for (size_t i = 0; i < MAX_TIMERS; ++i) {
        auto& t = timers_[i];
        if (!t.enabled) continue;

        if (t.type == domain::TimerType::Once) {
            if (currentEpochSec >= t.targetEpochSec && (currentEpochSec - t.targetEpochSec) <= 5U) {
                relays_.setDesiredState(targetDeviceId_, t.targetAction);
                relays_.applyDesiredState(targetDeviceId_, nowMs);
                t.enabled = false;
                statePersistedNeeded = true;
                ++triggeredCount;
            }
        } else if (t.type == domain::TimerType::Repeat) {
            const bool dayMatch = (t.weekdaysMask & (1U << weekday)) != 0;
            if (dayMatch && hour == t.hourUtc && minute == t.minuteUtc &&
                second < 5U && t.lastTriggeredMinute != currentEpochMinute) {
                t.lastTriggeredMinute = currentEpochMinute;
                relays_.setDesiredState(targetDeviceId_, t.targetAction);
                relays_.applyDesiredState(targetDeviceId_, nowMs);
                ++triggeredCount;
            }
        } else if (t.type == domain::TimerType::Duration) {
            if (currentEpochSec >= t.cycleStartEpochSec && t.cyclePeriodSec > 0) {
                const uint32_t elapsed = (currentEpochSec - t.cycleStartEpochSec) % t.cyclePeriodSec;
                const bool desiredAction = (elapsed < t.cycleOnDurationSec) ? t.startAction : t.endAction;
                const auto* current = relays_.getState(targetDeviceId_);
                if (current && current->currentState != desiredAction) {
                    relays_.setDesiredState(targetDeviceId_, desiredAction);
                    relays_.applyDesiredState(targetDeviceId_, nowMs);
                    ++triggeredCount;
                }
            }
        }
    }

    if (statePersistedNeeded) {
        saveToStorage();
    }
    return triggeredCount;
}

} // namespace relay::app

