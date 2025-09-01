/**
 * @file DataQueue.h
 * @brief Thread‑safe containers and helper classes for queuing sensor data
 *        and MQTT messages.
 *
 * Sensor readings collected throughout the application are wrapped in
 * DataItem objects and stored in a global map keyed by parameter name. The
 * queue is later processed and forwarded to the MQTT broker. A separate
 * queue is used for MQTT messages that are waiting to be published.
 */

#pragma once

#include <memory>
#include <map>
#include <queue>
#include <mutex>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "WebSocketsClient.h"
#include "MQTTPubSubClient.h"
#include "MutexLock.h"

// External MQTT client instance (defined in mqttFunc.h)
extern MQTTPubSub::PubSubClient<MQTT_BUFFER_SIZE> mqtt;
extern String getChipID();

/**
 * @brief Abstract base class representing a data item destined for MQTT.
 *
 * Derivatives must provide the parameter name, value and method type. The
 * process() method is invoked when the queue is flushed.
 */
class DataItem {
public:
    virtual ~DataItem() {}

    virtual String getParamName() const = 0;   ///< Return parameter name
    virtual String getParamValue() const = 0;  ///< Return parameter value
    virtual String getMethodType() const = 0;  ///< Return method type

    /// Publish or queue the value. Implemented by derived classes.
    virtual void process() const = 0;
};

// Global map holding DataItem instances indexed by parameter name
extern std::map<String, std::unique_ptr<DataItem>> dataQueue;
extern SemaphoreHandle_t dataQueueMutex;  ///< Protects access to dataQueue
extern SemaphoreHandle_t mqttMutex;       ///< Protects access to MQTT client

/**
 * @brief Simple implementation of DataItem that stores values as strings.
 */
class GenericDataItem : public DataItem {
public:
    String paramName;   ///< MQTT topic or parameter name
    String paramValue;  ///< Value converted to string
    String methodType;  ///< RPC method associated with the value

    GenericDataItem(String name, String value, String method)
        : paramName(name), paramValue(value), methodType(method) {}

    String getParamName() const override { return paramName; }
    String getParamValue() const override { return paramValue; }
    String getMethodType() const override { return methodType; }

    void process() const override {
        if (mqtt.isConnected()) {
            mqtt.publish(paramName.c_str(), paramValue.c_str());
            Serial.printf("Data sent [%s]: %s\n", paramName.c_str(), paramValue.c_str());
        } else {
            Serial.printf("MQTT not connected. Queuing data for [%s]: %s\n",
                          paramName.c_str(), paramValue.c_str());
        }
    }
};

/**
 * @brief Structure representing a pending MQTT publication.
 */
struct MQTTMessage {
    String topic;    ///< MQTT topic
    String payload;  ///< Message payload
    bool retain;     ///< Retain flag
    int qos;         ///< Quality of service level
};

extern std::queue<MQTTMessage> mqttQueue;  ///< FIFO for outgoing messages
extern std::mutex mqttQueueMutex;          ///< Protects access to mqttQueue

// Queue management functions
void processQueue();
void enqueueMQTTMessage(const String& topic, const String& payload,
                        bool retain = false, int qos = 0);
void processMQTTQueue();
