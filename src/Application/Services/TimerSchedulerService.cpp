#include "Application/Services/TimerSchedulerService.h"
#include <cstdio>
#include <cstring>

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

bool TimerSchedulerService::replaceAllTimers(const domain::ScheduledTimer* entries, size_t count) {
    for (size_t i = 0; i < MAX_TIMERS; ++i) {
        timers_[i] = domain::ScheduledTimer{};
    }
    const size_t safeCount = (count > MAX_TIMERS) ? MAX_TIMERS : count;
    for (size_t i = 0; i < safeCount && entries != nullptr; ++i) {
        domain::ScheduledTimer entry = entries[i];
        if (entry.id == 0) {
            entry.id = static_cast<uint8_t>(i + 1U);
        }
        if (entry.externalId[0] == '\0') {
            std::snprintf(entry.externalId, sizeof(entry.externalId), "%u", static_cast<unsigned>(entry.id));
        }
        entry.lastTriggeredMinute = 0;
        timers_[i] = entry;
    }
    saveToStorage();
    return true;
}

bool TimerSchedulerService::upsertTimer(const domain::ScheduledTimer& timer) {
    domain::ScheduledTimer entry = timer;
    if (entry.id != 0) {
        for (size_t i = 0; i < MAX_TIMERS; ++i) {
            if (timers_[i].id == entry.id) {
                if (entry.externalId[0] == '\0') {
                    std::snprintf(entry.externalId, sizeof(entry.externalId), "%u", static_cast<unsigned>(entry.id));
                }
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
            if (entry.externalId[0] == '\0') {
                std::snprintf(entry.externalId, sizeof(entry.externalId), "%u", static_cast<unsigned>(entry.id));
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
        if (timers_[i].id != 0 && timers_[i].enabled) ++count;
    }
    return count;
}

size_t TimerSchedulerService::applyActionToTarget(uint8_t channel, bool action, unsigned long nowMs, bool onlyIfChanged) {
    const auto allStates = relays_.getAllStates();
    if (allStates.empty()) {
        const auto* st = relays_.getState(targetDeviceId_);
        if (onlyIfChanged && st && st->currentState == action) {
            return 0;
        }
        relays_.setDesiredState(targetDeviceId_, action);
        relays_.applyDesiredState(targetDeviceId_, nowMs);
        return 1;
    }

    size_t changed = 0;
    if (channel == 0) {
        for (const auto& r : allStates) {
            if (onlyIfChanged && r.currentState == action) continue;
            relays_.setDesiredState(r.deviceId, action);
            relays_.applyDesiredState(r.deviceId, nowMs);
            ++changed;
        }
        return changed;
    }

    const size_t idx = (static_cast<size_t>(channel - 1U) < allStates.size())
        ? static_cast<size_t>(channel - 1U)
        : 0U;
    if (onlyIfChanged && allStates[idx].currentState == action) {
        return 0;
    }
    relays_.setDesiredState(allStates[idx].deviceId, action);
    relays_.applyDesiredState(allStates[idx].deviceId, nowMs);
    return 1;
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
        if (t.id == 0 || !t.enabled) continue;

        if (t.type == domain::TimerType::Once) {
            if (t.targetEpochSec > 0 && currentEpochSec >= t.targetEpochSec) {
                if ((currentEpochSec - t.targetEpochSec) <= 60U) {
                    applyActionToTarget(t.channel, t.targetAction, nowMs, false);
                    ++triggeredCount;
                }
                t.enabled = false;
                statePersistedNeeded = true;
            }
        } else if (t.type == domain::TimerType::Repeat) {
            const bool isOneShotSchedule = (t.weekdaysMask == 0);
            const bool dayMatch = isOneShotSchedule || ((t.weekdaysMask & (1U << weekday)) != 0);
            if (dayMatch && hour == t.hourUtc && minute == t.minuteUtc &&
                second < 15U && t.lastTriggeredMinute != currentEpochMinute) {
                t.lastTriggeredMinute = currentEpochMinute;
                applyActionToTarget(t.channel, t.targetAction, nowMs, false);
                ++triggeredCount;
                if (isOneShotSchedule) {
                    t.enabled = false;
                    statePersistedNeeded = true;
                }
            }
        } else if (t.type == domain::TimerType::Duration) {
            if (t.cyclePeriodSec >= 2U && t.cycleOnDurationSec > 0U && t.cycleOnDurationSec < t.cyclePeriodSec) {
                const uint32_t anchor = (t.cycleStartEpochSec > 0U && currentEpochSec >= t.cycleStartEpochSec)
                    ? t.cycleStartEpochSec
                    : currentEpochSec;
                const uint32_t elapsed = (currentEpochSec - anchor) % t.cyclePeriodSec;
                const bool desiredAction = (elapsed < t.cycleOnDurationSec) ? t.startAction : t.endAction;
                const uint32_t phaseToken = desiredAction ? 2U : 1U;
                if (t.lastTriggeredMinute != phaseToken) {
                    t.lastTriggeredMinute = phaseToken;
                    applyActionToTarget(t.channel, desiredAction, nowMs, false);
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
