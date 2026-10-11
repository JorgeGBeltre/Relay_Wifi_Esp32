#pragma once

#include <Arduino.h>

void initBLEProvisioning();
bool runBLEProvisioning(unsigned long timeoutMs = 60000);
