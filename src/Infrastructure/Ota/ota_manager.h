#pragma once

#include <Arduino.h>

void initOTA(const String& deviceId, const String& firmwareVersion);
void otaHandle();
bool otaInProgress();
