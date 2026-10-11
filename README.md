# Relay-Wifi

**Enterprise-Grade Smart Relay Controller for ESP32 with Clean Architecture, Native MQTT 5.0, Bluetooth Low Energy (BLE) Provisioning, and Secure Chunked OTA**

[![Platform](https://img.shields.io/badge/platform-ESP32-blue)](https://www.espressif.com/en/products/socs/esp32)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-green)](https://en.cppreference.com/w/cpp/17)
[![MQTT 5.0](https://img.shields.io/badge/MQTT-5.0-orange)](https://mqtt.org)
[![BLE](https://img.shields.io/badge/Bluetooth-BLE%20GATT-blueviolet)](https://github.com/h2zero/NimBLE-Arduino)
[![Architecture](https://img.shields.io/badge/Architecture-Clean%20Architecture-purple)](#architecture)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-teal)](https://platformio.org)
[![CMake](https://img.shields.io/badge/CMake-Supported-red)](https://cmake.org)

---

**Relay-Wifi** is a production-ready, highly reliable firmware for ESP32 smart relay and IoT actuator devices. Engineered following strict **Clean Architecture (Hexagonal / Onion Architecture)** principles, the project completely decouples core business and domain rules from external drivers, frameworks, and hardware peripherals.

The system natively integrates **[ESP32_MQTTv5](https://github.com/JorgeGBeltre/ESP32_MQTTv5)** for secure, session-aware MQTT 5.0 telemetry and command handling, **[MQTTOTAv5](https://github.com/JorgeGBeltre/MQTTOTAv5)** for chunked Over-The-Air updates with SHA-256 integrity verification, and **Bluetooth Low Energy (BLE) Provisioning** powered by **[NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)** for frictionless onboarding via mobile apps.

---

## Table of Contents

- [Features](#features)
- [Architecture](#architecture)
  - [Layer Separation](#layer-separation)
  - [Directory Tree](#directory-tree)
- [Hardware Specifications](#hardware-specifications)
- [Dependencies](#dependencies)
- [Configuration & Placeholders](#configuration--placeholders)
- [Bluetooth BLE Provisioning & Mobile App Integration](#bluetooth-ble-provisioning--mobile-app-integration)
  - [Overview & Workflow](#overview--workflow)
  - [GATT Service & Characteristics Layout](#gatt-service--characteristics-layout)
  - [Sonoff-Style Account Binding & Mutual Identification](#sonoff-style-account-binding--mutual-identification)
  - [Standard Bluetooth Device Information Service (0x180A)](#standard-bluetooth-device-information-service-0x180a)
  - [Provisioning via Individual Characteristics](#provisioning-via-individual-characteristics)
  - [Provisioning via Unified Data Frames (BLE Frames)](#provisioning-via-unified-data-frames-ble-frames)
  - [Device Identification to the Mobile App](#device-identification-to-the-mobile-app)
  - [Dual Provisioning Flow & Fallback Architecture](#dual-provisioning-flow--fallback-architecture)
- [MQTT 5.0 Messaging & Topics](#mqtt-50-messaging--topics)
  - [Topic Topology](#topic-topology)
  - [Command Handling](#command-handling)
  - [JSON Telemetry & Status Payloads](#json-telemetry--status-payloads)
- [Over-The-Air (OTA) Updates](#over-the-air-ota-updates)
- [Physical Controls & Dual Provisioning Portals](#physical-controls--dual-provisioning-portals)
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
| **Bluetooth (BLE) Provisioning** | Powered by `NimBLE-Arduino` (protocol schema matching `GLP_Presure`) — runtime provisioning of WiFi & MQTT credentials via mobile app frames or individual GATT characteristics, with device self-identification |
| **Dual Control Flow** | Physical button support (debounced short press for relay toggle) and remote cloud actuation (state synchronization, get/set commands) |
| **Dual Provisioning Portals** | Primary BLE Provisioning with automatic fallback to SoftAP Web Portal (`WiFiManager` / `VESIS-<ChipID>`) on timeout or button long-press |
| **NVS Persistent Storage** | Dynamic credentials and IDs persisted non-volatilely via `Preferences` in the `relay-config` namespace |
| **Non-blocking LED** | Status indicator LED driven by non-blocking timer loops (Fast Blink: Disconnected/Provisioning, Slow Blink: Connecting, Solid ON: Operational) |
| **NTP Synchronization** | Accurate UTC timestamping (ISO 8601 `YYYY-MM-DDTHH:MM:SS.mmmZ`) synchronized with public NTP pools (`pool.ntp.org`, `time.nist.gov`) |
| **Dual Build Toolchain** | Full target firmware builds via PlatformIO (`platformio.ini`) and pure host test compilation via CMake (`CMakeLists.txt`) |

---

## Architecture

The project adheres to Robert C. Martin's Clean Architecture pattern, mirroring the design conventions of enterprise ESP32 systems like `GLP_Presure`.

### Layer Separation

1. **Domain Layer (`src/Domain/`)**:
   - Contains pure C++17 entities (`RelayState`, `DeviceIdentity`, `SystemMetrics`, `DeviceEvent`, `DeviceConfig`) and abstract interfaces (`IRelayController`, `IButtonHandler`, `ITelemetryTransport`, `IOtaService`, `INetworkLink`, `IIndicator`, `ILogger`, `IClock`, `ITimeProvider`, `IDeviceRepository`, `ISystemControl`, `IConfigRepository`, `IProvisioningPortal`).
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
     - `Config`: `config.h` (credentials and timing), `BoardConfig.h` (pin mapping), `nvs_config.h`/`cpp` (Preferences NVS persistence), and `NvsConfigRepository.h` (`IConfigRepository` adapter).
     - `Indicator`: `GpioIndicator` (non-blocking status LED).
     - `Logging`: `SerialLogger` (serial logging output).
     - `Messaging`: `mqtt_client` and `Mqttv5Transport` (wrapping `ESP32_MQTTv5`).
     - `Net`: `WifiLink` and `NetworkClient` (WiFiClientSecure with TLS and dynamic credentials).
     - `Ota`: `ota_manager` and `Mqttv5OtaService` (wrapping `MQTTOTAv5`).
     - `Provisioning`: `ble_provisioning.h`/`cpp` (NimBLE GATT server & frame parser), `ProvisioningPortals.h`, and `WifiApProvisioningPortal.h` (`WiFiManager` portal).
     - `Serialization`: `ArduinoJsonPayloadSerializer` (ArduinoJson v6 schema formatter).
     - `System`: `EspDeviceRepository` (eFuse MAC, chip model, heap) and `EspSystemControl` (reboot).
     - `Time`: `ArduinoClock` and `NtpTimeProvider` (SNTP + ISO 8601 formatting).
4. **Presentation Layer (`src/Presentation/`)**:
   - `main.cpp`: Composition Root. Instantiates concrete infrastructure adapters, injects dependencies into application services, registers button handlers and callbacks, and exposes standard `setup()` and `loop()` entry points.

### Directory Tree

```
Relay-Wifi/
├── CMakeLists.txt
├── platformio.ini
├── partitions.csv
├── .gitignore
├── README.md
└── src/
    ├── Domain/
    │   ├── Entities/
    │   │   ├── DeviceConfig.h
    │   │   ├── DeviceEvent.h
    │   │   ├── DeviceIdentity.h
    │   │   ├── RelayState.h
    │   │   └── SystemMetrics.h
    │   └── Interfaces/
    │       ├── IButtonHandler.h
    │       ├── IClock.h
    │       ├── IConfigRepository.h
    │       ├── IDeviceRepository.h
    │       ├── IIndicator.h
    │       ├── ILogger.h
    │       ├── INetworkLink.h
    │       ├── IOtaService.h
    │       ├── IProvisioningPortal.h
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
    │   │   ├── config.h
    │   │   ├── nvs_config.h
    │   │   ├── nvs_config.cpp
    │   │   └── NvsConfigRepository.h
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
    │   │   ├── ble_provisioning.h
    │   │   ├── ble_provisioning.cpp
    │   │   ├── ProvisioningPortals.h
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
| **Button 1** | `GPIO 0` | Input (`INPUT_PULLUP`) | Physical toggle (<3s) / BLE & SoftAP portal (>3s) |
| **Status LED** | `GPIO 2` | Output (Active High) | WiFi, BLE, and connection status indicator |

---

## Dependencies

The project relies on external libraries linked via PlatformIO:

- **[ESP32_MQTTv5](https://github.com/JorgeGBeltre/ESP32_MQTTv5)** (`^1.0.3`): Native MQTT 5.0 client library for ESP32.
- **[MQTTOTAv5](https://github.com/JorgeGBeltre/MQTTOTAv5)** (`^1.0.2`): Over-The-Air firmware updater over MQTT 5.0.
- **[NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)** (`^1.4.1`): High-performance, lightweight Bluetooth Low Energy stack.
- **[ArduinoJson](https://arduinojson.org/)** (`^6.21.0`): Efficient JSON serialization and parsing.
- **[WiFiManager](https://github.com/tzapu/WiFiManager)** (`^2.0.17`): SoftAP web portal for runtime WiFi credentials provisioning.
- **Arduino-ESP32 Core** (`>= 2.0.0`): Official Espressif framework for ESP32.

---

## Configuration & Placeholders

Default fallback credentials and endpoint settings are organized in `src/Infrastructure/Config/config.h`:

```cpp
// src/Infrastructure/Config/config.h

constexpr const char* MQTT_SERVER = "YOUR_MQTT_SERVER_HERE";
constexpr int MQTT_PORT = 8883;
constexpr const char* API_KEY = "YOUR_API_KEY_HERE";
constexpr unsigned long MQTT_RECONNECT_INTERVAL = 10000;
constexpr int MQTT_SOCKET_TIMEOUT = 30;
constexpr size_t MQTT_BUFFER_SIZE = 4096;
constexpr unsigned long MQTT_KEEP_ALIVE = 60;

inline const char* root_ca_pem = "-----BEGIN CERTIFICATE-----
" "YOUR_SSL_CERTIFICATE_HERE
" "-----END CERTIFICATE-----
";
```

> **Note**: Dynamic credentials provided via BLE Provisioning or SoftAP are stored non-volatilely in NVS (`relay-config`) and take precedence over static defaults in `config.h`. If `root_ca_pem` contains the placeholder string or is empty, the underlying network client automatically switches to `setInsecure()` mode for self-signed certificates.

---

## Bluetooth BLE Provisioning & Mobile App Integration

The system implements on-demand Bluetooth Low Energy (BLE) provisioning, fully compatible with mobile applications using the identical GATT protocol schema from `GLP_Presure`.

This channel is focused strictly on:
1. **Device Self-Identification**: Allows the mobile app to scan, connect, and identify the device type, MAC address, chip ID, and firmware version.
2. **Credential Provisioning**: Configures WiFi SSID/password and MQTT broker credentials (host, port, user/API key, password, device ID, OTA key) without needing measurement parameters or cellular/APN fields.
3. **Data Frame Handling**: Accepts both individual GATT characteristic writes and unified JSON data frames ("tramas").

### Overview & Workflow

```
+--------------------+                    +---------------------+                    +----------------------+
|  Mobile App (BLE)  |                    | Relay-Wifi (ESP32)  |                    |   NVS Flash Memory   |
+--------------------+                    +---------------------+                    +----------------------+
          |                                         |                                          |
          | --- 1. Scan ('RELAY-<DeviceID>') -----> |                                          |
          | --- 2. Connect to Service UUID -------> |                                          |
          |                                         |                                          |
          | <-- 3. Read CHAR_DEVID / JSON Frame --- | (Identification confirmed)               |
          |                                         |                                          |
          | --- 4. Write credentials -------------> |                                          |
          |        (Per-characteristic or JSON)     |                                          |
          |                                         |                                          |
          | --- 5. Write to CHAR_SAVE ------------> | --- Store credentials -----------------> |
          |                                         |                                          |
          | <-- 6. ACK Notification ('OK') -------- |                                          |
          |                                         | --- Reboot & Connect to WiFi -----------> |
```

### GATT Service & Characteristics Layout

- **Service UUID**: `12345678-1234-1234-1234-123456789abc`
- **Advertisement Name**: `RELAY-<DeviceID>` (e.g. `RELAY-0000A1B2C3D4`)

| Characteristic | UUID | Permissions | Description | Example / Default Value |
|---|---|---|---|---|
| **Device ID** | `aaaaaaaa-bbbb-cccc-dddd-000000000008` | `READ` / `WRITE` | Unique hardware identifier ("device identity card") presented to servers and app | `0000A1B2C3D4` or eFuse-derived |
| **Device Key** | `aaaaaaaa-bbbb-cccc-dddd-000000000015` | `READ` | Factory secret/private hardware cryptographic key (Sonoff `apikey`/`devicekey` style) to verify authenticity | Unique cryptographic UUID |
| **Bind Token** | `aaaaaaaa-bbbb-cccc-dddd-000000000016` | `READ` / `WRITE` | User account claim/binding token injected by mobile app to associate the switch with user profile | `"claim_tok_9876543210"` |
| **WiFi SSID** | `aaaaaaaa-bbbb-cccc-dddd-000000000012` | `READ` / `WRITE` | 2.4 GHz wireless network SSID | `"MyHome_Fiber"` |
| **WiFi Password** | `aaaaaaaa-bbbb-cccc-dddd-000000000013` | `READ` / `WRITE` | WPA/WPA2 network passphrase | `"SecretPassword123"` |
| **Cloud Server** | `aaaaaaaa-bbbb-cccc-dddd-000000000004` | `READ` / `WRITE` | Regional cloud server dispatch endpoint or MQTT broker | `"broker.hivemq.com"` |
| **Server Port** | `aaaaaaaa-bbbb-cccc-dddd-000000000005` | `READ` / `WRITE` | TCP/TLS server port (string or integer) | `"8883"` (or `1883`) |
| **User / Account** | `aaaaaaaa-bbbb-cccc-dddd-000000000006` | `READ` / `WRITE` | User account ID / registered email / MQTT username | `"user@example.com"` |
| **MQTT Password** | `aaaaaaaa-bbbb-cccc-dddd-000000000007` | `READ` / `WRITE` | MQTT broker password (if applicable) | `""` |
| **OTA HMAC Key** | `aaaaaaaa-bbbb-cccc-dddd-000000000011` | `READ` / `WRITE` | HMAC signature key for secure OTA firmware validation | `""` |
| **JSON / Data Frame** | `aaaaaaaa-bbbb-cccc-dddd-000000000014` | `READ` / `WRITE` / `NOTIFY` | Unified frame channel (read dynamic self-identification; write JSON or Key=Value frames) | See frame format below |
| **Save / ACK** | `aaaaaaaa-bbbb-cccc-dddd-000000000099` | `WRITE` / `READ` / `NOTIFY` | Save trigger into NVS and ACK notification (`"OK"`). Also accepts direct frame writes | `"READY"` -> Notifies `"OK"` |

> [!NOTE]
> **Compatibility with `GLP_Presure` & Sonoff/eWeLink Model**: The UUID layout maintains backwards compatibility with `GLP_Presure` (`...0004` to `...0013`, `...0099`) while introducing `...0015` (`device_key`) and `...0016` (`bind_token`) to support hardware identification and account claiming. Tank measurement parameters (`radius` ...0009, `length` ...0010) and cellular parameters (`apn` ...0001, `apn_user` ...0002, `apn_pass` ...0003) have been deliberately excluded, keeping the device streamlined for WiFi smart switches.

### Sonoff-Style Account Binding & Mutual Identification

Provisioning solves both local WiFi connectivity and **permanent switch-to-user account binding**:

1. **Hardware Mutual Identification (Device ID + Device Key)**:
   - On boot, the switch advertises via BLE as `RELAY-<DeviceID>` and exposes its `Device ID` (unique hardware identity) and factory `Device Key` (private cryptographic key).
   - The mobile application (logged into the user's account) detects the switch and reads these parameters to verify factory authenticity.
2. **Credential & Account Claim Token Injection (Bind Token)**:
   - The mobile app injects into the switch:
     - Local WiFi network credentials (`ssid` and `password`).
     - Regional cloud server or MQTT broker endpoint (`server` and `port`).
     - Account claim token (`bind_token` / `account`) generated by the active user session.
3. **Claiming and Registration ("ready_to_claim")**:
   - The firmware commits these parameters to non-volatile flash (NVS) and fires an `"OK"` BLE notification.
   - Upon reboot and connecting to WiFi, the device reaches out to the cloud server indicating it is ready to be claimed with its `Device ID`, `Device Key`, and `Bind Token`.
   - The cloud server validates the active user session token and permanently associates the switch's `Device ID` with that user account.

---

### Standard Bluetooth Device Information Service (`0x180A`)

To ensure instantaneous plug-and-play identification by generic mobile apps (nRF Connect, LightBlue, Flutter libraries such as `flutter_blue_plus` / `flutter_reactive_ble`), the device also exposes the official Bluetooth SIG Device Information Service:

| Standard Characteristic | SIG UUID | Permissions | Returned Value |
|---|---|---|---|
| **Model Number String** | `0x2A24` | `READ` | `"Relay-Wifi ESP32"` |
| **Serial Number String** | `0x2A25` | `READ` | `<DeviceID>` (e.g. `0000A1B2C3D4`) |
| **Firmware Revision** | `0x2A26` | `READ` | Firmware version (e.g. `"v0.0.8"`) |
| **Hardware Revision** | `0x2A27` | `READ` | `"ESP32"` |
| **Manufacturer Name** | `0x2A29` | `READ` | `"VESIS"` |

---

### Provisioning via Individual Characteristics

Following the standard GATT characteristic workflow:
1. Mobile app connects to service `12345678-1234-1234-1234-123456789abc`.
2. App reads `CHAR_DEVID_UUID` (`...0008`) and `CHAR_DEVKEY_UUID` (`...0015`) to authenticate hardware.
3. App writes to SSID (`...0012`), Password (`...0013`), Server (`...0004`), Port (`...0005`), Account (`...0006`), and Bind Token (`...0016`).
4. App writes `"SAVE"` to **Save** characteristic (`...0099`).
5. Firmware commits configuration to NVS, emits a BLE notification with `"OK"`, and restarts into operational mode.

---

### Provisioning via Unified Data Frames (BLE Frames)

For mobile apps preferring to send all parameters in a single transmission payload, the firmware supports two frame formats written to `CHAR_FRAME_JSON_UUID` (`...0014`) or `CHAR_SAVE_UUID` (`...0099`):

#### Option A: JSON Frame (Full Account Binding)

```json
{
  "ssid": "MyWiFiNetwork_2.4G",
  "password": "MySuperSecretPassword",
  "server": "broker.hivemq.com",
  "port": 8883,
  "account": "user@example.com",
  "bind_token": "claim_tok_9876543210abcdef",
  "device_id": "RELAY-0000A1B2C3D4",
  "ota_key": "s3cr3t_k3y"
}
```

*Accepted JSON key aliases:*
- `wifi_ssid` or `ssid`
- `wifi_pass` or `pass` or `password`
- `mqtt_broker` or `broker` or `server`
- `mqtt_port` or `port`
- `account` or `user` or `user_id` or `account_id` or `email` or `mqtt_user`
- `bind_token` or `token` or `claim_token` or `account_token`
- `device_key` or `devicekey` or `apikey`
- `device_id` or `id` or `deviceid`
- `ota_hmac_key` or `ota_key`

#### Option B: Delimited Key=Value Frame

Useful for lightweight mobile apps or BLE serial terminals without a JSON serializer:

```text
ssid=MyWiFiNetwork_2.4G;pass=MySuperSecretPassword;server=broker.hivemq.com;port=8883;account=user@example.com;token=claim_tok_9876543210abcdef;id=RELAY-0000A1B2C3D4
```

*(Accepts `;`, `&`, or newline `\n` as delimiters, and `=` or `:` as key-value separators).*

---

### Device Identification to the Mobile App

For seamless discovery and status tracking by the mobile app:
1. **BLE Advertising**:
   - Local name: `RELAY-<DeviceID>` (e.g. `RELAY-0000A1B2C3D4`).
   - Active Scan Response containing Service UUID `12345678-1234-1234-1234-123456789abc`.
2. **Device ID Read (`CHAR_DEVID_UUID`)**:
   - Returns the active unique chip identifier string.
3. **Device Key Read (`CHAR_DEVKEY_UUID`)**:
   - Returns the factory cryptographic hardware secret key (`apikey`/`devicekey`).
4. **Dynamic Self-Identification Frame Read (`CHAR_FRAME_JSON_UUID`)**:
   - Performing a `READ` on `CHAR_FRAME_JSON_UUID` causes the firmware to generate and return a complete JSON snapshot in real time:
   ```json
   {
     "device_type": "Relay-Wifi",
     "device_id": "0000A1B2C3D4",
     "device_key": "A1B2C3D4-E5F6-4718-A93A-5A5A5A5A5A5A",
     "mac": "24:6F:28:XX:XX:XX",
     "version": "v0.0.8",
     "wifi_ssid": "MyWiFiNetwork",
     "server": "broker.hivemq.com",
     "port": 8883,
     "account": "user@example.com",
     "bind_token": "claim_tok_9876543210",
     "status": "ready_to_claim"
   }
   ```
5. **ACK Response via BLE Notification**:
   - Upon saving configuration (whether via frame or Save characteristic), the firmware emits a BLE notification containing `"OK"` on both `CHAR_SAVE_UUID` and `CHAR_FRAME_JSON_UUID`, allowing the mobile app to display immediate visual confirmation before reboot.

---

### Dual Provisioning Flow & Fallback Architecture

Provisioning is governed by a fault-tolerant dual-portal architecture:
1. **First Boot (Unprovisioned)**: The device automatically starts in BLE mode, advertising for 60 seconds. If no app connects, it falls back to the SoftAP rescue web portal (`VESIS-<ChipID>`).
2. **Button Long-Press (> 3 seconds)**: At any point during runtime, holding the physical button triggers the BLE provisioning portal for 60 seconds with rapid status LED blinking. If timeout expires without receiving configuration, it automatically switches to the rescue SoftAP portal (180s).
3. **Boot Hold**: Powering up the board while holding the button forces immediate entry into BLE provisioning mode.

---

## MQTT 5.0 Messaging & Topics

### Topic Topology

Topics are derived dynamically at runtime using the chip's unique eFuse MAC address or the configured Device ID:

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
    "FirmwareVersion": "v0.0.8"
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

## Physical Controls & Dual Provisioning Portals

The onboard push button on **GPIO 0** provides multi-mode functionality via `GpioButton`:

- **Short Press (< 3000 ms)**: Toggles Relay 1 locally, emits an immediate `"BUTTON_PRESS"` event JSON to `events`, and publishes the state string (`"on"`/`"off"`) to `device_switch1_state`.
- **Long Press (> 3000 ms)**: Launches the **BLE Provisioning Portal** (advertising as `RELAY-<DeviceID>` for 60 seconds with rapid LED indicator blinking). If no configuration is submitted via Bluetooth before timeout, it automatically launches the **SoftAP Web Portal**:
  - **SSID**: `VESIS-<ChipID>`
  - **Password**: `12345678`
  - **Timeout**: 180 seconds
  - Users can connect their smartphone or laptop to configure WiFi credentials. Once configured (or on timeout), the ESP32 automatically reboots.
- **Boot Hold (> 3000 ms)**: Powering on the board while holding GPIO 0 forces immediate entry into the BLE provisioning portal.

---

## Status Indicator (LED)

The status LED on **GPIO 2** provides clear visual diagnostics without blocking the processor:

- **Fast Blink (100 ms interval)**: Disconnected from WiFi / searching for network / BLE Provisioning active.
- **Slow Blink (400 ms interval)**: WiFi connected, attempting connection to MQTT 5.0 broker.
- **Solid ON**: Fully operational (WiFi and MQTT 5.0 connected).

---

## Build & Flashing Instructions

### Using PlatformIO

Make sure [PlatformIO Core](https://platformio.org/install/cli) is installed.

1. **Compile Firmware (ESP32 Dev Module)**:
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

## Broker Compatibility

Tested with:
- [Mosquitto](https://mosquitto.org/) 2.0+ (with MQTT 5.0 enabled)
- [EMQX](https://www.emqx.com/) 5.0+
- [HiveMQ Cloud](https://www.hivemq.com/) (MQTT 5.0 plan)
- [AWS IoT Core](https://aws.amazon.com/iot-core/) (via custom authentication)

If you encounter issues, please open an issue with broker version and configuration.

---

## License

This library is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.

## Contact

Author: **Jorge Gaspar Beltre Rivera**  
Project: Relay_Wifi_Esp32

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
