#pragma once

#include "Domain/Interfaces/ITimeProvider.h"

namespace relay::infra {

class NtpTimeProvider : public domain::ITimeProvider {
public:
    NtpTimeProvider(long gmtOffsetSec = -14400, int daylightOffsetSec = 0,
                    const char* server1 = "pool.ntp.org",
                    const char* server2 = "time.nist.gov")
        : gmtOffsetSec_(gmtOffsetSec), daylightOffsetSec_(daylightOffsetSec),
          server1_(server1), server2_(server2) {}

    bool sync() override;
    std::string isoTimestamp() override;
    bool isSynchronized() override;

private:
    long gmtOffsetSec_;
    int daylightOffsetSec_;
    const char* server1_;
    const char* server2_;
    bool synced_ = false;
};

}
