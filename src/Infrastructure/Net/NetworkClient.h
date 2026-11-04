#pragma once

#include <WiFiClient.h>
#include <stdint.h>

namespace relay::infra {

WiFiClient& getActiveNetworkClient();
void configureNetworkClient(const char* rootCa, uint16_t port = 8883);

}
