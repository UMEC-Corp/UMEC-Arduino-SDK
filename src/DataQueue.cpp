/**
 * @file DataQueue.cpp
 * @brief Implementation of sensor data and MQTT message queues.
 */

#include "DataQueue.h"

// Actual storage for global containers declared in the header
std::map<String, std::unique_ptr<DataItem>> dataQueue;
std::queue<MQTTMessage> mqttQueue;
std::mutex mqttQueueMutex;
std::map<String, float> lastSentValues;  ///< last published values per parameter

/**
 * @brief Flush accumulated sensor data to the MQTT message queue.
 *
 * The function groups parameters by RPC method and decides whether a method
 * should be sent based on the difference between new and previously sent
 * values. Only changed parameters are published to reduce traffic.
 */
void processQueue() {
    static std::map<String, bool> methodShouldBeSent; // send flag per method

    if (!mqtt.isConnected()) {
        Serial.println("MQTT not connected. Queue will not be processed.");
        return;
    }

    // Transfer ownership of queued items into a local container
    std::map<String, std::unique_ptr<DataItem>> localQueue;
    {
        MutexLock lock(dataQueueMutex);
        if (dataQueue.empty()) {
            return;
        }
        localQueue = std::move(dataQueue);
    }

    // Step 1: group parameters by method and determine if method needs sending
    std::map<String, std::vector<std::pair<String, float>>> methodParams;

    for (const auto& item : localQueue) {
        String methodType = item.second->getMethodType();
        String paramName  = item.second->getParamName();
        float newValue    = item.second->getParamValue().toFloat();

        if (methodType.isEmpty() || paramName.isEmpty()) {
            Serial.println("Error: Method type or parameter name is empty, skipping this item.");
            continue;
        }

        if (methodShouldBeSent.find(methodType) == methodShouldBeSent.end()) {
            methodShouldBeSent[methodType] = false;
        }

        methodParams[methodType].push_back({paramName, newValue});

        // Compare with last sent value to decide on sending
        bool wasSent = (lastSentValues.find(paramName) != lastSentValues.end());
        if (wasSent) {
            float lastValue = lastSentValues[paramName];
            if (paramName.startsWith("current") || paramName.startsWith("flow")) {
                if (abs(newValue - lastValue) >= 0.3) {
                    methodShouldBeSent[methodType] = true;
                }
            } else if (paramName.startsWith("voltage")) {
                if (abs(newValue - lastValue) >= 5.0) {
                    methodShouldBeSent[methodType] = true;
                }
            } else if (paramName.startsWith("power")) {
                float change = (abs(newValue - lastValue) / lastValue) * 100.0;
                if (change >= 5.0) {
                    methodShouldBeSent[methodType] = true;
                }
            } else if (paramName.startsWith("energy")) {
                if (abs(newValue - lastValue) >= 100.0) {
                    methodShouldBeSent[methodType] = true;
                }
            } else if (paramName.startsWith("vbat")) {
                if (abs(newValue - lastValue) >= 0.2) {
                    methodShouldBeSent[methodType] = true;
                }
            } else if (paramName.startsWith("gauge")) {
                if (abs(newValue - lastValue) >= 0.1) {
                    methodShouldBeSent[methodType] = true;
                }
            } else if (paramName == "r00") {
                if (abs(newValue - lastValue) >= 3.0) {
                    methodShouldBeSent[methodType] = true;
                }
            } else if (paramName.startsWith("t0") ||
                       paramName.startsWith("target-temp") ||
                       paramName.startsWith("schedule-status")) {
                if (abs(newValue - lastValue) >= 1.0) {
                    methodShouldBeSent[methodType] = true;
                }
            } else if (paramName.startsWith("cons")) {
                if (abs(newValue - lastValue) >= 1.0) {
                    methodShouldBeSent[methodType] = true;
                }
            } else if (paramName.startsWith("active") ||
                       paramName.startsWith("leak") ||
                       paramName.startsWith("plugged") ||
                       paramName.startsWith("methane")) {
                if (newValue != lastValue) {
                    methodShouldBeSent[methodType] = true;
                }
            } else {
                float change = (abs(newValue - lastValue) / lastValue) * 100.0;
                if (change >= 5.0) {
                    methodShouldBeSent[methodType] = true;
                }
            }
        } else {
            // Parameter never sent before - send full method
            methodShouldBeSent[methodType] = true;
        }
    }

    // Step 2: create messages for methods that require sending
    for (auto& methodIter : methodParams) {
        const String& method = methodIter.first;
        auto& paramList = methodIter.second;

        if (methodShouldBeSent[method]) {
            String jsonMessage = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":{";

            bool firstParam = true;
            for (auto& kv : paramList) {
                const String& paramName = kv.first;
                float paramValue = kv.second;

                if (!firstParam) {
                    jsonMessage += ",";
                }
                jsonMessage += "\"" + paramName + "\":{\"value\":" + String(paramValue, 2) + "}";
                firstParam = false;
            }
            jsonMessage += "}}";

            String topic = "stream/" + String(getChipID()) + "/rpcout";
            enqueueMQTTMessage(topic, jsonMessage, false, 0);

            for (auto& kv : paramList) {
                lastSentValues[kv.first] = kv.second;
            }

            methodShouldBeSent[method] = false;
        }
    }

    // Step 3: clear local copy
    localQueue.clear();
}

/**
 * @brief Add an MQTT message to the queue.
 */
void enqueueMQTTMessage(const String& topic, const String& payload, bool retain, int qos) {
    std::lock_guard<std::mutex> lock(mqttQueueMutex);

    if (mqttQueue.size() >= 10) {
        Serial.println("Queue full. Dropping oldest message.");
        mqttQueue.pop();
    }

    mqttQueue.push({topic, payload, retain, qos});
}

/**
 * @brief Publish one message from the MQTT queue if the client is connected.
 */
void processMQTTQueue() {
    if (!mqtt.isConnected()) {
        return;
    }

    MQTTMessage message;
    {
        std::lock_guard<std::mutex> lock(mqttQueueMutex);
        if (!mqttQueue.empty()) {
            message = mqttQueue.front();
            mqttQueue.pop();
        } else {
            return;
        }
    }

    MutexLock lock(mqttMutex);
    bool publishResult = mqtt.publish(message.topic.c_str(), message.payload.c_str(), message.retain, message.qos);
    if (!publishResult) {
        Serial.printf("MQTT Publish Failed: Topic: %s, Payload: %s\n", message.topic.c_str(), message.payload.c_str());
        enqueueMQTTMessage(message.topic, message.payload, message.retain, message.qos);
    } else {
        Serial.printf("MQTT Publish Success: Topic: %s, Payload: %s\n", message.topic.c_str(), message.payload.c_str());
    }
}

