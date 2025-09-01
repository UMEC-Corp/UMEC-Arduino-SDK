#pragma once

//************************************************************************
#include <ArduinoJson.h>
#include "utilities.h"

#include "WebSocketsClient.h" // include before MQTTPubSubClient.h
#include "MQTTPubSubClient.h"

/**
 * @file mqttProcess.h
 * @brief Helper functions that register MQTT subscriptions used by the basic
 *        firmware example.  These are intentionally lightweight – the main
 *        logic lives in mqttFunc.h – but having them separate keeps the
 *        example easy to read.
 */

/** Placeholder for project‑specific subscriptions. */
void subscribeTo() {}

/**
 * Subscribe to a time synchronization topic. When a timestamp is received from
 * the server it is immediately echoed back to acknowledge reception.
 */
void subscribeToTimeExchange() {
    mqtt.subscribe("command/" + String(getChipID()) + "/time", 2,
                   [](const String &payload, const size_t size) {
        Serial.print("Получена команда /time/ от ");
        Serial.print(payload);

        mqtt.publish("time/" + getChipID(), payload);
    });
}

