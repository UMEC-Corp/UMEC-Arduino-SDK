#pragma once

#include "SmoothLED.h"
#include "globalConfig.h"
#include "MutexLock.h"
#include <WiFi.h>

/**
 * @file callbacks.h
 * @brief Asynchronous helpers for handling button presses, LED indication and
 *        periodic Wi‑Fi RSSI reporting.  These functions are used by various
 *        tasks and are kept in a separate translation unit to keep the main
 *        application code concise.
 */

// ---------------------------------------------------------------------------
// Extern declarations shared across modules
// ---------------------------------------------------------------------------

extern const int buttonPin;              ///< GPIO number of the user button
extern SmoothLED smoothLED2;             ///< Secondary status LED
extern SmoothLED smoothLED1;             ///< Primary status LED
extern TaskHandle_t prepareForPairingHandle;
extern TaskHandle_t readSerialCommandsHandle;
extern TaskHandle_t updateLEDsHandle;

extern Ticker setTimeTicker;             ///< Periodic time synchronization
extern Ticker ticker1min;                ///< One‑minute data polling ticker
extern Ticker ticker10sec;               ///< Regular sensor polling ticker

#ifdef CALIBRATION_MODE
extern void calibrationModeStart();
#endif

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

/** Forward declarations used elsewhere. */
void handleButtonPress(int buttonPin);
void blinkLEDs(SmoothLED& led1, SmoothLED& led2);

/**
 * @brief Safely stop auxiliary tasks and timers before entering pairing mode
 *        or performing a factory reset.
 */
inline void buttonTaskDelete() {
    if (readSerialCommandsHandle != NULL) {
        vTaskDelete(readSerialCommandsHandle);
        readSerialCommandsHandle = NULL;
        Serial.println("readSerialCommandsHandle удален");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Stop all active timers so they do not fire during reconfiguration.
    ticker10sec.detach();
    setTimeTicker.detach();
    ticker1min.detach();

    if (updateLEDsHandle != NULL) {
        vTaskDelete(updateLEDsHandle);
        updateLEDsHandle = NULL;
        Serial.println("updateLEDsHandle удален");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Task that watches the physical button and triggers either a factory
 *        reset (long press) or a simple reboot (short press).
 */
inline void prepareForPairing(void *pvParameters) {
    unsigned long lastTimeMillis = 0;  // Timestamp of the last button press

    for (;;) {
        int buttonState = digitalRead(buttonPin);

        if (buttonState == LOW) {
            Serial.println("button pressed");
            lastTimeMillis = millis();

            // Stop all other tasks while waiting for a potential reset.
            buttonTaskDelete();
            smoothLED1.setState(_BLINK_1);
            smoothLED2.setState(_BLINK_1);

            // Wait for the user to release the button.
            while (digitalRead(buttonPin) == LOW) {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }

            Serial.println("button released");

            if ((millis() - lastTimeMillis) > 7000) {
                // Long press: wipe all stored credentials and reboot.
                prefs.begin("nvs", false);
                prefs.putString("accessToken", "");
                prefs.putString("refreshToken", "");
                prefs.putString("authUrl", "");
                prefs.putString("mqttUrl", "");
                prefs.putString("tokenUrl", "");
                prefs.putString("configUrl", "");
                prefs.putString("cloudApiUrl", "");
                prefs.end();

                WiFi.disconnect(false, true);
                preferences.begin("wifi-config", false);
                preferences.putString("ssid", "");
                preferences.putString("password", "");
                preferences.end();
                Serial.println("Кнопка зажата более 7 секунд. Стираем данные и перезагружаемся");
                delay(300);
                ESP.restart();
            } else {
                // Short press: simply restart.
                Serial.println("Короткое нажатие. Перезагрузка платы");
                ESP.restart();
            }
       }

        vTaskDelay(100 / portTICK_PERIOD_MS);  // Reduce CPU usage of the task
    }
}

/**
 * @brief Measure current Wi‑Fi signal strength and enqueue it for publishing
 *        via MQTT.
 */
inline void sendWifiRSSI() {
    static String jsonTemplate =
        "{\"jsonrpc\":\"2.0\",\"method\":\"sensor-data\",\"params\":{\"wifi\":{\"value\":PLACEHOLDER}}}";

    int myRSSI = WiFi.RSSI();
    Serial.printf("Уровень сигнала WiFi: %d dBm\n", myRSSI);

    String jsonMessage = jsonTemplate;
    jsonMessage.replace("PLACEHOLDER", String(myRSSI));

    // Enqueue JSON message for later MQTT transmission.
    {
        MutexLock lock(dataQueueMutex);
        dataQueue["wifi"] = std::unique_ptr<DataItem>(
            new GenericDataItem("wifi", String(myRSSI), "sensor-data"));
        Serial.printf("Queue: Queued WiFi RSSI data: %s\n", jsonMessage.c_str());
    }
}

