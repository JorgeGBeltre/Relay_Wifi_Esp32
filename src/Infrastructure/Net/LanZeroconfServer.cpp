#include "Infrastructure/Net/LanZeroconfServer.h"
#include "Application/Services/CryptoUtils.h"
#include "Application/Services/TimerJsonCodec.h"
#include "Infrastructure/Config/config.h"
#include "Infrastructure/Config/nvs_config.h"
#include "Infrastructure/Messaging/mqtt_client.h"
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mbedtls/sha256.h>

namespace {

void computeLanProofHex(const std::string &deviceKey, const std::string &chipId,
                        const std::string &localIp, char outHex[33]) {
  outHex[0] = '\0';
  if (deviceKey.empty() || chipId.empty() || localIp.empty()) {
    return;
  }
  const std::string input = deviceKey + ":" + chipId + ":" + localIp;
  uint8_t digest[32] = {0};
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(
      &ctx, reinterpret_cast<const unsigned char *>(input.data()),
      input.size());
  mbedtls_sha256_finish_ret(&ctx, digest);
  mbedtls_sha256_free(&ctx);

  for (size_t i = 0; i < 16; ++i) {
    std::snprintf(outHex + (i * 2), 3, "%02x", digest[i]);
  }
  outHex[32] = '\0';
}

} // namespace

namespace relay::infra {

LanZeroconfServer::LanZeroconfServer(
    app::RelayService &relays, app::TelemetryService &telemetry,
    app::TimerSchedulerService &timerScheduler, domain::IIndicator &indicator,
    domain::IConfigRepository &configRepo, const std::string &activeDeviceId,
    const std::string &relayDeviceId, uint16_t port)
    : relays_(relays), telemetry_(telemetry), timerScheduler_(timerScheduler),
      indicator_(indicator), configRepo_(configRepo),
      activeDeviceId_(activeDeviceId), relayDeviceId_(relayDeviceId),
      port_(port), server_(port) {}

void LanZeroconfServer::registerEndpoint(const char *uri) {
  server_.on(uri, HTTP_OPTIONS, [this]() {
    server_.sendHeader("Access-Control-Allow-Origin", "*");
    server_.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server_.sendHeader("Access-Control-Allow-Headers",
                       "Content-Type, Authorization");
    server_.sendHeader("Access-Control-Allow-Private-Network", "true");
    server_.send(204, "text/plain", "");
  });
  server_.on(uri, HTTP_POST, [this, uri]() {
    String body = server_.arg("plain");
    responseBuf_[0] = '\0';
    dispatchZeroconf(uri, body, responseBuf_, sizeof(responseBuf_));
    server_.sendHeader("Access-Control-Allow-Origin", "*");
    server_.sendHeader("Access-Control-Allow-Private-Network", "true");
    server_.send(200, "application/json", responseBuf_);
  });
}

void LanZeroconfServer::startServer() {
  if (running_)
    return;

  char hostName[48];
  std::snprintf(hostName, sizeof(hostName), "iot_%s", activeDeviceId_.c_str());
  if (MDNS.begin(hostName)) {
    MDNS.addService("_iot", "_tcp", port_);
    MDNS.addServiceTxt("_iot", "_tcp", "txtvers", "1");
    MDNS.addServiceTxt("_iot", "_tcp", "id", activeDeviceId_.c_str());
    MDNS.addServiceTxt("_iot", "_tcp", "type", "Relay-Wifi");
    MDNS.addServiceTxt("_iot", "_tcp", "apivers", "1");
  }

  registerEndpoint("/zeroconf/switch");
  registerEndpoint("/zeroconf/switches");
  registerEndpoint("/zeroconf/startup");
  registerEndpoint("/zeroconf/pulse");
  registerEndpoint("/zeroconf/sled");
  registerEndpoint("/zeroconf/info");
  registerEndpoint("/zeroconf/signal_strength");
  registerEndpoint("/zeroconf/wifi");
  registerEndpoint("/zeroconf/config");
  registerEndpoint("/zeroconf/timers");
  registerEndpoint("/zeroconf/ota_unlock");

  server_.on("/zeroconf/info", HTTP_GET, [this]() {
    responseBuf_[0] = '\0';
    dispatchZeroconf("/zeroconf/info", String("{}"), responseBuf_,
                     sizeof(responseBuf_));
    server_.sendHeader("Access-Control-Allow-Origin", "*");
    server_.sendHeader("Access-Control-Allow-Private-Network", "true");
    server_.send(200, "application/json", responseBuf_);
  });

  server_.begin();
  udpRunning_ = udp_.begin(48899);
  running_ = true;
  notifyMdnsStateChange();
  Serial.printf("[LAN] Servidor HTTP Zeroconf iniciado en puerto %u + UDP "
                "48899 (mDNS: %s.local)\n",
                port_, hostName);
}

void LanZeroconfServer::stopServer() {
  if (!running_)
    return;
  if (udpRunning_) {
    udp_.stop();
    udpRunning_ = false;
  }
  server_.stop();
  MDNS.end();
  running_ = false;
}

void LanZeroconfServer::handleUdpDiscovery() {
  if (!udpRunning_)
    return;
  int pktLen = udp_.parsePacket();
  if (pktLen <= 0 || pktLen > 256)
    return;

  char rxBuf[260] = {0};
  int readBytes = udp_.read(rxBuf, sizeof(rxBuf) - 1);
  if (readBytes <= 0)
    return;
  rxBuf[readBytes] = '\0';

  if (std::strstr(rxBuf, "IOT_DISCOVER") == nullptr &&
      std::strstr(rxBuf, "discover") == nullptr) {
    return;
  }

  const bool wifiConn = (WiFi.status() == WL_CONNECTED);
  const bool mqttConn = isMQTTConnected();
  const char *cfgState = resolveDeviceConfigState(wifiConn, mqttConn);

  DynamicJsonDocument doc(768);
  const auto allStates = relays_.getAllStates();
  const bool firstOn = (!allStates.empty() && allStates.front().currentState);
  doc["device_id"] = activeDeviceId_.c_str();
  doc["chip_id"] = activeDeviceId_.c_str();
  doc["device_type"] = "Relay-Wifi";
  doc["model"] = RELAY_MODEL_NAME;
  doc["channel_count"] = allStates.empty() ? 1U : allStates.size();
  doc["state_version"] = relays_.getStateVersion();
  doc["switch"] = firstOn ? "on" : "off";
  JsonArray switchesArr = doc.createNestedArray("switches");
  for (size_t i = 0; i < allStates.size(); ++i) {
    JsonObject swObj = switchesArr.createNestedObject();
    swObj["outlet"] = static_cast<int>(i);
    swObj["switch"] = allStates[i].currentState ? "on" : "off";
  }
  doc["ip"] = WiFi.localIP().toString();
  doc["port"] = port_;
  doc["version"] = FIRMWARE_VERSION;
  doc["config_state"] = cfgState;
  doc["provisioning_mode"] = false;
  doc["factory_mode"] = !hasValidWifiCredentials();
  doc["has_wifi_credentials"] = hasValidWifiCredentials();
  doc["has_broker_credentials"] = hasValidBrokerCredentials();
  doc["wifi_connected"] = wifiConn;
  doc["mqtt_connected"] = mqttConn;
  doc["configured_ssid"] = config.wifi_ssid;
  doc["rssi"] = (int)WiFi.RSSI();
  JsonArray methods = doc.createNestedArray("methods");
  methods.add("ble");
  methods.add("softap");
  methods.add("smartconfig");
  methods.add("lan");

  responseBuf_[0] = '\0';
  size_t n = serializeJson(doc, responseBuf_, sizeof(responseBuf_));
  udp_.beginPacket(udp_.remoteIP(), udp_.remotePort());
  udp_.write(reinterpret_cast<const uint8_t *>(responseBuf_), n);
  udp_.endPacket();
}

void LanZeroconfServer::loop(bool wifiConnected) {
  if (!wifiConnected) {
    if (running_)
      stopServer();
    return;
  }
  if (!running_) {
    startServer();
  }
  server_.handleClient();
  handleUdpDiscovery();
}

void LanZeroconfServer::notifyMdnsStateChange() {
  if (!running_)
    return;

  ++sequence_;
  char seqBuf[16];
  std::snprintf(seqBuf, sizeof(seqBuf), "%u", sequence_);

  const auto *st = relays_.getState(relayDeviceId_);
  const bool relayOn = (st && st->currentState);

  char plainData[220];
  std::snprintf(plainData, sizeof(plainData),
                "{\"switch\":\"%s\",\"startup\":\"%s\",\"pulse\":\"%s\","
                "\"pulseWidth\":%u,\"sledOnline\":\"%s\"}",
                relayOn ? "on" : "off",
                domain::startupModeToString(relays_.getStartupMode()),
                relays_.isPulseEnabled() ? "on" : "off",
                relays_.getPulseWidthMs(),
                indicator_.isSledOnline() ? "on" : "off");

  MDNS.addServiceTxt("_iot", "_tcp", "seq", static_cast<const char *>(seqBuf));

  if (config.device_key.length() > 0) {
    uint8_t rawIv[16] = {0};
    std::snprintf(reinterpret_cast<char *>(rawIv), sizeof(rawIv), "%015u",
                  sequence_);
    char ivB64[32] = {0};
    app::CryptoUtils::base64Encode(rawIv, 16, ivB64, sizeof(ivB64));
    char dataB64[360] = {0};
    if (app::CryptoUtils::encryptZeroconfPayload(config.device_key.c_str(),
                                                 ivB64, plainData, dataB64,
                                                 sizeof(dataB64))) {
      MDNS.addServiceTxt("_iot", "_tcp", "encrypt", "true");
      MDNS.addServiceTxt("_iot", "_tcp", "iv",
                         static_cast<const char *>(ivB64));
      MDNS.addServiceTxt("_iot", "_tcp", "data1",
                         static_cast<const char *>(dataB64));
      return;
    }
  }

  MDNS.addServiceTxt("_iot", "_tcp", "data1",
                     static_cast<const char *>(plainData));
}

void LanZeroconfServer::dispatchZeroconf(const char *uri, const String &rawBody,
                                         char *respBuf, size_t respBufSize) {
  auto writeError = [&](int errCode) {
    std::snprintf(
        respBuf, respBufSize,
        "{\"sequence\":\"%u\",\"seq\":%u,\"state_version\":%u,\"error\":%d}",
        sequence_, sequence_, relays_.getStateVersion(), errCode);
  };

  if (rawBody.length() == 0 || rawBody.length() > 2560) {
    writeError(400);
    return;
  }

  DynamicJsonDocument outerDoc(2560);
  if (deserializeJson(outerDoc, rawBody) != DeserializationError::Ok) {
    writeError(400);
    return;
  }

  bool isEncrypted = outerDoc["encrypt"].as<bool>();
  decryptedJson_[0] = '\0';
  DynamicJsonDocument innerDoc(2048);
  JsonVariant dataObj;

  const std::string deviceKeyStr = config.device_key.c_str();

  if (isEncrypted) {
    const char *ivB64 = outerDoc["iv"];
    const char *dataB64 = outerDoc["data"];
    if (!ivB64 || !dataB64 || deviceKeyStr.empty()) {
      writeError(400);
      return;
    }
    if (!app::CryptoUtils::decryptZeroconfPayload(deviceKeyStr.c_str(), ivB64,
                                                  dataB64, decryptedJson_,
                                                  sizeof(decryptedJson_))) {
      writeError(400);
      return;
    }
    if (deserializeJson(innerDoc, decryptedJson_) != DeserializationError::Ok) {
      writeError(400);
      return;
    }
    dataObj = innerDoc.as<JsonVariant>();
  } else if (outerDoc["data"].is<JsonObject>()) {
    dataObj = outerDoc["data"];
  } else if (outerDoc["data"].is<const char *>()) {
    if (deserializeJson(innerDoc, outerDoc["data"].as<const char *>()) !=
        DeserializationError::Ok) {
      writeError(400);
      return;
    }
    dataObj = innerDoc.as<JsonVariant>();
  } else {
    dataObj = outerDoc.as<JsonVariant>();
  }

  const unsigned long now = millis();

  if (std::strcmp(uri, "/zeroconf/switch") == 0 ||
      std::strcmp(uri, "/zeroconf/switches") == 0) {
    const auto allStates = relays_.getAllStates();
    if (allStates.empty()) {
      writeError(500);
      return;
    }

    if (dataObj["switches"].is<JsonArray>()) {
      JsonArray arr = dataObj["switches"].as<JsonArray>();
      if (arr.size() == 0) {
        writeError(400);
        return;
      }
      bool anyApplied = false;
      std::string lastSw = "off";
      for (JsonVariant item : arr) {
        const char *itemSw = item["switch"];
        if (!itemSw)
          continue;
        const bool turnOn = (std::strcmp(itemSw, "on") == 0);
        const bool turnOff = (std::strcmp(itemSw, "off") == 0);
        if (!turnOn && !turnOff)
          continue;

        int outlet = 0;
        if (item.containsKey("outlet")) {
          outlet = item["outlet"].as<int>();
        } else if (item.containsKey("channel")) {
          int ch = item["channel"].as<int>();
          outlet = (ch > 0) ? (ch - 1) : 0;
        }
        if (outlet < 0 || static_cast<size_t>(outlet) >= allStates.size())
          continue;

        const std::string &targetId =
            allStates[static_cast<size_t>(outlet)].deviceId;
        relays_.setDesiredState(targetId, turnOn);
        relays_.applyDesiredState(targetId, now, "LAN_COMMAND");
        telemetry_.publishRelayState(targetId);
        lastSw = itemSw;
        anyApplied = true;
      }
      if (!anyApplied) {
        writeError(400);
        return;
      }
      telemetry_.publishStatus();
      telemetry_.publishEvent("LAN_ZEROCONF_SWITCH",
                              std::string("Cambio multicanal por HTTP LAN: ") +
                                  lastSw,
                              lastSw, "LAN_COMMAND");
      notifyMdnsStateChange();
      writeError(0);
      return;
    }

    const char *sw = dataObj["switch"];
    if (!sw) {
      writeError(400);
      return;
    }
    const bool turnOn = (std::strcmp(sw, "on") == 0);
    const bool turnOff = (std::strcmp(sw, "off") == 0);
    if (!turnOn && !turnOff) {
      writeError(400);
      return;
    }

    if (dataObj["all"].as<bool>()) {
      for (const auto &r : allStates) {
        relays_.setDesiredState(r.deviceId, turnOn);
        relays_.applyDesiredState(r.deviceId, now, "LAN_COMMAND");
      }
      telemetry_.publishAllRelayStates();
      telemetry_.publishStatus();
    } else {
      int outlet = 0;
      if (dataObj.containsKey("outlet")) {
        outlet = dataObj["outlet"].as<int>();
      } else if (dataObj.containsKey("channel")) {
        int ch = dataObj["channel"].as<int>();
        outlet = (ch > 0) ? (ch - 1) : 0;
      }
      if (outlet < 0 || static_cast<size_t>(outlet) >= allStates.size()) {
        outlet = 0;
      }
      const std::string &targetId =
          allStates[static_cast<size_t>(outlet)].deviceId;
      relays_.setDesiredState(targetId, turnOn);
      relays_.applyDesiredState(targetId, now, "LAN_COMMAND");
      telemetry_.publishRelayState(targetId);
      telemetry_.publishStatus();
    }
    telemetry_.publishEvent("LAN_ZEROCONF_SWITCH",
                            std::string("Cambio por HTTP LAN: ") + sw, sw,
                            "LAN_COMMAND");
    notifyMdnsStateChange();
    writeError(0);
    return;
  }

  if (std::strcmp(uri, "/zeroconf/startup") == 0) {
    const char *st = dataObj["startup"];
    if (!st) {
      writeError(400);
      return;
    }
    relays_.setStartupMode(domain::startupModeFromString(st));
    notifyMdnsStateChange();
    writeError(0);
    return;
  }

  if (std::strcmp(uri, "/zeroconf/pulse") == 0) {
    const char *pl = dataObj["pulse"];
    if (!pl) {
      writeError(400);
      return;
    }
    const bool en = (std::strcmp(pl, "on") == 0);
    const uint32_t widthMs = dataObj["pulseWidth"] | relays_.getPulseWidthMs();
    if (!relays_.configurePulse(en, widthMs, now)) {
      writeError(400);
      return;
    }
    notifyMdnsStateChange();
    writeError(0);
    return;
  }

  if (std::strcmp(uri, "/zeroconf/sled") == 0) {
    const char *sl = dataObj["sledOnline"];
    if (!sl) {
      writeError(400);
      return;
    }
    const bool en = (std::strcmp(sl, "on") == 0);
    indicator_.setSledOnline(en);
    configRepo_.saveSledOnline(en);
    notifyMdnsStateChange();
    writeError(0);
    return;
  }

  if (std::strcmp(uri, "/zeroconf/signal_strength") == 0) {
    ++sequence_;
    std::snprintf(respBuf, respBufSize,
                  "{\"sequence\":\"%u\",\"seq\":%u,\"error\":0,\"data\":{"
                  "\"signalStrength\":%d}}",
                  sequence_, sequence_, (int)WiFi.RSSI());
    return;
  }

  if (std::strcmp(uri, "/zeroconf/info") == 0) {
    ++sequence_;
    const auto allStates = relays_.getAllStates();
    const auto *st = relays_.getState(relayDeviceId_);
    const bool relayOn = (st && st->currentState);
    const bool wifiConn = (WiFi.status() == WL_CONNECTED);
    const bool mqttConn = isMQTTConnected();
    const char *cfgState = resolveDeviceConfigState(wifiConn, mqttConn);
    const String liveSsid = wifiConn ? WiFi.SSID() : config.wifi_ssid;
    const String localIp = wifiConn ? WiFi.localIP().toString() : String("");
    char lanProofHex[33] = {0};
    computeLanProofHex(deviceKeyStr, activeDeviceId_,
                       std::string(localIp.c_str()), lanProofHex);

    DynamicJsonDocument respDoc(2048);
    respDoc["sequence"] = String(sequence_);
    respDoc["seq"] = sequence_;
    respDoc["state_version"] = relays_.getStateVersion();
    respDoc["error"] = 0;
    JsonObject d = respDoc.createNestedObject("data");
    d["deviceid"] = activeDeviceId_.c_str();
    d["chipid"] = activeDeviceId_.c_str();
    d["device_type"] = "Relay-Wifi";
    d["model"] = RELAY_MODEL_NAME;
    d["channel_count"] = allStates.empty() ? 1U : allStates.size();
    d["state_version"] = relays_.getStateVersion();
    d["switch"] = relayOn ? "on" : "off";
    JsonArray switchesArr = d.createNestedArray("switches");
    for (size_t i = 0; i < allStates.size(); ++i) {
      JsonObject swItem = switchesArr.createNestedObject();
      swItem["outlet"] = static_cast<int>(i);
      swItem["switch"] = allStates[i].currentState ? "on" : "off";
    }
    d["startup"] = domain::startupModeToString(relays_.getStartupMode());
    d["pulse"] = relays_.isPulseEnabled() ? "on" : "off";
    d["pulseWidth"] = relays_.getPulseWidthMs();
    d["sledOnline"] = indicator_.isSledOnline() ? "on" : "off";
    d["fwVersion"] = FIRMWARE_VERSION;
    d["rssi"] = (int)WiFi.RSSI();
    d["config_state"] = cfgState;
    d["has_wifi_credentials"] = hasValidWifiCredentials();
    d["has_broker_credentials"] = hasValidBrokerCredentials();
    d["wifi_connected"] = wifiConn;
    d["mqtt_connected"] = mqttConn;
    d["ssid"] = liveSsid;
    d["configured_ssid"] = config.wifi_ssid;
    if (wifiConn) {
      d["bssid"] = WiFi.BSSIDstr();
      d["local_ip"] = localIp;
    }
    if (lanProofHex[0] != '\0') {
      d["lan_proof"] = lanProofHex;
    }
    JsonArray timersArr = d.createNestedArray("timers");
    app::TimerJsonCodec::serializeTimersToJsonArray(timerScheduler_, timersArr);
    serializeJson(respDoc, respBuf, respBufSize);
    return;
  }

  if (std::strcmp(uri, "/zeroconf/wifi") == 0 ||
      std::strcmp(uri, "/zeroconf/config") == 0) {
    domain::DeviceConfig devCfg;
    configRepo_.load(devCfg);
    const char *ssid = dataObj["ssid"] | dataObj["wifi_ssid"];
    if (std::strcmp(uri, "/zeroconf/wifi") == 0 &&
        (!ssid || std::strlen(ssid) == 0 || std::strlen(ssid) > 32)) {
      writeError(400);
      return;
    }
    if (ssid && std::strlen(ssid) > 0 && std::strlen(ssid) <= 32) {
      devCfg.wifiSsid = ssid;
      devCfg.wifiPass = dataObj["password"] | (dataObj["wifi_pass"] | "");
    }
    if (const char *brk = dataObj["mqtt_broker"] |
                          (dataObj["serverName"] | dataObj["server"])) {
      if (std::strlen(brk) > 0)
        devCfg.mqttBroker = brk;
    }
    if (dataObj.containsKey("mqtt_port") || dataObj.containsKey("port")) {
      int p = dataObj["mqtt_port"] | (dataObj["port"] | 0);
      if (p > 0 && p <= 65535)
        devCfg.mqttPort = p;
    }
    if (const char *usr =
            dataObj["mqtt_user"] | (dataObj["apikey"] | dataObj["account"])) {
      if (std::strlen(usr) > 0)
        devCfg.mqttUser = usr;
    }
    if (const char *mp = dataObj["mqtt_pass"]) {
      devCfg.mqttPass = mp;
    }
    if (const char *dk = dataObj["devicekey"] | dataObj["device_key"]) {
      if (std::strlen(dk) > 0)
        devCfg.deviceKey = dk;
    }
    if (const char *tok = dataObj["bind_token"] | dataObj["token"]) {
      devCfg.bindToken = tok;
    }
    configRepo_.save(devCfg);
    writeError(0);
    if (onCredentialsReload_) {
      onCredentialsReload_();
    }
    return;
  }

  if (std::strcmp(uri, "/zeroconf/timers") == 0) {
    if (!dataObj["timers"].is<JsonArray>()) {
      writeError(400);
      return;
    }
    const uint32_t nowEpoch = static_cast<uint32_t>(std::time(nullptr));
    app::TimerJsonCodec::applyTimersFromJsonArray(
        dataObj["timers"].as<JsonArrayConst>(), timerScheduler_,
        nowEpoch > 1000000000U ? nowEpoch : 0U);
    if (isMQTTConnected()) {
      telemetry_.publishTimersEvent(
          "TIMERS_UPDATED",
          "Temporizadores sincronizados por LAN: " +
              std::to_string(timerScheduler_.getActiveTimerCount()) +
              " activos",
          timerScheduler_);
    }
    writeError(0);
    return;
  }

  if (std::strcmp(uri, "/zeroconf/ota_unlock") == 0) {
    otaUnlocked_ = true;
    writeError(0);
    return;
  }

  writeError(404);
}

} // namespace relay::infra
