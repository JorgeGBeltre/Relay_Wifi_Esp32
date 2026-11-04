#include "Infrastructure/Net/NetworkClient.h"
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

namespace relay::infra {

static WiFiClient g_plainClient;
static WiFiClientSecure g_secureClient;
static bool g_useTls = false;

WiFiClient& getActiveNetworkClient() {
    return g_useTls ? static_cast<WiFiClient&>(g_secureClient) : g_plainClient;
}

void configureNetworkClient(const char* rootCa, uint16_t port) {
    if (port == 1883) {
        g_useTls = false;
        Serial.println("[NET] Puerto 1883 detectado -> Modo TCP en claro");
        return;
    }

    g_useTls = true;
    if (rootCa && strstr(rootCa, "YOUR_SSL_CERTIFICATE_HERE") == nullptr && strlen(rootCa) > 50) {
        g_secureClient.setCACert(rootCa);
        Serial.println("[NET] TLS Root CA configurado correctamente");
    } else {
        g_secureClient.setInsecure();
        Serial.println("[NET] TLS en modo Insecure (placeholder o certificado autofirmado)");
    }
}

}
