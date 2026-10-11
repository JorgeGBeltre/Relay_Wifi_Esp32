#include "Infrastructure/Provisioning/ble_provisioning.h"
#include "Infrastructure/Config/nvs_config.h"
#include "Infrastructure/Config/config.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <sstream>

#define SERVICE_UUID          "12345678-1234-1234-1234-123456789abc"
#define CHAR_BROKER_UUID      "aaaaaaaa-bbbb-cccc-dddd-000000000004"
#define CHAR_PORT_UUID        "aaaaaaaa-bbbb-cccc-dddd-000000000005"
#define CHAR_MQTTUSER_UUID    "aaaaaaaa-bbbb-cccc-dddd-000000000006"
#define CHAR_MQTTPASS_UUID    "aaaaaaaa-bbbb-cccc-dddd-000000000007"
#define CHAR_DEVID_UUID       "aaaaaaaa-bbbb-cccc-dddd-000000000008"
#define CHAR_OTAKEY_UUID      "aaaaaaaa-bbbb-cccc-dddd-000000000011"
#define CHAR_WIFISSID_UUID    "aaaaaaaa-bbbb-cccc-dddd-000000000012"
#define CHAR_WIFIPASS_UUID    "aaaaaaaa-bbbb-cccc-dddd-000000000013"
#define CHAR_FRAME_JSON_UUID  "aaaaaaaa-bbbb-cccc-dddd-000000000014"
#define CHAR_DEVKEY_UUID      "aaaaaaaa-bbbb-cccc-dddd-000000000015"
#define CHAR_BINDTOKEN_UUID   "aaaaaaaa-bbbb-cccc-dddd-000000000016"
#define CHAR_SAVE_UUID        "aaaaaaaa-bbbb-cccc-dddd-000000000099"

static bool _configSaved = false;

static bool parseJsonCredentials(const std::string& raw) {
    size_t first = raw.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || raw[first] != '{') return false;

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, raw.substr(first));
    if (error) {
        Serial.printf("[BLE] Error parseando trama JSON: %s\n", error.c_str());
        return false;
    }

    if (doc.containsKey("wifi_ssid")) config.wifi_ssid = doc["wifi_ssid"].as<String>();
    else if (doc.containsKey("ssid")) config.wifi_ssid = doc["ssid"].as<String>();

    if (doc.containsKey("wifi_pass")) config.wifi_pass = doc["wifi_pass"].as<String>();
    else if (doc.containsKey("pass")) config.wifi_pass = doc["pass"].as<String>();
    else if (doc.containsKey("password")) config.wifi_pass = doc["password"].as<String>();

    if (doc.containsKey("mqtt_broker")) config.mqtt_broker = doc["mqtt_broker"].as<String>();
    else if (doc.containsKey("broker")) config.mqtt_broker = doc["broker"].as<String>();
    else if (doc.containsKey("server")) config.mqtt_broker = doc["server"].as<String>();

    if (doc.containsKey("mqtt_port")) config.mqtt_port = doc["mqtt_port"].as<int>();
    else if (doc.containsKey("port")) config.mqtt_port = doc["port"].as<int>();

    if (doc.containsKey("account")) config.mqtt_user = doc["account"].as<String>();
    else if (doc.containsKey("user")) config.mqtt_user = doc["user"].as<String>();
    else if (doc.containsKey("user_id")) config.mqtt_user = doc["user_id"].as<String>();
    else if (doc.containsKey("account_id")) config.mqtt_user = doc["account_id"].as<String>();
    else if (doc.containsKey("email")) config.mqtt_user = doc["email"].as<String>();
    else if (doc.containsKey("mqtt_user")) config.mqtt_user = doc["mqtt_user"].as<String>();
    else if (doc.containsKey("api_key")) config.mqtt_user = doc["api_key"].as<String>();

    if (doc.containsKey("mqtt_pass")) config.mqtt_pass = doc["mqtt_pass"].as<String>();
    else if (doc.containsKey("pass_mqtt")) config.mqtt_pass = doc["pass_mqtt"].as<String>();

    if (doc.containsKey("device_id")) config.device_id = doc["device_id"].as<String>();
    else if (doc.containsKey("id")) config.device_id = doc["id"].as<String>();
    else if (doc.containsKey("deviceid")) config.device_id = doc["deviceid"].as<String>();

    if (doc.containsKey("device_key")) config.device_key = doc["device_key"].as<String>();
    else if (doc.containsKey("devicekey")) config.device_key = doc["devicekey"].as<String>();
    else if (doc.containsKey("apikey")) config.device_key = doc["apikey"].as<String>();

    if (doc.containsKey("bind_token")) config.bind_token = doc["bind_token"].as<String>();
    else if (doc.containsKey("token")) config.bind_token = doc["token"].as<String>();
    else if (doc.containsKey("claim_token")) config.bind_token = doc["claim_token"].as<String>();
    else if (doc.containsKey("account_token")) config.bind_token = doc["account_token"].as<String>();

    if (doc.containsKey("ota_hmac_key")) config.ota_hmac_key = doc["ota_hmac_key"].as<String>();
    else if (doc.containsKey("ota_key")) config.ota_hmac_key = doc["ota_key"].as<String>();

    Serial.println("[BLE] Trama JSON de aprovisionamiento procesada con éxito");
    return true;
}

static bool parseKeyValueFrame(const std::string& raw) {
    auto trim = [](const std::string& s) -> std::string {
        size_t start = s.find_first_not_of(" \t\r\n\"'");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n\"'");
        return s.substr(start, end - start + 1);
    };

    bool matched = false;
    std::string cur = raw;
    for (char& c : cur) {
        if (c == '&' || c == '\n' || c == '\r') c = ';';
    }

    std::stringstream ss(cur);
    std::string token;
    while (std::getline(ss, token, ';')) {
        if (token.empty()) continue;
        size_t sep = token.find('=');
        if (sep == std::string::npos) sep = token.find(':');
        if (sep == std::string::npos) continue;

        std::string k = trim(token.substr(0, sep));
        std::string v = trim(token.substr(sep + 1));
        for (char& ch : k) ch = tolower((unsigned char)ch);

        if (k == "wifi_ssid" || k == "ssid") { config.wifi_ssid = v.c_str(); matched = true; }
        else if (k == "wifi_pass" || k == "pass" || k == "password") { config.wifi_pass = v.c_str(); matched = true; }
        else if (k == "mqtt_broker" || k == "broker" || k == "server") { config.mqtt_broker = v.c_str(); matched = true; }
        else if (k == "mqtt_port" || k == "port") { config.mqtt_port = atoi(v.c_str()); matched = true; }
        else if (k == "account" || k == "user" || k == "user_id" || k == "account_id" || k == "email" || k == "mqtt_user" || k == "api_key") { config.mqtt_user = v.c_str(); matched = true; }
        else if (k == "mqtt_pass" || k == "pass_mqtt") { config.mqtt_pass = v.c_str(); matched = true; }
        else if (k == "device_id" || k == "id" || k == "deviceid") { config.device_id = v.c_str(); matched = true; }
        else if (k == "device_key" || k == "devicekey" || k == "apikey") { config.device_key = v.c_str(); matched = true; }
        else if (k == "bind_token" || k == "token" || k == "claim_token" || k == "account_token") { config.bind_token = v.c_str(); matched = true; }
        else if (k == "ota_hmac_key" || k == "ota_key") { config.ota_hmac_key = v.c_str(); matched = true; }
    }

    if (matched) {
        Serial.println("[BLE] Trama Clave=Valor de aprovisionamiento procesada con éxito");
    }
    return matched;
}

static bool parseFrame(const std::string& raw) {
    if (raw.empty()) return false;
    if (parseJsonCredentials(raw)) return true;
    if (parseKeyValueFrame(raw)) return true;
    return false;
}

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        Serial.println("[BLE] App móvil conectada al dispositivo.");
    }
    void onDisconnect(NimBLEServer* pServer) override {
        Serial.println("[BLE] App móvil desconectada.");
    }
};

class StringWriteCallback : public NimBLECharacteristicCallbacks {
public:
    String* _target;
    explicit StringWriteCallback(String* target) : _target(target) {}
    void onRead(NimBLECharacteristic* c) override {
        c->setValue(_target->c_str());
    }
    void onWrite(NimBLECharacteristic* c) override {
        std::string val = c->getValue();
        *_target = val.c_str();
        c->setValue(val);
        Serial.printf("[BLE] %s = %s\n", c->getUUID().toString().c_str(), _target->c_str());
    }
};

class PortWriteCallback : public NimBLECharacteristicCallbacks {
public:
    int* _target;
    explicit PortWriteCallback(int* target) : _target(target) {}
    void onRead(NimBLECharacteristic* c) override {
        c->setValue(String(*_target).c_str());
    }
    void onWrite(NimBLECharacteristic* c) override {
        std::string val = c->getValue();
        if (val.size() == 2 && (!isdigit((unsigned char)val[0]) || !isdigit((unsigned char)val[1]))) {
            uint16_t p = (uint8_t)val[0] | ((uint8_t)val[1] << 8);
            *_target = p;
        } else if (!val.empty()) {
            *_target = atoi(val.c_str());
        }
        c->setValue(String(*_target).c_str());
        Serial.printf("[BLE] Puerto MQTT/Servidor = %d\n", *_target);
    }
};

class DevIdCallback : public NimBLECharacteristicCallbacks {
public:
    void onRead(NimBLECharacteristic* c) override {
        c->setValue(config.device_id.c_str());
        Serial.printf("[BLE] Lectura de Device ID solicitada: %s\n", config.device_id.c_str());
    }
    void onWrite(NimBLECharacteristic* c) override {
        std::string val = c->getValue();
        if (!val.empty()) {
            config.device_id = val.c_str();
            c->setValue(config.device_id.c_str());
            Serial.printf("[BLE] Device ID personalizado = %s\n", config.device_id.c_str());
        }
    }
};

class DevKeyCallback : public NimBLECharacteristicCallbacks {
public:
    void onRead(NimBLECharacteristic* c) override {
        c->setValue(config.device_key.c_str());
        Serial.printf("[BLE] Lectura de Device Key (apikey) para verificación: %s\n", config.device_key.c_str());
    }
};

class BindTokenCallback : public NimBLECharacteristicCallbacks {
public:
    void onRead(NimBLECharacteristic* c) override {
        c->setValue(config.bind_token.c_str());
    }
    void onWrite(NimBLECharacteristic* c) override {
        std::string val = c->getValue();
        config.bind_token = val.c_str();
        c->setValue(val);
        Serial.printf("[BLE] Token de enlace de cuenta recibido: %s\n", config.bind_token.c_str());
    }
};

class SaveCallback : public NimBLECharacteristicCallbacks {
public:
    void onRead(NimBLECharacteristic* c) override {
        c->setValue(_configSaved ? "OK" : "READY");
    }
    void onWrite(NimBLECharacteristic* c) override {
        std::string val = c->getValue();
        if (!val.empty()) {
            parseFrame(val);
        }
        saveConfig();
        _configSaved = true;
        Serial.println("[BLE] Configuración y enlace guardados en NVS. Enviando ACK 'OK'...");
        c->setValue("OK");
        c->notify();
    }
};

class FrameCallback : public NimBLECharacteristicCallbacks {
public:
    void onRead(NimBLECharacteristic* c) override {
        StaticJsonDocument<512> idDoc;
        idDoc["device_type"] = "Relay-Wifi";
        idDoc["device_id"]   = config.device_id;
        idDoc["device_key"]  = config.device_key;
        idDoc["mac"]         = WiFi.macAddress();
        idDoc["version"]     = FIRMWARE_VERSION;
        idDoc["wifi_ssid"]   = config.wifi_ssid;
        idDoc["server"]      = config.mqtt_broker;
        idDoc["port"]        = config.mqtt_port;
        idDoc["account"]     = config.mqtt_user;
        idDoc["bind_token"]  = config.bind_token;
        idDoc["status"]      = _configSaved ? "configured" : "ready_to_claim";

        String json;
        serializeJson(idDoc, json);
        c->setValue(json.c_str());
        Serial.printf("[BLE] Reporte de identidad entregado a la app (%u bytes): %s\n", json.length(), json.c_str());
    }

    void onWrite(NimBLECharacteristic* c) override {
        std::string val = c->getValue();
        if (parseFrame(val)) {
            saveConfig();
            _configSaved = true;
            Serial.println("[BLE] Trama procesada y vinculación guardada en NVS. Enviando ACK 'OK'...");
            c->setValue("OK");
            c->notify();
        } else {
            Serial.println("[BLE] Error: Trama de aprovisionamiento inválida.");
            c->setValue("ERROR:INVALID_FRAME");
            c->notify();
        }
    }
};

void initBLEProvisioning() {
}

bool runBLEProvisioning(unsigned long timeoutMs) {
    Serial.println("\n[BLE] =========================================");
    Serial.println("[BLE] Iniciando servidor BLE de aprovisionamiento (Estilo Sonoff)...");
    Serial.println("[BLE] =========================================");
    _configSaved = false;

    if (config.device_id.length() == 0) {
        uint64_t chipId = ESP.getEfuseMac();
        char buf[16];
        snprintf(buf, sizeof(buf), "%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
        config.device_id = String(buf);
    }

    if (config.device_key.length() == 0) {
        uint64_t mac = ESP.getEfuseMac();
        char buf[37];
        snprintf(buf, sizeof(buf), "%08X-%04X-4%03X-%04X-%012llX",
                 (uint32_t)(mac & 0xFFFFFFFF),
                 (uint16_t)((mac >> 32) & 0xFFFF),
                 (uint16_t)((mac >> 16) & 0x0FFF),
                 (uint16_t)(((mac >> 48) & 0x3FFF) | 0x8000),
                 (unsigned long long)(mac ^ 0x5A5A5A5A5A5AULL));
        config.device_key = String(buf);
    }

    String advName = "RELAY-" + config.device_id;
    NimBLEDevice::init(advName.c_str());
    NimBLEDevice::setMTU(512);

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    NimBLEService* dis = server->createService(NimBLEUUID((uint16_t)0x180A));
    if (dis) {
        NimBLECharacteristic* mChar = dis->createCharacteristic(
            NimBLEUUID((uint16_t)0x2A24), NIMBLE_PROPERTY::READ);
        mChar->setValue("Relay-Wifi ESP32");

        NimBLECharacteristic* sChar = dis->createCharacteristic(
            NimBLEUUID((uint16_t)0x2A25), NIMBLE_PROPERTY::READ);
        sChar->setValue(config.device_id.c_str());

        NimBLECharacteristic* fwChar = dis->createCharacteristic(
            NimBLEUUID((uint16_t)0x2A26), NIMBLE_PROPERTY::READ);
        fwChar->setValue(FIRMWARE_VERSION);

        NimBLECharacteristic* hwChar = dis->createCharacteristic(
            NimBLEUUID((uint16_t)0x2A27), NIMBLE_PROPERTY::READ);
        hwChar->setValue("ESP32");

        NimBLECharacteristic* mfChar = dis->createCharacteristic(
            NimBLEUUID((uint16_t)0x2A29), NIMBLE_PROPERTY::READ);
        mfChar->setValue("VESIS");

        dis->start();
    }

    NimBLEService* service = server->createService(SERVICE_UUID);

    auto mkChar = [&](const char* uuid, String* field) {
        NimBLECharacteristic* ch = service->createCharacteristic(
            uuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);
        ch->setCallbacks(new StringWriteCallback(field));
        ch->setValue(field->c_str());
        return ch;
    };

    mkChar(CHAR_WIFISSID_UUID,  &config.wifi_ssid);
    mkChar(CHAR_WIFIPASS_UUID,  &config.wifi_pass);
    mkChar(CHAR_BROKER_UUID,    &config.mqtt_broker);
    mkChar(CHAR_MQTTUSER_UUID,  &config.mqtt_user);
    mkChar(CHAR_MQTTPASS_UUID,  &config.mqtt_pass);
    mkChar(CHAR_OTAKEY_UUID,    &config.ota_hmac_key);

    NimBLECharacteristic* devIdChar = service->createCharacteristic(
        CHAR_DEVID_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);
    devIdChar->setCallbacks(new DevIdCallback());
    devIdChar->setValue(config.device_id.c_str());

    NimBLECharacteristic* devKeyChar = service->createCharacteristic(
        CHAR_DEVKEY_UUID, NIMBLE_PROPERTY::READ);
    devKeyChar->setCallbacks(new DevKeyCallback());
    devKeyChar->setValue(config.device_key.c_str());

    NimBLECharacteristic* bindTokenChar = service->createCharacteristic(
        CHAR_BINDTOKEN_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);
    bindTokenChar->setCallbacks(new BindTokenCallback());
    bindTokenChar->setValue(config.bind_token.c_str());

    NimBLECharacteristic* portChar = service->createCharacteristic(
        CHAR_PORT_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);
    portChar->setCallbacks(new PortWriteCallback(&config.mqtt_port));
    portChar->setValue(String(config.mqtt_port).c_str());

    NimBLECharacteristic* frameChar = service->createCharacteristic(
        CHAR_FRAME_JSON_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    frameChar->setCallbacks(new FrameCallback());

    StaticJsonDocument<512> initDoc;
    initDoc["device_type"] = "Relay-Wifi";
    initDoc["device_id"]   = config.device_id;
    initDoc["device_key"]  = config.device_key;
    initDoc["mac"]         = WiFi.macAddress();
    initDoc["version"]     = FIRMWARE_VERSION;
    initDoc["wifi_ssid"]   = config.wifi_ssid;
    initDoc["server"]      = config.mqtt_broker;
    initDoc["port"]        = config.mqtt_port;
    initDoc["account"]     = config.mqtt_user;
    initDoc["bind_token"]  = config.bind_token;
    initDoc["status"]      = "ready_to_claim";
    String initJson;
    serializeJson(initDoc, initJson);
    frameChar->setValue(initJson.c_str());

    NimBLECharacteristic* saveChar = service->createCharacteristic(
        CHAR_SAVE_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    saveChar->setCallbacks(new SaveCallback());
    saveChar->setValue("READY");

    service->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setName(advName.c_str());
    adv->start();

    Serial.printf("[BLE] Publicitando como '%s' durante %lu ms\n", advName.c_str(), timeoutMs);
    Serial.printf("[BLE] Service UUID: %s\n", SERVICE_UUID);
    Serial.printf("[BLE] Device ID:    %s\n", config.device_id.c_str());
    Serial.printf("[BLE] Device Key:   %s\n", config.device_key.c_str());
    Serial.println("[BLE] Estado: Dispositivo listo para ser reclamado (ready_to_claim)...");

    unsigned long start = millis();
    while ((millis() - start) < timeoutMs) {
        if (_configSaved) {
            Serial.println("[BLE] ¡Aprovisionamiento y enlace a la cuenta completados con éxito!");
            delay(600);
            adv->stop();
            NimBLEDevice::deinit(true);
            return true;
        }
        delay(100);
    }

    Serial.println("[BLE] Tiempo de espera agotado (timeout) sin recibir configuración.");
    adv->stop();
    NimBLEDevice::deinit(true);
    return false;
}
