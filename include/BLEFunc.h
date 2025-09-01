#include <BLEWiFiConfig.h>
/**
 * @file BLEFunc.h
 * @brief Declarations for handling BLE requests from a mobile application.
 *
 * The device uses a BLE service to receive configuration data from a companion
 * mobile application. Incoming JSON commands are parsed and processed by
 * handleRequest(). The parsed data (such as server URLs or authorization
 * tokens) is stored in non‑volatile storage for later use by the firmware.
 */

#ifndef BLEFUNC_H
#define BLEFUNC_H

#include <BLEWiFiConfig.h>
#include <ArduinoJson.h>

// Forward declarations of global objects defined elsewhere in the firmware.
// BLEWiFiConfig instance that manages BLE communication and Wi‑Fi provisioning.
extern BLEWiFiConfig bleWiFiConfig;
// Preferences objects used for storing configuration parameters in NVS.
extern Preferences preferences;
extern Preferences prefs;

// Server URLs exchanged over BLE during provisioning.
extern String authUrl;
extern String tokenUrl;
extern String mqttUrl;

/**
 * @brief Parse and execute a JSON command received over BLE.
 *
 * Supported commands include:
 *  - SERVER_DATA   : store server endpoints in NVS
 *  - GET_TOKEN     : return the temporary user code for device pairing
 *  - CHECK_AUTH    : verify that an access token has been received
 *
 * @param data Raw JSON payload received from the mobile app.
 */
void handleRequest(const std::string& data);

/**
 * @brief Initialize the MQTT client once valid credentials are available.
 */
extern void initializeMQTT();

#endif // BLEFUNC_H
