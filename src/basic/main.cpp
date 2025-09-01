/**
 * @file main.cpp
 * @brief Entry point for the firmware build configured by the `basic` profile.
 *
 * The setup routine initializes subsystems such as Serial, Wi‑Fi, MQTT and
 * status LED handling. The application relies heavily on tasks defined in
 * setupTasks.h to run asynchronously under FreeRTOS.
 */

#include "settings.h"
#include "utilities.h"
#include <HTTPClient.h>
#include "SmoothLED.h"
#include "mqttFunc.h"
#include "mqttProcess.h"
#include "serverDataExchange.h"
#include "callbacks.h"
#include "BLEFunc.h"
#include "DataQueue.h"
#include "MutexLock.h"

// Firmware identification strings
const char versionf[] = "0.01";
const char model_code[] = "mydevice";
const char* hw_code = nullptr;

const int buttonPin(0);  ///< GPIO used for user button

// LEDs used to indicate Wi‑Fi/MQTT status
const byte LED_WIFI = 16;
const byte LED_SERVER = 2;
SmoothLED smoothLED1(LED_WIFI, 0, 255, 20, 5);   ///< LED for Wi‑Fi state
SmoothLED smoothLED2(LED_SERVER, 0, 255, 20, 5); ///< LED for server state

#include "setupTasks.h"

/**
 * @brief Arduino setup function. Initializes all required subsystems.
 */
void setup() {
    initializeTasks();
    initializeSerial();
    initializeSensors();
    initializePreferences();
    initializeWiFi();
    initializeIndication();
    initializeMQTT();
}

/**
 * @brief Arduino loop function. Delegates work to scheduled tasks.
 */
void loop() {
  loopTasks();
}
