/**
 * @file utilities.h
 * @brief Collection of helper functions and global variables used throughout
 *        the firmware.
 */

#ifndef UTILITIES_H
#define UTILITIES_H

#include <Preferences.h>
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include "settings.h"
#include "DataQueue.h"
#include "MutexLock.h"

extern SemaphoreHandle_t dataQueueMutex;  ///< Protects access to data queue
extern SemaphoreHandle_t mqttMutex;       ///< Protects MQTT client

// Global configuration variables populated during provisioning
extern String authUrl;
extern String tokenUrl;
extern String mqttUrl;
extern String accessToken;
extern String refreshToken;
extern String deviceCode;
extern String userCode;
extern String verificationUrl;
extern int expiresIn;
extern String cloudApiUrl;
extern String configUrl;

extern Preferences preferences;
extern Preferences prefs;

// Firmware metadata constants
extern const char versionf[];
extern const char model_code[];
extern const char* hw_code;

// Utility function declarations
String getChipID();
void readSerialCommands(void *pvParameters);
void processCommand(const String& command);
void initFileSystem();
const char *stringToConstChar(String str);
extern void checkWiFiAndMQTTConnection();
extern void mqttDisconnectTask();
void sendNvsSuccessResponse(int64_t id);
void sendErrorResponse(int64_t id, const char* errorMessage);

// I2C port expander helpers
extern uint8_t portStates[2];
void updatePortExpander(uint8_t reg, uint8_t value);
void setupPortExpander();
bool checkPCA9555();
void resetPCA9555();

// Watchdog timer wrapper
#include <esp_task_wdt.h>

/**
 * @brief Thin wrapper around the ESP task watchdog functions.
 *
 * The class ensures that watchdog initialization and task registration happen
 * only once and provides a static reset() method that can be called from
 * anywhere.
 */
class WDTWrapper {
public:
    static void init(int timeoutSec = 60) {
        static bool initialized = false;
        if (!initialized) {
            esp_task_wdt_init(timeoutSec, true);
            initialized = true;
        }
    }

    static void addThisTask() {
        static bool added = false;
        if (!added) {
            esp_task_wdt_add(NULL);
            added = true;
        }
    }

    static void reset() {
        esp_task_wdt_reset();
    }
};

#endif // UTILITIES_H

