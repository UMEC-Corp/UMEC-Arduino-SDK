#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include "HTTPClient.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "BLEWiFiConfig.h"
#include <Wire.h>
#include "cert.h"
#include "WebSocketsClient.h" // include before MQTTPubSubClient.h
#include "MQTTPubSubClient.h"
#include <Ticker.h>

/**
 * @file settings.h
 * @brief Declares global configuration variables and handles used across the
 *        firmware modules.  These variables are defined in settings.cpp.
 */

// URLs and tokens retrieved during provisioning -------------------------
extern String authUrl;
extern String tokenUrl;
extern String mqttUrl;
extern String deviceCode;
extern String userCode;
extern String verificationUrl;
extern String accessToken;
extern String refreshToken;
extern String cloudApiUrl;
extern String configUrl;
extern String storedSSID;

// Preference namespaces --------------------------------------------------
extern Preferences prefs;        ///< Generic application preferences
extern Preferences pref;         ///< Additional namespace
extern Preferences preferences;  ///< Wi‑Fi credentials storage

extern WiFiClientSecure secureClient; ///< TLS client configured with CA bundle

extern void checkMemory(const char* stage);

// Synchronisation primitives shared between modules
extern SemaphoreHandle_t mqttMutex;
extern SemaphoreHandle_t dataQueueMutex;

#endif // SETTINGS_H

