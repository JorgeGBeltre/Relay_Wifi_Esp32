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

constexpr unsigned long WIFI_CONNECT_TIMEOUT = 5000;

constexpr const char* ntpServer1 = "pool.ntp.org";
constexpr const char* ntpServer2 = "time.nist.gov";
constexpr long gmtOffset_sec = -4 * 3600;
constexpr int daylightOffset_sec = 0;

constexpr const char* MQTT_SERVER = "YOUR_MQTT_SERVER_HERE";
constexpr int MQTT_PORT = 8883;
constexpr const char* API_KEY = "YOUR_API_KEY_HERE";
constexpr unsigned long MQTT_RECONNECT_INTERVAL = 10000;
constexpr int MQTT_SOCKET_TIMEOUT = 30;
constexpr size_t MQTT_BUFFER_SIZE = 4096;
constexpr unsigned long MQTT_KEEP_ALIVE = 60;

inline const char* root_ca_pem = \
"-----BEGIN CERTIFICATE-----\n" \
"YOUR_SSL_CERTIFICATE_HERE\n" \
"-----END CERTIFICATE-----\n";

#ifndef RELAY_CHANNEL_COUNT
  #define RELAY_CHANNEL_COUNT 1
#endif

constexpr uint8_t RelayPin1 = 4;
constexpr uint8_t RelayPin2 = 16;
constexpr uint8_t RelayPin3 = 17;
constexpr uint8_t RelayPin4 = 18;
constexpr uint8_t BUTTON_PIN1 = 0;
constexpr uint8_t BUTTON_PIN2 = 32;
constexpr uint8_t BUTTON_PIN3 = 33;
constexpr uint8_t BUTTON_PIN4 = 25;
constexpr uint8_t wifiLed = 2;

constexpr const char* RELAY_MODEL_NAME =
    (RELAY_CHANNEL_COUNT == 4) ? "Relay-Wifi-4CH" :
    (RELAY_CHANNEL_COUNT == 3) ? "Relay-Wifi-3CH" :
    (RELAY_CHANNEL_COUNT == 2) ? "Relay-Wifi-2CH" : "Relay-Wifi";
constexpr uint8_t DEBOUNCE_TIME = 50;             // 2 ticks * 25 ms = 50 ms
constexpr uint16_t BUTTON_LONG_PRESS_TIME = 5000; // 200 ticks * 25 ms = 5000 ms

constexpr unsigned long STATUS_PUBLISH_INTERVAL = 3600000;
constexpr unsigned long TIME_SYNC_INTERVAL = 3600000;
constexpr int FAST_BLINK_INTERVAL = 100;
constexpr int SLOW_BLINK_INTERVAL = 400;

constexpr uint16_t LAN_HTTP_PORT = 8081;
constexpr const char* FIRMWARE_VERSION = "v0.0.8";

#endif
