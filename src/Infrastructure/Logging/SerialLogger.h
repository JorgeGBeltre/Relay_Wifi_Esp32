#pragma once

#include <Arduino.h>
#include <string>
#include "Domain/Interfaces/ILogger.h"

namespace relay::infra {

class SerialLogger : public domain::ILogger {
public:
    void info(const std::string& msg) override {
        Serial.println(msg.c_str());
    }

    void warn(const std::string& msg) override {
        Serial.print("[WARN] ");
        Serial.println(msg.c_str());
    }

    void error(const std::string& msg) override {
        Serial.print("[ERROR] ");
        Serial.println(msg.c_str());
    }
};

}
