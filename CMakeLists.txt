cmake_minimum_required(VERSION 3.14)
project(relay_wifi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HOST_BUILD         "Build pure Domain+Application for host (no Arduino)" ON)
option(RELAY_BUILD_TESTS  "Build host unit tests (requires HOST_BUILD)"         OFF)

set(RELAY_APPLICATION_SOURCES
    src/Application/Services/RelayService.cpp
    src/Application/Services/ConnectivityService.cpp
    src/Application/Services/TelemetryService.cpp
    src/Application/Controllers/RelayFirmwareController.cpp
)

set(RELAY_INFRASTRUCTURE_SOURCES
    src/Infrastructure/Actuators/GpioRelay.cpp
    src/Infrastructure/Button/GpioButton.cpp
    src/Infrastructure/Messaging/Mqttv5Transport.cpp
    src/Infrastructure/Messaging/mqtt_client.cpp
    src/Infrastructure/Net/NetworkClient.cpp
    src/Infrastructure/Net/WifiLink.cpp
    src/Infrastructure/Ota/ota_manager.cpp
    src/Infrastructure/Serialization/ArduinoJsonPayloadSerializer.cpp
    src/Infrastructure/System/EspDeviceRepository.cpp
    src/Infrastructure/Time/NtpTimeProvider.cpp
    src/Infrastructure/Config/nvs_config.cpp
    src/Infrastructure/Provisioning/ble_provisioning.cpp
)

add_library(relay_core STATIC ${RELAY_APPLICATION_SOURCES})
target_include_directories(relay_core PUBLIC src src/Infrastructure/Config .)

if(NOT HOST_BUILD)
    target_sources(relay_core PRIVATE ${RELAY_INFRASTRUCTURE_SOURCES})
    target_include_directories(relay_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../ESP32_MQTTv5
        ${CMAKE_CURRENT_SOURCE_DIR}/../MQTTOTAv5
    )
endif()

if(NOT HOST_BUILD)
    add_executable(relay_firmware src/Presentation/main.cpp)
    target_link_libraries(relay_firmware PRIVATE relay_core)
endif()

if(RELAY_BUILD_TESTS AND HOST_BUILD)
    enable_testing()
    add_subdirectory(tests)
endif()
