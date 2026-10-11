#include "Infrastructure/Net/NetworkClient.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>

namespace relay::infra {

static WiFiClientSecure g_secureClient;

WiFiClient& getActiveNetworkClient() {
    return g_secureClient;
}

void configureNetworkClient(const char* rootCa) {
    if (rootCa && strstr(rootCa, "YOUR_SSL_CERTIFICATE_HERE") == nullptr && strlen(rootCa) > 50) {
        g_secureClient.setCACert(rootCa);
        Serial.println("[NET] TLS Root CA configurado correctamente");
    } else {
        g_secureClient.setInsecure();
        Serial.println("[NET] TLS en modo Insecure (placeholder o certificado autofirmado)");
    }
}

}
