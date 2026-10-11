#pragma once

#include "Domain/Interfaces/IOtaService.h"
#include "Infrastructure/Ota/ota_manager.h"

namespace relay::infra {

class Mqttv5OtaService : public domain::IOtaService {
public:
    void begin(const std::string& deviceId, const std::string& firmwareVersion) override {
        initOTA(String(deviceId.c_str()), String(firmwareVersion.c_str()));
    }

    void handle() override {
        otaHandle();
    }

    bool isInProgress() override {
        return otaInProgress();
    }
};

}
