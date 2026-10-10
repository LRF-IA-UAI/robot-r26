#pragma once
#include <stdint.h>
// ==========================================
// CONFIGURACIÓN DE RED Y CREDENCIALES (PLANTILLA)
// Renombra o copia este archivo como "secrets.h"
// y completa con tus valores reales.
// ==========================================

// Wi-Fi
const char *const SECRET_WIFI_SSID = "TU_NOMBRE_DE_RED";
const char *const SECRET_WIFI_PASS = "TU_PASSWORD_WIFI";

// MQTT
const char *const SECRET_MQTT_SERVER = "broker.ejemplo.com";
const unsigned SECRET_MQTT_PORT = 1883;
const char *const SECRET_MQTT_USER = "usuario_mqtt";
const char *const SECRET_MQTT_PASS = "pass_mqtt";

// Motor pins
const uint8_t MOTOR_FRONT_PIN_A = 0;
const uint8_t MOTOR_FRONT_PIN_B = 1;
const uint8_t MOTOR_BACK_PIN_A = 2;
const uint8_t MOTOR_BACK_PIN_B = 3;

// Robot configuration
const uint8_t ROBOT_ID = 0;
const char *const ROBOT_TYPE = "r26";