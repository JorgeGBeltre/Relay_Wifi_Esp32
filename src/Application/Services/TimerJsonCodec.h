#pragma once

#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>
#include "Application/Services/TimerSchedulerService.h"
#include "Domain/Entities/ScheduledTimer.h"

namespace relay::app {

class TimerJsonCodec {
public:
    static bool applyTimersFromJsonArray(JsonArrayConst arr,
                                         TimerSchedulerService& scheduler,
                                         uint32_t currentEpochSec = 0) {
        domain::ScheduledTimer parsed[TimerSchedulerService::MAX_TIMERS]{};
        size_t count = 0;
        uint8_t nextNumericId = 1;

        for (JsonVariantConst item : arr) {
            if (count >= TimerSchedulerService::MAX_TIMERS) break;
            if (!item.is<JsonObjectConst>()) continue;

            domain::ScheduledTimer t{};
            t.id = nextNumericId++;

            if (item["id"].is<const char*>()) {
                const char* extId = item["id"].as<const char*>();
                if (extId && extId[0] != '\0') {
                    std::snprintf(t.externalId, sizeof(t.externalId), "%s", extId);
                }
            } else if (item["mId"].is<const char*>()) {
                const char* extId = item["mId"].as<const char*>();
                if (extId && extId[0] != '\0') {
                    std::snprintf(t.externalId, sizeof(t.externalId), "%s", extId);
                }
            } else if (item["id"].is<uint32_t>()) {
                const uint32_t numId = item["id"].as<uint32_t>();
                if (numId >= 1U && numId <= 255U) {
                    t.id = static_cast<uint8_t>(numId);
                }
                std::snprintf(t.externalId, sizeof(t.externalId), "%u", static_cast<unsigned>(t.id));
            }
            if (t.externalId[0] == '\0') {
                std::snprintf(t.externalId, sizeof(t.externalId), "%u", static_cast<unsigned>(t.id));
            }

            if (item["enabled"].is<bool>()) {
                t.enabled = item["enabled"].as<bool>();
            } else if (item.containsKey("enabled")) {
                t.enabled = (item["enabled"].as<int>() != 0);
            } else {
                t.enabled = true;
            }

            if (item.containsKey("channel")) {
                const int ch = item["channel"].as<int>();
                t.channel = (ch >= 0 && ch <= 16) ? static_cast<uint8_t>(ch) : 1U;
            } else if (item.containsKey("outlet")) {
                const int outlet = item["outlet"].as<int>();
                t.channel = (outlet >= 0 && outlet < 16) ? static_cast<uint8_t>(outlet + 1) : 1U;
            } else {
                t.channel = 1U;
            }

            const char* tp = item["type"] | "once";
            if (std::strcmp(tp, "repeat") == 0 || std::strcmp(tp, "schedule") == 0) {
                t.type = domain::TimerType::Repeat;
            } else if (std::strcmp(tp, "duration") == 0 || std::strcmp(tp, "loop") == 0) {
                t.type = domain::TimerType::Duration;
            } else {
                t.type = domain::TimerType::Once;
            }

            const char* doSw = item["do"]["switch"] | (item["action"] | "off");
            t.targetAction = (std::strcmp(doSw, "on") == 0 || std::strcmp(doSw, "1") == 0 || std::strcmp(doSw, "true") == 0);

            const char* startSw = item["startDo"]["switch"] | "on";
            const char* endSw = item["endDo"]["switch"] | "off";
            t.startAction = (std::strcmp(startSw, "on") == 0);
            t.endAction = (std::strcmp(endSw, "on") == 0);

            t.targetEpochSec = item["at"] | (item["targetEpochSec"] | 0U);
            if (t.type == domain::TimerType::Once && t.targetEpochSec == 0U && item.containsKey("durationSec")) {
                const uint32_t durSec = item["durationSec"] | 0U;
                if (durSec > 0U && currentEpochSec > 1000000000U) {
                    t.targetEpochSec = currentEpochSec + durSec;
                }
            }

            if (item.containsKey("weekdays")) {
                t.weekdaysMask = static_cast<uint8_t>(item["weekdays"].as<uint32_t>() & 0x7FU);
            } else {
                t.weekdaysMask = 0x7FU;
            }
            t.hourUtc = static_cast<uint8_t>((item["hour"] | 0U) % 24U);
            t.minuteUtc = static_cast<uint8_t>((item["minute"] | 0U) % 60U);

            t.cycleStartEpochSec = item["startAt"] | 0U;
            if (t.type == domain::TimerType::Duration && t.cycleStartEpochSec == 0U && currentEpochSec > 1000000000U) {
                t.cycleStartEpochSec = currentEpochSec;
            }
            t.cyclePeriodSec = item["periodSec"] | 0U;
            t.cycleOnDurationSec = item["onDurationSec"] | 0U;
            if (t.cyclePeriodSec < 2U && (item.containsKey("onSec") || item.containsKey("offSec"))) {
                const uint32_t onS = item["onSec"] | 0U;
                const uint32_t offS = item["offSec"] | 0U;
                t.cycleOnDurationSec = onS;
                t.cyclePeriodSec = onS + offS;
            }

            parsed[count++] = t;
        }

        return scheduler.replaceAllTimers(parsed, count);
    }

    static void serializeTimersToJsonArray(const TimerSchedulerService& scheduler, JsonArray outArr) {
        const auto* timers = scheduler.getTimers();
        if (!timers) return;

        for (size_t i = 0; i < TimerSchedulerService::MAX_TIMERS; ++i) {
            const auto& t = timers[i];
            if (t.id == 0) continue;

            JsonObject obj = outArr.createNestedObject();
            if (t.externalId[0] != '\0') {
                obj["id"] = t.externalId;
            } else {
                obj["id"] = String(t.id);
            }
            obj["enabled"] = t.enabled;
            obj["channel"] = t.channel;

            if (t.type == domain::TimerType::Repeat) {
                obj["type"] = "repeat";
                obj["hour"] = t.hourUtc;
                obj["minute"] = t.minuteUtc;
                obj["weekdays"] = t.weekdaysMask;
                JsonObject doObj = obj.createNestedObject("do");
                doObj["switch"] = t.targetAction ? "on" : "off";
            } else if (t.type == domain::TimerType::Duration) {
                obj["type"] = "duration";
                obj["startAt"] = t.cycleStartEpochSec;
                obj["periodSec"] = t.cyclePeriodSec;
                obj["onDurationSec"] = t.cycleOnDurationSec;
                JsonObject startDo = obj.createNestedObject("startDo");
                startDo["switch"] = t.startAction ? "on" : "off";
                JsonObject endDo = obj.createNestedObject("endDo");
                endDo["switch"] = t.endAction ? "on" : "off";
            } else {
                obj["type"] = "once";
                obj["at"] = t.targetEpochSec;
                JsonObject doObj = obj.createNestedObject("do");
                doObj["switch"] = t.targetAction ? "on" : "off";
            }
        }
    }
};

} // namespace relay::app

