#include "Infrastructure/Ota/ota_manager.h"
#include "Infrastructure/Messaging/mqtt_client.h"
#include "MQTTOTAv5.h"

extern ESP32_MQTTv5 mqtt;

static MQTTOTAv5 ota;
static volatile bool g_otaInProgress = false;

bool otaInProgress() {
    return g_otaInProgress || ota.isUpdateInProgress();
}

void initOTA(const String& deviceId, const String& firmwareVersion) {
    Serial.printf("[OTA] Inicializando MQTTOTAv5 para %s (v%s)...\n",
                  deviceId.c_str(), firmwareVersion.c_str());

    ota.begin(mqtt, deviceId, firmwareVersion);
    ota.requireSignature(false);

    ota.onProgress([](int pct, const String& ver) {
        g_otaInProgress = true;
        Serial.printf("[OTA] Descargando v%s: %d%% completado\n", ver.c_str(), pct);
    });

    ota.onSuccess([](const String& ver) {
        g_otaInProgress = false;
        Serial.printf("[OTA] Firmware v%s instalado con éxito! Reiniciando...\n", ver.c_str());
    });

    ota.onError([](const String& err, const String& ver) {
        g_otaInProgress = false;
        Serial.printf("[OTA] Error actualizando a v%s: %s\n", ver.c_str(), err.c_str());
    });

    ota.onStateChange([](uint8_t state) {
        Serial.printf("[OTA] Cambio de estado -> %d\n", state);
    });

    Serial.println("[OTA] MQTTOTAv5 listo y registrado.");
}

void otaHandle() {
    ota.handle();
}
