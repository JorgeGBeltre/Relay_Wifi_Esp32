#pragma once

#include <Arduino.h>

/**
 * @brief Inicializa los recursos de BLE Provisioning.
 */
void initBLEProvisioning();

/**
 * @brief Ejecuta el servidor BLE de aprovisionamiento idéntico al protocolo de GLP_Presure.
 * Permite a la app móvil identificar el dispositivo y configurar credenciales WiFi y MQTT
 * mediante características GATT individuales o mediante tramas JSON.
 *
 * @param timeoutMs Tiempo máximo de espera en milisegundos (por defecto 60 segundos).
 * @return true si se guardó una configuración válida, false si expiró el tiempo.
 */
bool runBLEProvisioning(unsigned long timeoutMs = 60000);
