# Relay-Wifi

**Smart Relay Controller for ESP32 with Clean Architecture, Native MQTT 5.0, Quick Pairing (BLE GATT + SmartConfig), Local LAN HTTP Control, and Secure Chunked OTA**

[![Platform](https://img.shields.io/badge/platform-ESP32-blue)](https://www.espressif.com/en/products/socs/esp32)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-green)](https://en.cppreference.com/w/cpp/17)
[![MQTT 5.0](https://img.shields.io/badge/MQTT-5.0-orange)](https://mqtt.org)
[![BLE](https://img.shields.io/badge/Bluetooth-BLE%20GATT-blueviolet)](https://github.com/h2zero/NimBLE-Arduino)
[![Architecture](https://img.shields.io/badge/Architecture-Clean%20Architecture-purple)](#architecture)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-teal)](https://platformio.org)
[![CMake](https://img.shields.io/badge/CMake-Supported-red)](https://cmake.org)

---

**Relay-Wifi** is an open-source firmware for ESP32 smart switches and relays, designed in **Clean Architecture** with native **MQTT 5.0**, **Quick Pairing (BLE GATT + Wi-Fi SmartConfig)**, **Local LAN HTTP Control (`/zeroconf/*`)**, and **Chunked OTA**. Engineered following strict **Clean Architecture (Hexagonal / Onion Architecture)** principles, the project completely decouples core business and domain rules from external drivers, frameworks, and hardware peripherals.

The system natively integrates **[ESP32_MQTTv5](https://github.com/JorgeGBeltre/ESP32_MQTTv5)** for secure, session-aware MQTT 5.0 telemetry and command handling, **[MQTTOTAv5](https://github.com/JorgeGBeltre/MQTTOTAv5)** for chunked Over-The-Air updates with SHA-256 integrity verification, **Quick Pairing** powered by **[NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)** plus **ESP-Touch / AirKiss**, and a **SoftAP Captive Portal (`VESIS-<ChipID>`)** for frictionless device onboarding.

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
  - [Step-by-Step Onboarding & Account Claiming Lifecycle](#step-by-step-onboarding--account-claiming-lifecycle)
  - [Standard Bluetooth Device Information Service (0x180A)](#standard-bluetooth-device-information-service-0x180a)
  - [Provisioning via Individual Characteristics](#provisioning-via-individual-characteristics)
  - [Provisioning via Unified Data Frames (BLE Frames)](#provisioning-via-unified-data-frames-ble-frames)
  - [Device Identification to the Mobile App](#device-identification-to-the-mobile-app)
  - [Dual Provisioning Flow & Fallback Architecture](#dual-provisioning-flow--fallback-architecture)
- [MQTT 5.0 Messaging & Topics](#mqtt-50-messaging--topics)
  - [Topic Topology](#topic-topology)
  - [Command Handling](#command-handling)
  - [JSON Telemetry & Status Payloads](#json-telemetry--status-payloads)
- [Local LAN HTTP Control (/zeroconf/*) & Scheduled Timers](#local-lan-http-control-zeroconf--scheduled-timers)
  - [Added Application & Infrastructure Functions](#added-application--infrastructure-functions)
  - [Local LAN HTTP Endpoints (:8081)](#local-lan-http-endpoints-8081)
- [Over-The-Air (OTA) Updates](#over-the-air-ota-updates)
- [Physical Controls & Dual Provisioning Portals](#physical-controls--dual-provisioning-portals)
- [Status Indicator (LED)](#status-indicator-led)
- [Build & Flashing Instructions](#build--flashing-instructions)
  - [Using PlatformIO](#using-platformio)
  - [Dual OTA Partition Scheme](#dual-ota-partition-scheme)
  - [Host Compilation with CMake](#host-compilation-with-cmake)
- [Backend Implementation](#backend-implementation)
- [Broker Compatibility](#broker-compatibility)
- [License](#license)
- [Contact](#contact)
- [Support](#support)

---

## Features

| Feature | Description |
|---|---|
| **Clean Architecture** | Pure Domain (zero-dependency C++17 entities/interfaces), decoupled Application services, isolated Infrastructure drivers, and unified Presentation composition root |
| **Native MQTT 5.0** | Powered by `ESP32_MQTTv5` — TLS encryption (`WiFiClientSecure`), keep-alive, sessions, properties, and JSON Last Will & Testament (LWT) |
| **Secure OTA Updates** | Powered by `MQTTOTAv5` — chunked binary transfers over MQTT 5.0, SHA-256 hash checks, automatic rollback protection, and non-blocking restarts |
| **Quick Pairing (BLE + SmartConfig)** | Powered by `NimBLE-Arduino` + `ESP-Touch / AirKiss` (`runQuickPairing`) — simultaneous BLE GATT (`RELAY-<DeviceID>`) and Wi-Fi SmartConfig onboarding via mobile app frames or individual GATT characteristics, with hardware self-identification (`Device ID` + factory `Device Key`) and account claim tokens (`Bind Token`) |
| **2-Stage Pairing & SoftAP `VESIS-<ChipID>`** | Stage 1 launches Quick Pairing (`runPairingWorkflow`) on a `5 s` long press; a 2nd `5 s` long press (or initial boot timeout) transitions to the `VESIS-<ChipID>` SoftAP captive portal (`WifiApProvisioningPortal::startPortal`), while a short press exits pairing mode |
| **Power-On State (`StartupMode`) & Deferred Save** | Immediate `< 20 ms` Offline-First boot restoration (`initWithStartupConfig`) distinguishing true hardware power-on resets (`ESP_RST_POWERON` / `ESP_RST_BROWNOUT` / `ESP_RST_EXT`) from software resets (`RTC_NOINIT` state mask), supporting `Off`, `On`, and `Stay` modes (`setStartupMode`) with `150 ms` deferred NVS writes (`flushDeferredSave`) |
| **Multi-Channel Relay Support (`1..4` Channels)** | Configurable `RELAY_CHANNEL_COUNT` (`1..4`, model `Relay-Wifi` .. `Relay-Wifi-4CH`) with per-channel physical buttons, `switch<N>:on\|off\|toggle`, `all:on\|off`, and multi-outlet LAN `/zeroconf/switches` |
| **Deterministic Desired vs Reported State** | Monotonic `sequence` & `state_version` ordering, 16-entry `commandId` idempotency ring buffer, physical GPIO readback verification, and correlated ACKs (`APPLIED` / `STALE_REJECTED`) on `devices/<ChipID>/commands/ack` |
| **Device Family ID & Dynamic `credential_mode`** | 7-byte BLE Manufacturer Data (`0xFFFF` `'V','S'`, `device_code = 0x01`) + JSON metadata exposing `device_family: "relay_wifi"` and `credential_mode` (`"wifi"` or `"adopt_only"` when Wi-Fi is already active while unbound) |
| **Account-Binding & Cryptographic Unlink** | Explicit `account_bound` NVS flag (`acct_bound`) isolating temporary Wi-Fi/broker outages from authoritative platform unlinking (`devices/<ChipID>/unlink/command`, `CONNACK 0x87` `ACCOUNT_UNLINKED:<proof>`, and `/zeroconf/unlink` verified via constant-time `SHA-256`) |
| **Guarded Remote Wi-Fi Adoption** | Hot Wi-Fi credential switch over MQTT 5.0 (`devices/<ChipID>/wifi/command`) with 15-second association verification and automatic rollback to the previous SSID/passphrase on failure |
| **Inching / Pulse Mode** | Configurable auto-shutoff timer (`configurePulse`, `updatePulseTimer`) from `500 ms` to `36,000,000 ms` in `500 ms` steps |
| **Scheduled Timers (`TimerSchedulerService`)** | Up to `8` persistent NVS timers (`Once`, `Repeat`, `Duration`, schema `tmr_ver = 2` with per-channel or all-channel targeting) synced over MQTT 5.0 (`devices/<ChipID>/timers/command`) and LAN `/zeroconf/timers` without actuating relays on timer deletion |
| **Local LAN HTTP & AES-128-CBC (`LanZeroconfServer`)** | Local HTTP server on port `8081` (`/zeroconf/*`) + `_iot._tcp` mDNS (`iot_<ChipID>.local`) + UDP `:48899` discovery (`IOT_DISCOVER`) supporting plain JSON and `AES-128-CBC` (`PKCS#7`, `MD5(deviceKey)`) encrypted payloads via `CryptoUtils` |
| **Dual Control Flow** | Physical button support (`25 ms` sample ticks, `50 ms` press/release debounce, immediate `5,000 ms` long-press detection while held) and remote MQTT 5.0 / LAN actuation |
| **NVS Persistent Storage** | Dynamic credentials, `StartupMode`, pulse settings, `sledOnline`, and scheduled timers persisted non-volatilely via `Preferences` in the `relay-config` namespace |
| **Non-blocking Multi-Pattern LED (`GpioIndicator`)** | `100 ms` base-tick LED pattern state machine (`Disconnected`, `Connecting`, `QuickPairing`, `CompatibleAp`, `OtaProgress`, `Operational`) with persistent `sledOnline` (`setSledOnline`) configuration |
| **NTP Synchronization** | Accurate UTC timestamping (ISO 8601 `YYYY-MM-DDTHH:MM:SS.mmmZ` and `epochSeconds()`) synchronized with public NTP pools (`pool.ntp.org`, `time.nist.gov`) |
| **Dual Build Toolchain** | Full target firmware builds via PlatformIO (`platformio.ini`) and pure host test compilation via CMake (`CMakeLists.txt`) |

---

## Architecture

The project is structured in **Clean Architecture**, engineered for modularity, testability, and clean separation of concerns.

### Layer Separation

1. **Domain Layer (`src/Domain/`)**:
   - Contains pure C++17 entities (`RelayState`, `DeviceIdentity`, `SystemMetrics`, `DeviceEvent`, `DeviceConfig`, `StartupMode`, `ScheduledTimer`, `TimerType`) and abstract interfaces (`IRelayController`, `IButtonHandler`, `ITelemetryTransport`, `IOtaService`, `INetworkLink`, `IIndicator`, `ILogger`, `IClock`, `ITimeProvider`, `IDeviceRepository`, `ISystemControl`, `IConfigRepository`, `IProvisioningPortal`).
   - Contains **zero** external library or Arduino/ESP-IDF dependencies.
2. **Application Layer (`src/Application/`)**:
   - Implements use cases and business orchestration:
     - `RelayFirmwareController`: finite state machine managing system transitions (`Connecting` → `Operational`), Offline-First button events (`handleButtonEvents`), deferred NVS flushes, pulse expiration, scheduled timer evaluation, indicator status updates (`updateIndicatorForConnectivity`), local state change callbacks (`setOnLocalStateChangeCallback`), and command routing (`processCommand`).
     - `RelayService`: manages physical and virtual relay states, boot restoration (`initWithStartupConfig`), power-on mode (`setStartupMode`, `getStartupMode`), `150 ms` deferred NVS persistence (`flushDeferredSave`, `hasPendingDeferredSave`), and Inching/Pulse mode (`configurePulse`, `updatePulseTimer`, `isPulseEnabled`, `getPulseWidthMs`, `isPulseActive`).
     - `TimerSchedulerService`: manages up to `8` persistent scheduled timers (`loadFromStorage`, `upsertTimer`, `clearAllTimers`, `getActiveTimerCount`, `evaluate`) for `Once`, `Repeat`, and `Duration` schedules.
     - `CryptoUtils`: pure C++17 cryptographic utilities (`computeMd5`, `deriveLanKey`, `aes128CbcEncryptPkcs7`, `aes128CbcDecryptPkcs7`, `base64Encode`, `base64Decode`, `decryptZeroconfPayload`, `encryptZeroconfPayload`).
     - `ConnectivityService`: coordinates network links (WiFi) and MQTT 5.0 transports.
     - `TelemetryService`: formats and dispatches periodic status, event envelopes, and switch states.
     - `MqttTopics`: pure string topic generation based on device identity.
     - `IPayloadSerializer`: abstract data transfer interface.
3. **Infrastructure Layer (`src/Infrastructure/`)**:
   - Implements concrete domain interfaces using hardware drivers and external libraries:
     - `Actuators`: `GpioRelay` (GPIO pin control).
     - `Button`: `GpioButton` (falling-edge ISR + `25 ms` periodic tick sampler, `50 ms` press/release debounce, immediate `5,000 ms` `LongPress` emission while held, and `isPhysicallyPressed`).
     - `Config`: `config.h` (credentials and timing), `BoardConfig.h` (pin mapping), `nvs_config.h`/`cpp` (`Preferences` NVS persistence including `saveLastRelayStateNvs`, `saveStartupModeNvs`, `savePulseConfigNvs`, `saveSledOnlineNvs`, `saveTimersNvs`, `loadTimersNvs`), and `NvsConfigRepository.h` (`IConfigRepository` adapter).
     - `Indicator`: `GpioIndicator` (`100 ms` non-blocking LED pattern state machine and `setSledOnline` / `isSledOnline`).
     - `Logging`: `SerialLogger` (serial logging output).
     - `Messaging`: `mqtt_client` and `Mqttv5Transport` (wrapping `ESP32_MQTTv5`).
     - `Net`: `WifiLink` (non-blocking background station association), `NetworkClient` (`WiFiClientSecure` with TLS), and `LanZeroconfServer` (HTTP `:8081` `/zeroconf/*` server + mDNS discovery via `loop`, `notifyMdnsStateChange`, `startServer`, `stopServer`, `dispatchZeroconf`).
     - `Ota`: `ota_manager` and `Mqttv5OtaService` (wrapping `MQTTOTAv5`).
     - `Provisioning`: `ble_provisioning.h`/`cpp` (`runQuickPairing`, `runBLEProvisioning`), `ProvisioningPortals.h` (`BleProvisioningPortal::runInteractive`), and `WifiApProvisioningPortal.h` (`WifiApProvisioningPortal::startPortal` for `VESIS-<ChipID>`).
     - `Serialization`: `ArduinoJsonPayloadSerializer` (ArduinoJson v6 schema formatter).
     - `System`: `EspDeviceRepository` (eFuse MAC, chip model, heap) and `EspSystemControl` (reboot).
     - `Time`: `ArduinoClock` and `NtpTimeProvider` (SNTP + ISO 8601 formatting + `epochSeconds()`).
4. **Presentation Layer (`src/Presentation/`)**:
   - `main.cpp`: Composition Root. Restores relay state immediately at boot (`initWithStartupConfig`), coordinates the 2-stage pairing workflow (`runPairingWorkflow`), wires `TimerSchedulerService` and `LanZeroconfServer`, and exposes `setup()` and `loop()`.

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
    │   │   ├── ScheduledTimer.h
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
    │       ├── CryptoUtils.h
    │       ├── CryptoUtils.cpp
    │       ├── RelayService.h
    │       ├── RelayService.cpp
    │       ├── TelemetryService.h
    │       ├── TelemetryService.cpp
    │       ├── TimerSchedulerService.h
    │       └── TimerSchedulerService.cpp
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
    │   │   ├── LanZeroconfServer.h
    │   │   ├── LanZeroconfServer.cpp
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

Default board wiring configured in `src/Infrastructure/Config/BoardConfig.h` and `src/Infrastructure/Config/config.h` (`RELAY_CHANNEL_COUNT` defaults to `1` and supports `1..4` channels):

| Peripheral | GPIO Pin | Configuration | Function |
|---|---|---|---|
| **Relay 1** | `GPIO 4` | Output (Active High) | Primary relay output actuator (`device_switch1`) |
| **Relay 2..4** *(optional)* | `GPIO 16 / 17 / 18` | Output (Active High) | Secondary relay actuators when `RELAY_CHANNEL_COUNT` is set to `2..4` (`device_switch2..4`) |
| **Button 1** | `GPIO 0` | Input (`INPUT_PULLUP`) | Physical toggle (`50 ms` debounce) / Quick Pairing & SoftAP `VESIS-<ChipID>` (`5,000 ms` long press) |
| **Button 2..4** *(optional)* | `GPIO 32 / 33 / 25` | Input (`INPUT_PULLUP`) | Per-channel physical toggle inputs when `RELAY_CHANNEL_COUNT` is set to `2..4` |
| **Status LED** | `GPIO 2` | Output (Active High) | Multi-pattern WiFi, Quick Pairing, SoftAP, OTA, and connection status indicator |

---

## Dependencies

The project relies on external libraries linked via PlatformIO:

- **[ESP32_MQTTv5](https://github.com/JorgeGBeltre/ESP32_MQTTv5)** (`v1.0.6`, linked locally via `file://../ESP32_MQTTv5`): Native MQTT 5.0 client library for ESP32 with dedicated TX/RX buffers, raw OTA topic coexistence (`setRawExclusiveTopic`), and `CONNACK`/`DISCONNECT` `ReasonString` inspection.
- **[MQTTOTAv5](https://github.com/JorgeGBeltre/MQTTOTAv5)** (`v1.0.3`, linked locally via `file://../MQTTOTAv5`): Over-The-Air firmware updater over MQTT 5.0 preserving application Will messages.
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

inline const char* root_ca_pem = "-----BEGIN CERTIFICATE-----\n"
"YOUR_SSL_CERTIFICATE_HERE\n"
"-----END CERTIFICATE-----\n";
```

> **Note**: Dynamic credentials provided via Quick Pairing (BLE / SmartConfig) or SoftAP (`VESIS-<ChipID>`) are stored non-volatilely in NVS (`relay-config`) and take precedence over static defaults in `config.h`. If `root_ca_pem` contains the placeholder string or is empty, the underlying network client automatically switches to `setInsecure()` mode for self-signed certificates.

---

## Bluetooth BLE Provisioning & Mobile App Integration

The system implements on-demand **Quick Pairing** (`runQuickPairing`), combining Bluetooth Low Energy (BLE) GATT provisioning and **Wi-Fi SmartConfig (`ESP-Touch / AirKiss`)** simultaneously, engineered specifically for smart switches with hardware identity and account-binding support.

This channel is focused strictly on:
1. **Device Self-Identification**: The switch exposes its factory `Device ID` (unique hardware serial number) and `Device Key` (`apikey` private cryptographic key) to allow mobile apps to authenticate hardware authenticity.
2. **Credential Provisioning**: Injects WiFi credentials (`ssid`, `password`), cloud server / MQTT broker endpoint (`server`, `port`, `account`, `password`, `ota_key`), and optional device behavior settings (`startup`, `pulse`, `pulseWidth`, `sledOnline`).
3. **Account Binding (Claim Token)**: Injects the user's active session claim token (`bind_token`) to permanently bind the switch to the user's profile upon first cloud connection.
4. **Data Frame Handling**: Accepts individual GATT characteristic writes, unified JSON or `Key=Value` data frames ("tramas"), or SmartConfig (`ESP-Touch / AirKiss`) broadcast frames.

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
| **Device Key** | `aaaaaaaa-bbbb-cccc-dddd-000000000015` | `READ` | Factory secret/private hardware cryptographic key (`apikey`/`devicekey`) to verify authenticity and derive LAN keys | Unique cryptographic UUID |
| **Bind Token** | `aaaaaaaa-bbbb-cccc-dddd-000000000016` | `READ` / `WRITE` | User account claim/binding token injected by mobile app to associate the switch with user profile | `"claim_tok_9876543210"` |
| **WiFi SSID** | `aaaaaaaa-bbbb-cccc-dddd-000000000012` | `READ` / `WRITE` | 2.4 GHz wireless network SSID | `"MyHome_Fiber"` |
| **WiFi Password** | `aaaaaaaa-bbbb-cccc-dddd-000000000013` | `WRITE` | WPA/WPA2 network passphrase (write-only; never exposed over BLE reads) | `"SecretPassword123"` |
| **Cloud Server** | `aaaaaaaa-bbbb-cccc-dddd-000000000004` | `READ` / `WRITE` | Regional cloud server dispatch endpoint or MQTT broker | `"broker.hivemq.com"` |
| **Server Port** | `aaaaaaaa-bbbb-cccc-dddd-000000000005` | `READ` / `WRITE` | TCP/TLS server port (plain TCP on `1883`, TLS on `8883`) | `"8883"` (or `1883`) |
| **User / Account** | `aaaaaaaa-bbbb-cccc-dddd-000000000006` | `WRITE` | User account ID / MQTT API key (write-only secret) | `"user@example.com"` |
| **MQTT Password** | `aaaaaaaa-bbbb-cccc-dddd-000000000007` | `WRITE` | MQTT broker password (write-only secret) | `""` |
| **OTA HMAC Key** | `aaaaaaaa-bbbb-cccc-dddd-000000000011` | `WRITE` | HMAC signature key for secure OTA firmware validation (write-only secret) | `""` |
| **JSON / Data Frame** | `aaaaaaaa-bbbb-cccc-dddd-000000000014` | `READ` / `WRITE` / `NOTIFY` | Unified frame channel with chunk reassembly (read dynamic self-identification; write JSON or Key=Value frames) | See frame format below |
| **Save / ACK** | `aaaaaaaa-bbbb-cccc-dddd-000000000099` | `WRITE` / `READ` / `NOTIFY` | Save trigger into NVS and ACK notification (`"OK"`). Also accepts direct frame writes | `"READY"` -> Notifies `"OK"` |

> [!NOTE]
> **Dedicated Smart Switch Architecture**: This BLE provisioning implementation is purpose-built for WiFi smart switches. It configures WiFi credentials, server/broker endpoints, hardware identification (Device ID and factory Device Key), user account claiming (Bind Token), and startup/pulse parameters.

### Step-by-Step Onboarding & Account Claiming Lifecycle

The provisioning flow executes a 7-step deterministic security and association lifecycle:

#### Step 1: Hardware Identity Generation & Initialization
1. Upon entering provisioning mode, the firmware inspects the non-volatile storage (`NVS`, namespace `relay-config`).
2. **Device ID Generation**: If no `device_id` is stored, the firmware extracts the ESP32's factory eFuse MAC address via `ESP.getEfuseMac()` and formats it into a 12-character uppercase hexadecimal string:
   ```cpp
   uint64_t chipId = ESP.getEfuseMac();
   snprintf(buf, sizeof(buf), "%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
   ```
   This serves as the unique hardware serial number ("device identity card") recognized by mobile apps and cloud servers.
3. **Factory Device Key Generation**: If no `device_key` is stored, the firmware derives a factory-grade UUID v4 cryptographic key using the hardware MAC entropy, bit shifting, and XOR masking (`%08X-%04X-4%03X-%04X-%012llX`). This key acts as the factory private symmetric key (`apikey` / `devicekey`) used for LAN-mode encryption (`deriveLanKey`) and hardware authenticity verification.
4. Both keys are permanently stored in NVS and survive reboots and OTA updates.

#### Step 2: BLE Stack, GATT Server & SmartConfig Startup
1. The firmware starts **Wi-Fi SmartConfig (`ESP-Touch / AirKiss`)** in station mode (`WiFi.beginSmartConfig()`).
2. In parallel, the firmware sets the BLE local advertising name to `RELAY-<DeviceID>` (e.g., `RELAY-0000A1B2C3D4`).
3. The NimBLE stack initializes with an extended MTU of **512 bytes**, allowing high-throughput JSON frame transfers without packet truncation.
4. The server starts the standard **Bluetooth SIG Device Information Service** (`0x180A`) for universal mobile OS compatibility.
5. The server creates the primary **Relay Provisioning Service** (`12345678-1234-1234-1234-123456789abc`) and registers all credential and identity characteristics.
6. Advertising begins with active scan response enabled while the status LED emits the `QuickPairing` double-blink cadence (`100 ms ON, 100 ms OFF, 100 ms ON, 700 ms OFF`).

#### Step 3: Mobile App Scanning & Hardware Authenticity Verification
1. The user opens the mobile application (where they have already authenticated into their cloud user account).
2. The mobile app scans for BLE peripherals matching the `RELAY-*` prefix or advertising the Service UUID.
3. Upon connection, the app reads:
   - `CHAR_DEVID_UUID` (`...0008`): Confirms the target device ID.
   - `CHAR_DEVKEY_UUID` (`...0015`): Reads the factory cryptographic secret key (`apikey`).
   - Or alternatively, issues a single `READ` to `CHAR_FRAME_JSON_UUID` (`...0014`), which returns a live JSON payload with `"status": "ready_to_claim"`.
4. The mobile app validates the cryptographic `Device Key` against cloud authority records to certify that the physical hardware is genuine before prompting the user for network credentials.

#### Step 4: Credential & Account Claim Token Injection
The mobile app injects the required configuration into the switch. The firmware supports four interchangeable injection workflows:
- **Method A (Unified JSON Frame)**: The app sends a single JSON payload to `CHAR_FRAME_JSON_UUID` (`...0014`) or `CHAR_SAVE_UUID` (`...0099`) containing WiFi credentials, server endpoint, user account, and the active session claim token (`bind_token`).
- **Method B (Delimited Key=Value Frame)**: The app sends a compact delimited string (`ssid=...;pass=...;server=...;port=...;account=...;token=...`) formatted with `;`, `&`, or newlines.
- **Method C (Individual GATT Characteristics)**: The app writes to dedicated characteristics sequentially:
  - `CHAR_WIFISSID_UUID` (`...0012`): Target 2.4 GHz WiFi SSID.
  - `CHAR_WIFIPASS_UUID` (`...0013`): Target WiFi WPA/WPA2 passphrase.
  - `CHAR_BROKER_UUID` (`...0004`): Regional cloud server or MQTT broker hostname/IP.
  - `CHAR_PORT_UUID` (`...0005`): Server port (e.g., `8883` TLS or `1883`).
  - `CHAR_MQTTUSER_UUID` (`...0006`): User account identifier or email address.
  - `CHAR_BINDTOKEN_UUID` (`...0016`): The account claim token generated by the cloud for the logged-in user.
- **Method D (Wi-Fi SmartConfig `ESP-Touch / AirKiss`)**: Broadcasts SSID and passphrase over 2.4 GHz UDP frames captured directly by `WiFi.smartConfigDone()`.

#### Step 5: NVS Flash Persistence & BLE ACK Handshake
1. Once all parameters are received (or upon receiving `"SAVE"` on `CHAR_SAVE_UUID` `...0099`), the firmware commits the configuration to flash memory using `saveConfig()` under the `relay-config` namespace.
2. The internal flag `_configSaved` is set to `true`.
3. The firmware immediately triggers a BLE notification with the payload `"OK"` on both `CHAR_SAVE_UUID` (`...0099`) and `CHAR_FRAME_JSON_UUID` (`...0014`).
4. The mobile app receives the `"OK"` ACK and updates its UI to notify the user that provisioning succeeded.
5. The firmware pauses for a 600 ms grace period to guarantee that the BLE ACK packet is completely transmitted over the air before shutting down the radio.

#### Step 6: Controlled Reboot & Cloud Account Claiming
1. The firmware stops SmartConfig and BLE advertising, cleanly deinitializes the NimBLE stack (`NimBLEDevice::deinit(true)`), and reboots the ESP32.
2. Upon restart, the firmware reads the newly saved credentials from NVS and initiates a station connection to the local WiFi access point.
3. Once WiFi is connected, the device establishes a TLS connection to the configured MQTT 5.0 broker.
4. The switch reports its identity and claim payload (`Device ID`, `Device Key`, and `Bind Token`).
5. The cloud backend validates that the `Bind Token` matches the user's active session and registers the switch's `Device ID` under that user's account.
6. The switch is now operational, reporting status via MQTT 5.0 and listening for remote and local LAN commands.

#### Step 7: 2-Stage Pairing & SoftAP `VESIS-<ChipID>` Fallback
1. **Unprovisioned First Boot**: Starts in Quick Pairing (`runQuickPairing`) for 60 seconds; if no credentials are received, it transitions automatically to the SoftAP captive web portal (`VESIS-<ChipID>`, password `12345678`).
2. **1st Button Long-Press (`5,000 ms`)**: Enters Stage 1 Quick Pairing (`BLE GATT` + `ESP-Touch / AirKiss`) for 180 seconds with the double-blink LED pattern.
3. **2nd Button Long-Press (`5,000 ms` while in Quick Pairing)**: Transitions immediately to Stage 2 Compatible SoftAP (`VESIS-<ChipID>`, password `12345678`, `180 s`) with fast continuous LED blinking (`100 ms ON / 100 ms OFF`).
4. **Short Press during Pairing**: Cancels the active pairing portal (`QuickPairingResult::CancelledByButton`) and resumes normal operation without losing existing settings.

---

### Standard Bluetooth Device Information Service (`0x180A`)

To ensure instantaneous plug-and-play identification by generic mobile apps (nRF Connect, LightBlue, Flutter libraries such as `flutter_blue_plus` / `flutter_reactive_ble`), the device also exposes the official Bluetooth SIG Device Information Service:

| Standard Characteristic | SIG UUID | Permissions | Returned Value |
|---|---|---|---|
| **Model Number String** | `0x2A24` | `READ` | `"Relay-Wifi ESP32"` |
| **Serial Number String** | `0x2A25` | `READ` | `<DeviceID>` (e.g. `0000A1B2C3D4`) |
| **Firmware Revision** | `0x2A26` | `READ` | Firmware version (e.g. `"v0.0.11"`) |
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

#### Option A: JSON Frame (Full Account Binding & Behavior Settings)

```json
{
  "ssid": "MyWiFiNetwork_2.4G",
  "password": "MySuperSecretPassword",
  "server": "broker.hivemq.com",
  "port": 8883,
  "account": "user@example.com",
  "bind_token": "claim_tok_9876543210abcdef",
  "device_id": "RELAY-0000A1B2C3D4",
  "ota_key": "s3cr3t_k3y",
  "startup": "stay",
  "pulse": "off",
  "pulseWidth": 500,
  "sledOnline": "on"
}
```

*Accepted JSON key aliases:*
- `wifi_ssid` or `ssid`
- `wifi_pass` or `pass` or `password`
- `mqtt_broker` or `broker` or `server`
- `mqtt_port` or `port`
- `account` or `user` or `user_id` or `account_id` or `email` or `mqtt_user` or `api_key`
- `bind_token` or `token` or `claim_token` or `account_token`
- `device_key` or `devicekey` or `apikey`
- `device_id` or `id` or `deviceid`
- `ota_hmac_key` or `ota_key`
- `startup` (`"off"`, `"on"`, `"stay"`)
- `pulse` (`"on"`, `"off"`) and `pulseWidth` (`500..36000000`, step `500`)
- `sledOnline` (`"on"`, `"off"`)

#### Option B: Delimited Key=Value Frame

Useful for lightweight mobile apps or BLE serial terminals without a JSON serializer:

```text
ssid=MyWiFiNetwork_2.4G;pass=MySuperSecretPassword;server=broker.hivemq.com;port=8883;account=user@example.com;token=claim_tok_9876543210abcdef;id=RELAY-0000A1B2C3D4
```

*(Accepts `;`, `&`, or newline `\n` as delimiters, and `=` or `:` as key-value separators).*

---

### Device Identification to the Mobile App

For seamless discovery and status tracking by the mobile app:
1. **BLE Advertising & 7-Byte Manufacturer Data (`0xFFFF`)**:
   - Local name: `RELAY-<DeviceID>` (e.g. `RELAY-0000A1B2C3D4`).
   - Active Scan Response containing Service UUID `12345678-1234-1234-1234-123456789abc`.
   - 7-byte Manufacturer Specific Data frame: `[0xFF, 0xFF, 'V', 'S', stateByte, 0x01, provFlags]`, where `0x01` identifies `device_family = "relay_wifi"`, `stateByte` encodes configuration state (`0x00` `unconfigured`, `0x01` `incomplete`, `0x02` `configured_disconnected`, `0x03` `connection_failed`, `0x04` `wifi_connected`), and `provFlags` encodes `bits 0..1` `credential_mode` (`0 = wifi`, `2 = adopt_only`), `bit 4` `supports_wifi`, `bit 5` `account_bound`, and `bits 6..7` `(channel_count - 1)`.
2. **Device ID Read (`CHAR_DEVID_UUID`)**:
   - Returns the active unique chip identifier string.
3. **Device Key Read (`CHAR_DEVKEY_UUID`)**:
   - Returns the factory cryptographic hardware secret key (`apikey`/`devicekey`).
4. **Dynamic Self-Identification Frame Read (`CHAR_FRAME_JSON_UUID`)**:
   - Performing a `READ` on `CHAR_FRAME_JSON_UUID` causes the firmware to generate and return a complete non-secret JSON snapshot in real time (omitting stored Wi-Fi/MQTT passwords and API keys):
   ```json
   {
     "device_family": "relay_wifi",
     "device_code": 1,
     "device_type": "Relay-Wifi",
     "model": "Relay-Wifi",
     "channel_count": 1,
     "device_id": "0000A1B2C3D4",
     "chip_id": "0000A1B2C3D4",
     "device_key": "A1B2C3D4-E5F6-4718-A93A-5A5A5A5A5A5A",
     "mac": "24:6F:28:XX:XX:XX",
     "version": "v0.0.11",
     "config_state": "unconfigured",
     "credential_mode": "wifi",
     "requires_wifi": true,
     "requires_apn": false,
     "supports_wifi": true,
     "supports_apn": false,
     "adopt_ready": false,
     "account_bound": false,
     "wifi_connected": false,
     "configured_ssid": "",
     "server": "broker.hivemq.com",
     "port": 8883,
     "status": "ready_to_claim",
     "methods": ["ble", "softap", "smartconfig", "lan"]
   }
   ```
   - When Wi-Fi is already connected while `!account_bound`, `credential_mode` switches to `"adopt_only"` (`adopt_ready: true`) and omitting `ssid`/`wifi_ssid` in incoming frames preserves stored Wi-Fi credentials while binding platform broker/account credentials (`account_bound = true`).
5. **Live Wi-Fi Verification, SmartConfig UDP ACK & Safe BLE/SoftAP Transitions**:
   - When new Wi-Fi credentials arrive over BLE, the firmware tests `WiFi.begin(ssid, pass)` before closing GATT, notifying `"OK"` / `wifi_connected` on association or reporting authentication/AP errors (`auth_failed` / `ap_not_found`) so invalid credentials can be corrected immediately.
   - SmartConfig (`ESP-Touch / AirKiss`) broadcasts UDP `:18266` JSON ACKs (`chip_id`, `ip`, `port: 8081`, `channel_count`) upon `WL_CONNECTED`.
   - Before `NimBLEDevice::deinit(true)` or switching to Stage 2 SoftAP (`VESIS-<ChipID>`), `_bleStopping` suppresses `onDisconnect` re-advertising, disconnects active GATT peers cleanly, and `WifiApProvisioningPortal::startPortal` disables background station `autoReconnect` (`WiFi.setAutoReconnect(false)`) with `Connection: close` HTTP responses to lock the SoftAP on a single stable 2.4 GHz channel.

---

### Dual Provisioning Flow & Fallback Architecture

Provisioning is governed by a fault-tolerant 2-stage architecture (`runPairingWorkflow`):
1. **First Boot (Unprovisioned)**: Starts in Quick Pairing (`runQuickPairing`: BLE GATT + SmartConfig) for 60 seconds. If no configuration is received, it falls back automatically to the SoftAP rescue web portal (`VESIS-<ChipID>`).
2. **1st Button Long-Press (`5,000 ms`)**: At any point during runtime, holding the physical button for 5 seconds triggers Stage 1 Quick Pairing (`RELAY-<DeviceID>` BLE GATT + SmartConfig) for 180 seconds with the double-blink LED pattern.
3. **2nd Button Long-Press (`5,000 ms`)**: Holding the physical button for 5 seconds while in Quick Pairing switches immediately to Stage 2 SoftAP (`WifiApProvisioningPortal::startPortal`, SSID `VESIS-<ChipID>`, password `12345678`, timeout `180 s`), exposing both the HTML captive portal form (`WiFi SSID`, `WiFi Password`, `MQTT 5.0 Server`, `Port`, `API Key / Account`, `MQTT Password`, `Bind Token`, and `Power-On State`) and the Compatible Pairing JSON endpoints (`GET /device` and `POST /ap`), persisting all settings to NVS via `saveConfig()`.
4. **Short Press Cancel**: Pressing the button briefly while in Quick Pairing or SoftAP exits pairing mode and returns to normal operation.

---

## MQTT 5.0 Messaging & Topics

### Topic Topology

Topics are derived dynamically at runtime in `MqttTopics::forDevice(chipId)` (`src/Application/Messaging/MqttTopics.h`) so every channel is isolated per `<ChipID>` on multi-tenant brokers (auto-selecting plain TCP on port `1883` and `WiFiClientSecure` TLS on port `8883`):

| Topic | Direction | Description |
|---|---|---|
| `status/<ChipID>` | Outbound | Periodic (`60 s`) or on-demand full JSON state report (`Retain = true`); also used as MQTT 5.0 LWT offline topic |
| `events/<ChipID>` | Outbound | Discrete events (`MQTT_CONNECTED`, `BUTTON_PRESS`, `DEVICE_COMMAND`, `PULSE_AUTO_OFF`, `TIMER_TRIGGER`, `TIMERS_UPDATED`, `WIFI_UPDATED`, `WIFI_UPDATE_FAILED`, `Retain = false`) |
| `telemetry/<ChipID>` | Outbound | Periodic operational and diagnostic metrics snapshot (`Retain = false`) |
| `commands/<ChipID>` | Inbound | Device-scoped control commands and deterministic JSON command envelopes (`Retain = false`) |
| `devices/<ChipID>/commands/ack` | Outbound | Correlated command execution ACKs (`commandId`, `sequence`, `stateVersion`, `reportedState`, `status`: `APPLIED` \| `STALE_REJECTED` \| `FAILED`) |
| `devices/<ChipID>/timers/command` | Inbound | Synchronizes up to `8` UTC timers (`set_timers`) into NVS without actuating relays on timer deletion |
| `devices/<ChipID>/wifi/command` | Inbound | Guarded hot Wi-Fi credential update (`set_wifi`) with 15 s association verification and automatic rollback |
| `devices/<ChipID>/unlink/command` | Inbound | Authenticated platform account unlink (`unlink_proof = SHA256(deviceKey + ":UNLINK:" + chipId)[0..31]`) |
| `event/<ChipID>` | Inbound | Device-targeted event channel |
| `devices/<ChipID>/device_switch<N>/state` | Inbound | Turn channel `<N>` (`1..4`) on/off/toggle via MQTT payload (`on`, `off`, `toggle`) |
| `devices/<ChipID>/device_switch<N>/command` | Inbound | Per-channel command topic |
| `devices/<ChipID>/device_switch<N>_state` | Outbound | Confirmed physical channel state |
| `devices/<ChipID>/device_switch<N>/report` | Outbound | ACL-authorized uplink physical state confirmation (`"on"` / `"off"`) |
| `ota/<ChipID>` | Inbound | Exclusive binary OTA stream (`setRawExclusiveTopic`) handled by `MQTTOTAv5` |

### Command Handling

Sending plain command strings or deterministic JSON envelopes (`{"commandId":"...","sequence":125,"action":"SET_POWER","channel":1,"desiredState":false}`) to `commands/<ChipID>` triggers the following actions in `RelayFirmwareController::processCommand`:

- `"reset"`: Triggers a clean system restart via `ESP.restart()`.
- `"status"`: Forces immediate publication of the full device status JSON.
- `"toggle_all"`, `"all:on"`, `"all:off"`: Toggles or explicitly sets all configured relay channels (`1..RELAY_CHANNEL_COUNT`) and publishes confirmed states.
- `"switch<N>:on"`, `"switch<N>:off"`, `"switch<N>:toggle"` (e.g. `"switch1:on"` .. `"switch4:off"`): Directly modifies Relay `<N>`, verifies GPIO pin readback (`IRelayController::readState()`), deduplicates repeated `commandId` entries, rejects stale `sequence < lastCommandSequence` (`STALE_REJECTED`), and emits a correlated ACK on `devices/<ChipID>/commands/ack`.
- `"startup:off"`, `"startup:on"`, `"startup:stay"`: Configures the power-on `StartupMode` via `RelayService::setStartupMode` and persists it in NVS (`saveStartupModeNvs`).
- `"pulse:on,<width_ms>"`, `"pulse:off"`: Enables or disables Inching / Pulse mode (`500..36000000 ms` in `500 ms` steps) via `RelayService::configurePulse` and persists it in NVS (`savePulseConfigNvs`).
- `"sled:on"`, `"sled:off"`: Enables or disables the operational status LED (`GpioIndicator::setSledOnline`) and persists it in NVS (`saveSledOnlineNvs`).
- **Account Unlink vs Wi-Fi Outage Isolation**: While `account_bound == true`, router power loss or broker downtime never erases credentials or enters Quick Pairing automatically. Only a verified `unlink_proof` (`SHA256(deviceKey + ":UNLINK:" + chipId)[0..31]`) received on `devices/<ChipID>/unlink/command`, in a deferred `CONNACK`/`DISCONNECT` `0x87` (`ReasonString = "ACCOUNT_UNLINKED:<proof>"`), or on `POST /zeroconf/unlink` turns relays `OFF`, wipes NVS + Wi-Fi credentials, sets `account_bound = false`, and reboots into Stage 1 Quick Pairing.

### JSON Telemetry & Status Payloads

Status publication to `status/<ChipID>`:

```json
{
  "Device": {
    "status": "connected",
    "ChipId": "0000A1B2C3D4",
    "MacAddress": "24:6F:28:XX:XX:XX",
    "ChipType": "ESP32-D0WDQ6",
    "FirmwareVersion": "v0.0.11"
  },
  "Timestamp": "2026-10-10T20:30:00.000Z",
  "Details": {
    "chipId": "0000A1B2C3D4",
    "account_bound": true,
    "channel_count": 1,
    "state_version": 42,
    "sync_completed": true,
    "devices_count": 1,
    "wifi_status": "connected",
    "wifi_ssid": "MyWiFiNetwork",
    "local_ip": "192.168.1.50",
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

## Local LAN HTTP Control (`/zeroconf/*`) & Scheduled Timers

### Added Application & Infrastructure Functions

- **`RelayService` (`src/Application/Services/RelayService.h`)**:
  - `initWithStartupConfig(startupMode, lastSavedState, pulseEnabled, pulseWidthMs, nowMs)`: Restores the relay output in `< 20 ms` at boot according to `StartupMode::Off`, `StartupMode::On`, or `StartupMode::Stay`.
  - `flushDeferredSave(nowMs)` / `hasPendingDeferredSave()`: Commits `lastRelayState` to NVS `150 ms` after the latest state transition when `StartupMode::Stay` is active, preventing Flash wear during rapid toggles.
  - `configurePulse(enabled, widthMs, nowMs)` / `updatePulseTimer(nowMs, outExpiredDeviceId)`: Arms and evaluates the one-shot auto-shutoff pulse timer (`500 ms` to `36,000,000 ms` in `500 ms` increments).
  - `setStartupMode(mode)` / `getStartupMode()`: Updates and persists the power-on state policy.
- **`TimerSchedulerService` (`src/Application/Services/TimerSchedulerService.h`)**:
  - `loadFromStorage()`: Loads up to `8` `ScheduledTimer` entries from NVS (`loadTimersNvs`).
  - `upsertTimer(timer)` / `clearAllTimers()` / `getActiveTimerCount()`: Manages `Once`, `Repeat` (weekday bitmask + UTC hour/minute), and `Duration` (cyclic on/off loop) timers in NVS (`saveTimersNvs`).
  - `evaluate(currentEpochSec, nowMs)`: Evaluates active timers once per second against `NtpTimeProvider::epochSeconds()`.
- **`CryptoUtils` (`src/Application/Services/CryptoUtils.h`)**:
  - `computeMd5(data, len, outDigest)` & `deriveLanKey(deviceKey, outKey)`: Computes the 16-byte `MD5(deviceKey)` symmetric key.
  - `aes128CbcEncryptPkcs7(...)` & `aes128CbcDecryptPkcs7(...)`: Pure C++17 `AES-128-CBC` cipher with `PKCS#7` padding.
  - `base64Encode(...)` & `base64Decode(...)`: RFC 4648 Base64 encoding/decoding.
  - `decryptZeroconfPayload(...)` & `encryptZeroconfPayload(...)`: Encrypts and decrypts LAN `/zeroconf/*` envelopes.
- **`LanZeroconfServer` (`src/Infrastructure/Net/LanZeroconfServer.h`)**:
  - `loop(wifiConnected)` / `startServer()` / `stopServer()`: Manages the non-blocking HTTP server on port `8081` and mDNS responder.
  - `notifyMdnsStateChange()`: Increments the sequence counter and updates mDNS TXT records (`seq`, `encrypt`, `iv`, `data1`) on every local, timer, LAN, or MQTT 5.0 state change.
  - `dispatchZeroconf(uri, rawBody, respBuf, respBufSize)`: Processes plain JSON or `AES-128-CBC` encrypted HTTP `POST` requests and synchronizes changes with `RelayService` and `TelemetryService`.
- **`GpioButton`, `GpioIndicator` & `ble_provisioning`**:
  - `GpioButton::update()` & `GpioButton::isPhysicallyPressed()`: `25 ms` periodic tick sampler with `50 ms` debounce and immediate `5,000 ms` `LongPress` trigger.
  - `GpioIndicator::setSledOnline(enabled)` & `GpioIndicator::isSledOnline()`: Controls the `Operational` LED state and executes the `100 ms` multi-pattern cadence table.
  - `runQuickPairing(timeoutMs, indicator, button)` & `WifiApProvisioningPortal::startPortal(timeoutSec, indicator, button)`: Implements the 2-stage pairing state machine (`runPairingWorkflow`).

### Local LAN HTTP Endpoints (`:8081`)

`LanZeroconfServer` advertises `_iot._tcp.local` (`iot_<ChipID>.local`), responds to UDP `:48899` (`IOT_DISCOVER`) broadcasts, supports `GET /zeroconf/info` (including `device_family`, `device_code`, `credential_mode`, `account_bound`, `channel_count`, `switches`, `timers`, and `lan_proof = SHA256(deviceKey:chipId:localIp)[0..31]`), and accepts `HTTP POST` with either a plain JSON body (`{"data": {...}}`) or an encrypted envelope (`{"sequence":"...","deviceid":"...","selfApikey":"...","iv":"<Base64>","encrypt":true,"data":"<Base64>"}`):

| Endpoint | Method | Description | Example `data` Payload |
|---|---|---|---|
| `/zeroconf/switch` | `POST` | Turn relay ON or OFF immediately over LAN | `{"switch":"on"}` |
| `/zeroconf/switches` | `POST` | Multi-channel array format for relay actuation (`outlet: 0..3`) | `{"switches":[{"switch":"off","outlet":0}]}` |
| `/zeroconf/startup` | `POST` | Configure power-on behavior (`off`, `on`, `stay`) | `{"startup":"stay"}` |
| `/zeroconf/pulse` | `POST` | Configure Inching / Pulse mode (`500..36000000 ms`) | `{"pulse":"on","pulseWidth":1500}` |
| `/zeroconf/sled` | `POST` | Enable or disable the status LED when online | `{"sledOnline":"off"}` |
| `/zeroconf/info` | `GET` / `POST` | Query current switch(es), `device_family`, `credential_mode`, `account_bound`, `lan_proof`, startup, pulse, SLED, timers, version, and RSSI | `{}` |
| `/zeroconf/signal_strength` | `POST` | Query current Wi-Fi RSSI in dBm | `{}` |
| `/zeroconf/wifi` | `POST` | Update stored Wi-Fi SSID, password, and optional MQTT 5.0 broker parameters in NVS | `{"ssid":"NewWiFi","password":"Pass"}` |
| `/zeroconf/config` | `POST` | Update MQTT 5.0 broker, port, credentials, bind token (`account_bound = true`), startup, pulse, and SLED in NVS | `{"mqtt_broker":"broker.example.com","mqtt_port":8883,"startup":"stay"}` |
| `/zeroconf/timers` | `POST` | Replace/configure up to 8 scheduled timers (`once`, `repeat`, `duration`, `channel: 0..4`) | `{"timers":[{"id":"t1","enabled":true,"type":"once","channel":1,"at":1793500000,"do":{"switch":"on"}}]}` |
| `/zeroconf/unlink` | `POST` | Cryptographically verified local account unlink (`unlink_proof`) resetting device into Stage 1 Quick Pairing | `{"unlink_proof":"<32-hex-sha256>"}` |
| `/zeroconf/ota_unlock` | `POST` | Unlock local OTA state flag | `{}` |

---

## Over-The-Air (OTA) Updates

Over-The-Air updates are fully handled by **`MQTTOTAv5`**:

1. **Initialization**: Hooked directly inside `mqtt.onConnect()` in `mqtt_client.cpp`, subscribing the device to OTA update streams.
2. **Chunked Stream**: Firmware binaries are delivered in discrete chunks over MQTT 5.0, directly written into the standby OTA partition (`app0` or `app1`).
3. **Integrity & Security**: Verifies the cumulative SHA-256 hash against metadata transmitted in MQTT 5.0 User Properties.
4. **Safety & Rollback**: Validates image headers before writing; registers rollback protection via `esp_ota_ops.h` so the board reverts automatically if a new build fails to boot.
5. **Non-blocking Loop**: Managed via `ota.handle()` inside `loop()` while preserving physical button responsiveness and displaying the `OtaProgress` triple-blink LED pattern.

---

## Physical Controls & Dual Provisioning Portals

The onboard push button on **GPIO 0** provides multi-mode functionality via `GpioButton` (`25 ms` sample ticks):

- **Short Press (`50 ms .. < 5,000 ms`)**:
  - In normal operation: Toggles Relay 1 locally (Offline-First), schedules a `150 ms` deferred NVS save if `StartupMode == Stay`, emits a `"BUTTON_PRESS"` event JSON to `events`, publishes the state string (`"on"`/`"off"`) to `device_switch1_state`, and updates mDNS TXT state (`notifyMdnsStateChange`).
  - During `Quick Pairing` or `SoftAP`: Immediately cancels pairing mode and returns to normal operation.
- **1st Long Press (`>= 5,000 ms`, triggered immediately while held)**:
  - Launches Stage 1 **Quick Pairing** (`runQuickPairing`: advertising BLE GATT as `RELAY-<DeviceID>` + listening for Wi-Fi SmartConfig `ESP-Touch / AirKiss` simultaneously, with the double-blink LED pattern).
- **2nd Long Press (`>= 5,000 ms` while inside Quick Pairing)**:
  - Transitions immediately to Stage 2 **SoftAP Web Portal (`WifiApProvisioningPortal::startPortal`)**:
    - **SSID**: `VESIS-<ChipID>`
    - **Password**: `12345678`
    - **Timeout**: 180 seconds
    - Users can connect their smartphone or laptop to configure WiFi and MQTT 5.0 credentials, which are persisted to NVS (`relay-config`) via `saveConfig()` before rebooting.

---

## Status Indicator (LED)

The status LED on **GPIO 2** provides visual diagnostics via `GpioIndicator` using a non-blocking `100 ms` base timer:

| Status (`IndicatorStatus`) | Cadence Pattern (`100 ms` base tick) | Meaning |
|---|---|---|
| **`Disconnected`** | `1000 ms ON / 1000 ms OFF` | Disconnected from WiFi / searching for access point |
| **`Connecting`** | `200 ms ON / 1800 ms OFF` | WiFi connected, attempting connection to MQTT 5.0 broker |
| **`QuickPairing`** | `100 ms ON, 100 ms OFF, 100 ms ON, 700 ms OFF` | Double blink + pause: Stage 1 Quick Pairing (`BLE GATT` + `SmartConfig`) active |
| **`CompatibleAp`** | `100 ms ON / 100 ms OFF` (continuous 5 Hz) | Fast continuous blink: Stage 2 SoftAP (`VESIS-<ChipID>`) active |
| **`OtaProgress` / `Error`** | `100 ms ON, 100 ms OFF` (x3) + `400 ms OFF` | Triple blink + pause: OTA firmware transfer in progress or error |
| **`Operational`** | Solid `ON` (if `sledOnline == true`) or `OFF` (`sledOnline == false`) | Fully operational (WiFi and MQTT 5.0 connected) |

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

## Backend Implementation

To use MQTTOTA in your project, you'll need an MQTT/MQTTS/MQTTv5.0 server to manage OTA updates. You can implement your own backend using our reference repository:

**MQTT/MQTTS/MQTTv5.0 Broker support OTA Updates**
- **Repository:** [github.com/Ruben890/Mqtt-Broker](https://github.com/Ruben890/Mqtt-Broker)
- **Description:** Complete backend for managing broker via MQTT/MQTTS/MQTTv5.0
- **Features:**
  - Configurable MQTT/MQTTS/MQTTv5.0 server
  - IoT device management
  - Firmware update delivery
  - OTA progress tracking
  - Error handling and retry mechanisms

**Steps to use the backend:**
1. Clone the backend repository
2. Configure the MQTT broker according to your needs
3. Implement the update delivery logic
4. Connect your ESP32 devices to the broker
5. Manage OTA updates from a centralized interface

**Example workflow:**
```javascript
// From your backend
1. Prepare firmware in base64 format
2. Publish MQTT/MQTTS/MQTT5 message to target device
3. Monitor progress via callbacks
4. Confirm successful completion
5. Log results in database
```

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
