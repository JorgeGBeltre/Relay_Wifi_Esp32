#pragma once

#include <WiFiClient.h>

namespace relay::infra {

WiFiClient& getActiveNetworkClient();
void configureNetworkClient(const char* rootCa);

}
