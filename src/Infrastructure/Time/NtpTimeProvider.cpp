#include "Infrastructure/Time/NtpTimeProvider.h"
#include <Arduino.h>
#include <time.h>

namespace relay::infra {

bool NtpTimeProvider::sync() {
    configTime(gmtOffsetSec_, daylightOffsetSec_, server1_, server2_);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 2000)) {
        time_t now;
        time(&now);
        if (now > 1000000000) {
            synced_ = true;
            return true;
        }
    }
    return false;
}

bool NtpTimeProvider::isSynchronized() {
    time_t now;
    time(&now);
    return (now > 1000000000);
}

std::string NtpTimeProvider::isoTimestamp() {
    struct tm timeinfo;
    time_t now;
    time(&now);

    if (now < 1000000000 || !getLocalTime(&timeinfo, 100)) {
        unsigned long ms = millis();
        char fallback[32];
        snprintf(fallback, sizeof(fallback), "2026-01-01T00:00:%02lu.%03luZ",
                 (ms / 1000) % 60, ms % 1000);
        return fallback;
    }

    uint32_t ms = millis() % 1000;
    char timestamp[25];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &timeinfo);

    char fullTimestamp[32];
    snprintf(fullTimestamp, sizeof(fullTimestamp), "%s.%03uZ", timestamp, ms);

    return fullTimestamp;
}

}
