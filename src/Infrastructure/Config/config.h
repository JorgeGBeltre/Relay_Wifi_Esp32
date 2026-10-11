#ifndef CONFIG_H
#define CONFIG_H

#include <cstddef>
#include <cstdint>

#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
  #define DEBUG_ESP_PORT Serial
  #define NODEBUG_WEBSOCKETS
  #define NDEBUG
#endif

const unsigned long WIFI_CONNECT_TIMEOUT = 5000;

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = -4 * 3600;
const int daylightOffset_sec = 0;

const char* MQTT_SERVER = "YOUR_MQTT_SERVER_HERE";
const int MQTT_PORT = 8883;
const char* API_KEY = "YOUR_API_KEY_HERE";
const unsigned long MQTT_RECONNECT_INTERVAL = 10000;
const int MQTT_SOCKET_TIMEOUT = 30;
const size_t MQTT_BUFFER_SIZE = 4096;
const unsigned long MQTT_KEEP_ALIVE = 60;

const char* root_ca_pem = \
"-----BEGIN CERTIFICATE-----\n" \
"YOUR_SSL_CERTIFICATE_HERE\n" \
"-----END CERTIFICATE-----\n";

constexpr uint8_t RelayPin1 = 4;
constexpr uint8_t BUTTON_PIN1 = 0;
constexpr uint8_t wifiLed = 2;
constexpr uint8_t DEBOUNCE_TIME = 50;
constexpr uint16_t BUTTON_LONG_PRESS_TIME = 3000;

const unsigned long STATUS_PUBLISH_INTERVAL = 3600000;
const unsigned long TIME_SYNC_INTERVAL = 3600000;
const int FAST_BLINK_INTERVAL = 100;
const int SLOW_BLINK_INTERVAL = 400;

const char* FIRMWARE_VERSION = "v0.0.7";

#endif
