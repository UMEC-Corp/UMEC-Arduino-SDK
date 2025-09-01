#include "settings.h"
/**
 * @file settings.cpp
 * @brief Defines global configuration variables declared in settings.h.  This
 *        translation unit centralises all mutable singletons such as network
 *        URLs, access tokens and mutex objects so that they can be easily
 *        accessed by other modules.
 */

BLEWiFiConfig bleWiFiConfig;      ///< BLE provisioning helper instance
WiFiClientSecure secureClient;    ///< Shared TLS client

// Runtime configuration and tokens ---------------------------------------
String authUrl;
String tokenUrl;
String mqttUrl;
String deviceCode;
String userCode;
String verificationUrl;
String accessToken;
String refreshToken;
String cloudApiUrl;
String configUrl;
int expiresIn;                     ///< OAuth token TTL in seconds

// Preferences namespaces -------------------------------------------------
Preferences prefs;
Preferences pref;
Preferences preferences;

// Global mutexes ---------------------------------------------------------
SemaphoreHandle_t mqttMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t dataQueueMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t adsMutex = xSemaphoreCreateMutex();

bool normalMode = true;            ///< Flag used to indicate normal runtime mode

