/**
 * @file BLEFunc.cpp
 * @brief Implements parsing of BLE JSON commands used during initial device setup.
 */

#include "BLEFunc.h"
#include "utilities.h"

// Firmware metadata defined in main.cpp
extern const char versionf[];     ///< Firmware version string
extern const char model_code[];   ///< Short model identifier
extern const char* hw_code;       ///< Hardware revision code

/**
 * @brief Utility helper used by legacy code to strip protocol/suffix from URLs.
 *
 * The current implementation merely removes the "wss://" prefix and trailing
 * "/broker" suffix if present. The function is kept for backwards compatibility
 * and may be removed in future revisions.
 */
String processUrl(String url) {
    // Remove "wss://" prefix if the URL contains it
    if (url.startsWith("wss://")) {
        url.remove(0, 6);
    }
    // Remove trailing "/broker" suffix if present
    if (url.endsWith("/broker")) {
        url.remove(url.length() - 7, 7);
    }
    return url;
}

/**
 * @brief Handle a JSON command received from the companion mobile application.
 *
 * The function decodes the incoming JSON, updates internal state and responds
 * back over BLE with the result of the operation. Supported commands are:
 *
 *  - SERVER_DATA: store server URLs in NVS
 *  - GET_TOKEN:   return user/verification codes used for OAuth pairing
 *  - CHECK_AUTH:  verify that an access token has been received and initialize
 *                 the MQTT subsystem
 *
 * @param data Raw JSON string from the mobile application.
 */
void handleRequest(const std::string& data) {
    Serial.println("handleRequest called with data: " + String(data.c_str()));

    JsonDocument jsonDocument;      // parsed command
    JsonDocument responseDocument;  // response we send back

    DeserializationError error = deserializeJson(jsonDocument, data);
    if (!error) {
        const char* _command = jsonDocument["command"];
        String command = String(_command);

        // Prepare default response fields
        responseDocument["command"] = command;
        responseDocument["isSuccess"] = false;
        responseDocument["error"] = "Unknown command";
        responseDocument["byteReceived"] = data.length();

        Serial.println("Command received: " + command);

        if (command == "SERVER_DATA") {
            // Extract endpoints required for further communication
            authUrl = jsonDocument["data"]["authUrl"].as<String>();
            tokenUrl = jsonDocument["data"]["tokenUrl"].as<String>();
            mqttUrl = jsonDocument["data"]["mqttUrl"].as<String>();
            cloudApiUrl = jsonDocument["data"]["cloudApiUrl"].as<String>();
            configUrl = jsonDocument["data"]["configUrl"].as<String>();

            // Persist server URLs in non‑volatile storage
            prefs.begin("nvs", false);
            prefs.putString("authUrl", authUrl);
            prefs.putString("tokenUrl", tokenUrl);
            prefs.putString("mqttUrl", mqttUrl);
            prefs.putString("cloudApiUrl", cloudApiUrl);
            prefs.putString("configUrl", configUrl);
            prefs.end();

            responseDocument["isSuccess"] = true;
            responseDocument["error"] = "";
        } else if (command == "GET_TOKEN") {
            // Wait for OAuth device-code exchange to populate variables
            unsigned long startTime = millis();
            const unsigned long timeout = 20000; // 20 seconds

            while (userCode.length() == 0 || verificationUrl.length() == 0) {
                if (millis() - startTime >= timeout) {
                    responseDocument["isSuccess"] = false;
                    responseDocument["error"] = "Timeout waiting for data!";
                    break;
                }
                delay(100);
            }

            if (userCode.length() != 0 && verificationUrl.length() != 0) {
                responseDocument["isSuccess"] = true;
                responseDocument["error"] = "";

                JsonObject dataObject = responseDocument["data"].to<JsonObject>();
                dataObject["chip_id"] = getChipID();
                dataObject["mac_address"] = WiFi.macAddress();
                dataObject["user_code"] = userCode;
                dataObject["verification_uri"] = verificationUrl;
                dataObject["model_code"] = String(model_code);
                dataObject["vendor_code"] = "vendor";
                dataObject["version_code"] = String(versionf);
                dataObject["hw_code"] = String(hw_code);
            }
        } else if (command == "CHECK_AUTH") {
            // Ensure access token is available before proceeding
            unsigned long startTime = millis();
            const unsigned long timeout = 20000; // 20 seconds

            while (accessToken.length() == 0) {
                if (millis() - startTime >= timeout) {
                    responseDocument["isSuccess"] = false;
                    responseDocument["error"] = "Timeout waiting for access token!";
                    break;
                }
                delay(100);
            }

            if (accessToken.length() != 0) {
                responseDocument["isSuccess"] = true;
                responseDocument["error"] = "";
                initializeMQTT();
            } else {
                responseDocument["isSuccess"] = false;
                responseDocument["error"] = "Can't get access token!";
            }
        }
    } else {
        Serial.print("Error parsing JSON: ");
        Serial.println(error.c_str());
    }

    // Serialize and send response back to the mobile application
    String response;
    serializeJson(responseDocument, response);
    Serial.println("Send data to mobile: " + response);
    bleWiFiConfig.sendResponse(response.c_str());
}


