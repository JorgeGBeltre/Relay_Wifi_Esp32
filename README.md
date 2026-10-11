# Relay-Wifi

**Enterprise-Grade Smart Relay Controller for ESP32 with Clean Architecture, Native MQTT 5.0, and Secure Chunked OTA**

[![Platform](https://img.shields.io/badge/platform-ESP32-blue)](https://www.espressif.com/en/products/socs/esp32)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-green)](https://en.cppreference.com/w/cpp/17)
[![MQTT 5.0](https://img.shields.io/badge/MQTT-5.0-orange)](https://mqtt.org)
[![Architecture](https://img.shields.io/badge/Architecture-Clean%20Architecture-purple)](#architecture)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-teal)](https://platformio.org)
[![CMake](https://img.shields.io/badge/CMake-Supported-red)](https://cmake.org)

---

**Relay-Wifi** is a production-ready, highly reliable firmware for ESP32 smart relay and IoT actuator devices. Engineered following strict **Clean Architecture (Hexagonal / Onion Architecture)** principles, the project completely decouples core business and domain rules from external drivers, frameworks, and hardware peripherals.

The system natively integrates **[ESP32_MQTTv5](https://github.com/JorgeGBeltre/ESP32_MQTTv5)** for secure, session-aware MQTT 5.0 telemetry and command handling, as well as **[MQTTOTAv5](https://github.com/JorgeGBeltre/MQTTOTAv5)** for secure, chunked Over-The-Air firmware updates with SHA-256 integrity verification, rollback protection, and non-blocking background downloads.

---

## Table of Contents

- [Features](#features)
- [Architecture](#architecture)
  - [Layer Separation](#layer-separation)
  - [Directory Tree](#directory-tree)
- [Hardware Specifications](#hardware-specifications)
- [Dependencies](#dependencies)
- [Configuration & Placeholders](#configuration--placeholders)
- [MQTT 5.0 Messaging & Topics](#mqtt-50-messaging--topics)
  - [Topic Topology](#topic-topology)
  - [Command Handling](#command-handling)
  - [JSON Telemetry & Status Payloads](#json-telemetry--status-payloads)
- [Over-The-Air (OTA) Updates](#over-the-air-ota-updates)
- [Physical Controls & SoftAP Provisioning](#physical-controls--softap-provisioning)
- [Status Indicator (LED)](#status-indicator-led)
- [Build & Flashing Instructions](#build--flashing-instructions)
  - [Using PlatformIO](#using-platformio)
  - [Dual OTA Partition Scheme](#dual-ota-partition-scheme)
  - [Host Compilation with CMake](#host-compilation-with-cmake)
- [License](#-license)
- [Contact](#contact)
- [Support](#support)

---

## Features

| Feature | Description |
|---|---|
| **Clean Architecture** | Pure Domain (zero-dependency C++17 entities/interfaces), decoupled Application services, isolated Infrastructure drivers, and unified Presentation composition root |
| **Native MQTT 5.0** | Powered by `ESP32_MQTTv5` — TLS encryption (`WiFiClientSecure`), keep-alive, sessions, properties, and JSON Last Will & Testament (LWT) |
| **Secure OTA Updates** | Powered by `MQTTOTAv5` — chunked binary transfers over MQTT 5.0, SHA-256 hash checks, automatic rollback protection, and non-blocking restarts |
| **Dual Control Flow** | Physical button support (debounced short press for relay toggle) and remote cloud actuation (state synchronization, get/set commands) |
| **SoftAP WiFi Provisioning** | 3-second long press on the hardware button launches an on-demand Access Point (`VESIS-<ChipID>`) powered by `WiFiManager` |
| **Non-blocking LED** | Status indicator LED driven by non-blocking timer loops (Fast Blink: Disconnected, Slow Blink: Connecting, Solid ON: Operational) |
| **NTP Synchronization** | Accurate UTC timestamping (ISO 8601 `YYYY-MM-DDTHH:MM:SS.mmmZ`) synchronized with public NTP pools (`pool.ntp.org`, `time.nist.gov`) |
| **Dual Build Toolchain** | Full target firmware builds via PlatformIO (`platformio.ini`) and pure host test compilation via CMake (`CMakeLists.txt`) |

---

## Architecture

The project adheres to Robert C. Martin's Clean Architecture pattern, mirroring the design conventions of enterprise ESP32 systems like `GLP_Presure`.

### Layer Separation

1. **Domain Layer (`src/Domain/`)**:
   - Contains pure C++17 entities (`RelayState`, `DeviceIdentity`, `SystemMetrics`, `DeviceEvent`) and abstract interfaces (`IRelayController`, `IButtonHandler`, `ITelemetryTransport`, `IOtaService`, `INetworkLink`, `IIndicator`, `ILogger`, `IClock`, `ITimeProvider`, `IDeviceRepository`, `ISystemControl`).
   - Contains **zero** external library or Arduino/ESP-IDF dependencies.
2. **Application Layer (`src/Application/`)**:
   - Implements use cases and business orchestration:
     - `RelayFirmwareController`: finite state machine managing system transitions (`Connecting` → `Operational`), button events, and command routing.
     - `RelayService`: manages multiple physical and virtual relay states, desired/current state delta matching, and toggles.
     - `ConnectivityService`: coordinates network links (WiFi) and MQTT 5.0 transports.
     - `TelemetryService`: formats and dispatches periodic status, event envelopes, and switch states.
     - `MqttTopics`: pure string topic generation based on device identity.
     - `IPayloadSerializer`: abstract data transfer interface.
3. **Infrastructure Layer (`src/Infrastructure/`)**:
   - Implements concrete domain interfaces using hardware drivers and external libraries:
     - `Actuators`: `GpioRelay` (GPIO pin control).
     - `Button`: `GpioButton` (non-blocking debouncing and long press detector).
     - `Config`: `config.h` (credentials and timing) and `BoardConfig.h` (pin mapping).
     - `Indicator`: `GpioIndicator` (non-blocking status LED).
     - `Logging`: `SerialLogger` (serial logging output).
     - `Messaging`: `mqtt_client` and `Mqttv5Transport` (wrapping `ESP32_MQTTv5`).
     - `Net`: `WifiLink` and `NetworkClient` (WiFiClientSecure with TLS).
     - `Ota`: `ota_manager` and `Mqttv5OtaService` (wrapping `MQTTOTAv5`).
     - `Provisioning`: `WifiApProvisioningPortal` (WiFiManager portal).
     - `Serialization`: `ArduinoJsonPayloadSerializer` (ArduinoJson v6 schema formatter).
     - `System`: `EspDeviceRepository` (eFuse MAC, chip model, heap) and `EspSystemControl` (reboot).
     - `Time`: `ArduinoClock` and `NtpTimeProvider` (SNTP + ISO 8601 formatting).
4. **Presentation Layer (`src/Presentation/`)**:
   - `main.cpp`: Composition Root. Instantiates concrete infrastructure adapters, injects dependencies into application services, registers button handlers and callbacks, and exposes standard `setup()` and `loop()` entry points.

### Directory Tree

```
Relay-Wifi-V0.0.5/
├── CMakeLists.txt
├── platformio.ini
├── partitions.csv
├── .gitignore
├── README.md
└── src/
    ├── Domain/
    │   ├── Entities/
    │   │   ├── DeviceEvent.h
    │   │   ├── DeviceIdentity.h
    │   │   ├── RelayState.h
    │   │   └── SystemMetrics.h
    │   └── Interfaces/
    │       ├── IButtonHandler.h
    │       ├── IClock.h
    │       ├── IDeviceRepository.h
    │       ├── IIndicator.h
    │       ├── ILogger.h
    │       ├── INetworkLink.h
    │       ├── IOtaService.h
    │       ├── IRelayController.h
    │       ├── ISystemControl.h
    │       ├── ITelemetryTransport.h
    │       └── ITimeProvider.h
    ├── Application/
    │   ├── Controllers/
    │   │   ├── RelayFirmwareController.h
    │   │   └── RelayFirmwareController.cpp
    │   ├── Dto/
    │   │   └── DeviceCommand.h
    │   ├── Messaging/
    │   │   └── MqttTopics.h
    │   ├── Serialization/
    │   │   └── IPayloadSerializer.h
    │   └── Services/
    │       ├── ConnectivityService.h
    │       ├── ConnectivityService.cpp
    │       ├── RelayService.h
    │       ├── RelayService.cpp
    │       ├── TelemetryService.h
    │       └── TelemetryService.cpp
    ├── Infrastructure/
    │   ├── Actuators/
    │   │   ├── GpioRelay.h
    │   │   └── GpioRelay.cpp
    │   ├── Button/
    │   │   ├── GpioButton.h
    │   │   └── GpioButton.cpp
    │   ├── Config/
    │   │   ├── BoardConfig.h
    │   │   └── config.h
    │   ├── Indicator/
    │   │   └── GpioIndicator.h
    │   ├── Logging/
    │   │   └── SerialLogger.h
    │   ├── Messaging/
    │   │   ├── mqtt_client.h
    │   │   ├── mqtt_client.cpp
    │   │   ├── Mqttv5Transport.h
    │   │   └── Mqttv5Transport.cpp
    │   ├── Net/
    │   │   ├── NetworkClient.h
    │   │   ├── NetworkClient.cpp
    │   │   ├── WifiLink.h
    │   │   └── WifiLink.cpp
    │   ├── Ota/
    │   │   ├── Mqttv5OtaService.h
    │   │   ├── ota_manager.h
    │   │   └── ota_manager.cpp
    │   ├── Provisioning/
    │   │   └── WifiApProvisioningPortal.h
    │   ├── Serialization/
    │   │   ├── ArduinoJsonPayloadSerializer.h
    │   │   └── ArduinoJsonPayloadSerializer.cpp
    │   ├── System/
    │   │   ├── EspDeviceRepository.h
    │   │   ├── EspDeviceRepository.cpp
    │   │   └── EspSystemControl.h
    │   └── Time/
    │       ├── ArduinoClock.h
    │       ├── NtpTimeProvider.h
    │       └── NtpTimeProvider.cpp
    └── Presentation/
        └── main.cpp
```

---

## Hardware Specifications

Default board wiring configured in `src/Infrastructure/Config/BoardConfig.h`:

| Peripheral | GPIO Pin | Configuration | Function |
|---|---|---|---|
| **Relay 1** | `GPIO 4` | Output (Active High) | Primary relay output actuator |
| **Button 1** | `GPIO 0` | Input (`INPUT_PULLUP`) | Physical toggle (<3s) / SoftAP portal (>3s) |
| **Status LED** | `GPIO 2` | Output (Active High) | WiFi and connection status indicator |

---

## Dependencies

The project relies on external libraries linked via PlatformIO:

- **[ESP32_MQTTv5](https://github.com/JorgeGBeltre/ESP32_MQTTv5)** (`^1.0.3`): Native MQTT 5.0 client library for ESP32.
- **[MQTTOTAv5](https://github.com/JorgeGBeltre/MQTTOTAv5)** (`^1.0.2`): Over-The-Air firmware updater over MQTT 5.0.
- **[ArduinoJson](https://arduinojson.org/)** (`^6.21.0`): Efficient JSON serialization and parsing.
- **[WiFiManager](https://github.com/tzapu/WiFiManager)** (`^2.0.17`): SoftAP web portal for runtime WiFi credentials provisioning.
- **Arduino-ESP32 Core** (`>= 2.0.0`): Official Espressif framework for ESP32.

---

## Configuration & Placeholders

All device credentials and endpoint settings are organized in `src/Infrastructure/Config/config.h`. Set your actual parameters before deployment:

```cpp
// src/Infrastructure/Config/config.h

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
```

> **Note**: If `root_ca_pem` contains the placeholder string or is left empty, the underlying network client automatically switches to `setInsecure()` mode, allowing testing with self-signed certificates.

---

## MQTT 5.0 Messaging & Topics

### Topic Topology

Topics are derived dynamically at runtime using the chip's unique eFuse MAC address:

| Topic | Direction | Description |
|---|---|---|
| `status` | Outbound | Periodic or on-demand full JSON state report; also used as MQTT 5.0 LWT offline topic |
| `events` | Outbound | System events (`MQTT_CONNECTED`, `BUTTON_PRESS`, `DEVICE_SET_COMMAND`, etc.) |
| `telemetry` | Outbound | Periodic health metrics snapshot |
| `commands` | Inbound | Global control commands (`reset`, `status`, `toggle_all`, `sync_states`, `switch1:...`) |
| `event/<ChipID>` | Inbound | Device-targeted event channel |
| `device_switch1/state` | Inbound | Turn relay on/off/toggle via MQTT payload (`on`, `off`, `toggle`) |
| `device_switch1/command` | Inbound | Alternate command topic |
| `device_switch1_state` | Inbound/Outbound | State sync: inbound desired state from cloud; outbound current confirmed state |
| `device_switch1/get` | Outbound | State query published on connection to request cloud synchronization |
| `ota` | Inbound | Handled automatically by `MQTTOTAv5` |

### Command Handling

Sending strings to the `commands` topic triggers the following actions:

- `"reset"`: Triggers a clean system restart via `ESP.restart()`.
- `"status"`: Forces immediate publication of the full device status JSON.
- `"toggle_all"`: Inverts all active relays and publishes their state updates.
- `"sync_states"`: Queries the server for current cloud desired states.
- `"switch1:on"`, `"switch1:off"`, `"switch1:toggle"`: Directly modifies Relay 1.

### JSON Telemetry & Status Payloads

Status publication to `status`:

```json
{
  "Device": {
    "status": "connected",
    "ChipId": "0000A1B2C3D4",
    "MacAddress": "24:6F:28:XX:XX:XX",
    "ChipType": "ESP32-D0WDQ6",
    "FirmwareVersion": "v0.0.7"
  },
  "Timestamp": "2026-10-10T20:30:00.000Z",
  "Details": {
    "chipId": "0000A1B2C3D4",
    "sync_completed": true,
    "devices_count": 1,
    "wifi_status": "connected",
    "rssi": -58,
    "free_heap": 182400,
    "uptime": 145020,
    "devices": {
      "0000A1B2C3D4-01": {
        "state": "on",
        "desired_state": "on",
        "pin": 4
      }
    }
  }
}
```

---

## Over-The-Air (OTA) Updates

Over-The-Air updates are fully handled by **`MQTTOTAv5`**:

1. **Initialization**: Hooked directly inside `mqtt.onConnect()` in `mqtt_client.cpp`, subscribing the device to OTA update streams.
2. **Chunked Stream**: Firmware binaries are delivered in discrete chunks over MQTT 5.0, directly written into the standby OTA partition (`app0` or `app1`).
3. **Integrity & Security**: Verifies the cumulative SHA-256 hash against metadata transmitted in MQTT 5.0 User Properties.
4. **Safety & Rollback**: Validates image headers before writing; registers rollback protection via `esp_ota_ops.h` so the board reverts automatically if a new build fails to boot.
5. **Non-blocking Loop**: Managed via `ota.handle()` inside `loop()` without blocking relay operations.

---

## Physical Controls & SoftAP Provisioning

The onboard push button on **GPIO 0** provides dual functionality via `GpioButton`:

- **Short Press (< 3000 ms)**: Toggles Relay 1 locally, emits an immediate `"BUTTON_PRESS"` event JSON to `events`, and publishes the state string (`"on"`/`"off"`) to `device_switch1_state`.
- **Long Press (> 3000 ms)**: Bypasses normal operation and launches a local SoftAP web portal:
  - **SSID**: `VESIS-<ChipID>`
  - **Password**: `12345678`
  - **Timeout**: 180 seconds
  - Users can connect their smartphone or laptop to configure new WiFi credentials. Once configured (or on timeout), the ESP32 automatically reboots.

---

## Status Indicator (LED)

The status LED on **GPIO 2** provides clear visual diagnostics without blocking the processor:

- **Fast Blink (100 ms interval)**: Disconnected from WiFi / searching for network.
- **Slow Blink (400 ms interval)**: WiFi connected, attempting connection to MQTT 5.0 broker.
- **Solid ON**: Fully operational (WiFi and MQTT 5.0 connected).

---

## Build & Flashing Instructions

### Using PlatformIO

Make sure [PlatformIO Core](https://platformio.org/install/cli) is installed.

1. **Compile Firmware**:
   ```bash
   pio run -e esp32dev
   ```

2. **Upload to Hardware**:
   ```bash
   pio run -e esp32dev -t upload
   ```

3. **Monitor Serial Output**:
   ```bash
   pio run -e esp32dev -t monitor
   ```

4. **Targeting ESP32-S3**:
   ```bash
   pio run -e esp32-s3-devkitc-1 -t upload
   ```

### Dual OTA Partition Scheme

The firmware relies on `partitions.csv` configured for two 1.9 MB application slots:

```csv
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xe000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x1E0000,
app1,     app,  ota_1,   0x1F0000, 0x1E0000,
spiffs,   data, spiffs,  0x3D0000, 0x30000,
```

### Host Compilation with CMake

The Domain and Application layers can be compiled on your host PC (Linux, macOS, or Windows) without requiring an ESP32 toolchain or Arduino headers:

```bash
mkdir build && cd build
cmake .. -DHOST_BUILD=ON
cmake --build .
```

---

---

##  Broker Compatibility

Tested with:
- [Mosquitto](https://mosquitto.org/) 2.0+ (with MQTT 5.0 enabled)
- [EMQX](https://www.emqx.com/) 5.0+
- [HiveMQ Cloud](https://www.hivemq.com/) (MQTT 5.0 plan)
- [AWS IoT Core](https://aws.amazon.com/iot-core/) (via custom authentication)

If you encounter issues, please open an issue with broker version and configuration.

---

##  License

This library is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.

## Contact

Author: **Jorge Gaspar Beltre Rivera**  
Project: Relay_Wifi_ep32

<p align="center">
  <a href="https://www.linkedin.com/in/jorge-gaspar-beltre-rivera/" target="_blank"><img src="https://user-images.githubusercontent.com/74038190/235294012-0a55e343-37ad-4b0f-924f-c8431d9d2483.gif" alt="LinkedIn" width="100"></a>
  <a href="https://github.com/JorgeGBeltre" target="_blank"><img src="https://user-images.githubusercontent.com/74038190/212257468-1e9a91f1-b626-4baa-b15d-5c385dfa7ed2.gif" alt="GitHub" width="100"></a>
  <a href="mailto:Jorgegaspar3021@gmail.com"><img src="https://user-images.githubusercontent.com/74038190/216122065-2f028bae-25d6-4a3c-bc9f-175394ed5011.png" alt="E-Mail" width="100"></a>

</p>

## Support

This project is developed independently. Even a small contribution helps me dedicate more time to development, testing, and releasing new features.


 <p align="center">
  <a href="https://www.paypal.com/donate/?hosted_button_id=2VLA8BWT967LU">
    <img src="https://www.paypalobjects.com/webstatic/icon/pp258.png"
         alt="Donate with PayPal"
         height="60">
  </a>
</p>