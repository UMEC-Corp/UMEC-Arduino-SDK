#include "BLEWiFiConfig.h"

// ---------------------------------------------------------------------------
// BLE service and characteristic identifiers
// ---------------------------------------------------------------------------
const char* SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const char* CHARACTERISTIC_UUID_RX = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const char* CHARACTERISTIC_UUID_TX = "beb5483e-36e1-4688-b7f5-ea07361b26a9";

BLEWiFiConfig::BLEWiFiConfig() : serverCallbacks(this), characteristicCallbacks(this) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void BLEWiFiConfig::loop() {
    // While the device waits for a BLE client or Wi‑Fi connection we print a
    // simple animated status message.  This keeps the serial console alive and
    // informs the user that provisioning is still pending.
    if (!deviceConnected && !wifiConnected) {
        static uint8_t dots = 0;
        Serial.print("\rWaiting for connection");
        for (uint8_t i = 0; i < dots; i++) {
            Serial.print(".");
        }
        dots = (dots + 1) % 4;
        delay(500);
    }
}

bool BLEWiFiConfig::isWiFiConnected() { return wifiConnected; }

bool BLEWiFiConfig::isConfigured() const { return configured; }

void BLEWiFiConfig::saveWiFiCredentials(const char* ssid, const char* password) {
    // Store credentials only if they actually changed to avoid unnecessary
    // flash writes.
    preferences.begin("wifi-config", false);
    String storedSSID = preferences.getString("ssid", "");
    String storedPassword = preferences.getString("password", "");
    if (storedSSID == ssid && storedPassword == password) {
        Serial.println("WiFi credentials are the same as stored. Skipping save.");
        preferences.end();
        return;
    }

    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    configured = true;
    preferences.end();
}

bool BLEWiFiConfig::loadWiFiCredentials(char* ssid, char* password) {
    // Attempt to load credentials from NVS. Returns true only when both
    // strings are present.
    preferences.begin("wifi-config", true);
    String storedSSID = preferences.getString("ssid", "");
    String storedPassword = preferences.getString("password", "");

    Serial.print("Stored SSID: ");
    Serial.println(storedSSID);
    Serial.print("Stored Password: ");
    Serial.println(storedPassword);

    if (storedSSID.length() > 0 && storedPassword.length() > 0) {
        strncpy(ssid, storedSSID.c_str(), 32);
        strncpy(password, storedPassword.c_str(), 32);
        preferences.end();
        return true;
    }
    preferences.end();
    return false;
}

void BLEWiFiConfig::clearWiFiCredentials() {
    // Erase stored credentials. This is typically triggered by a long button
    // press during factory reset.
    preferences.begin("wifi-config", false);
    preferences.putString("ssid", "");
    preferences.putString("password", "");
    preferences.end();
}

void BLEWiFiConfig::startBLE() {
    // Set up the BLE server and start advertising the provisioning service.
    Serial.println("Starting BLE work!");
    BLEDevice::init("ESP32_BLE");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    BLEService *pService = pServer->createService(SERVICE_UUID);
    pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
    pRxCharacteristic->setCallbacks(&characteristicCallbacks);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        NIMBLE_PROPERTY::NOTIFY);

    pService->start();
    pServer->getAdvertising()->start();
    BLEDevice::setMTU(512);
    Serial.println("Waiting for a client connection to notify...");
}

void BLEWiFiConfig::connectToWiFi(const char* ssid, const char* password) {
    // Try connecting using the provided credentials. If either value is empty
    // we immediately fall back to BLE provisioning mode.
    if (ssid == nullptr || password == nullptr || strlen(ssid) == 0 || strlen(password) == 0) {
        Serial.println("SSID or Password not provided, switching to BLE mode.");
        startBLE();
        return;
    }

    Serial.println("Attempt to connect to WiFi network");
    WiFi.begin(ssid, password);
    int retry = 0;
    int maxRetries = 10;
    int delayInterval = 100;

    while (WiFi.status() != WL_CONNECTED && retry < maxRetries) {
        delay(delayInterval);
        Serial.print("Connecting to WiFi. Attempt " + String(retry + 1) + ": ");
        Serial.println("Status: " + String(WiFi.status()));

        if (retry >= maxRetries - 1) {
            Serial.println("Failed to connect to WiFi after maximum attempts.");
            return; // Do not switch back to BLE automatically.
        }
        retry++;
        delayInterval *= 2; // exponential backoff
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi successfully");
        Serial.println("IP Address: " + WiFi.localIP().toString());
        secureClient.setCACert(cert_bundle);
        wifiConnected = true;
    } else {
        Serial.println("Failed to connect to WiFi. Check settings.");
    }
}

void BLEWiFiConfig::sendResponse(const char* response) {
    // Helper to send a JSON response to the connected phone.
    if (deviceConnected) {
        size_t length = strlen(response);
        uint8_t* byteArray = new uint8_t[length];
        memcpy(byteArray, response, length);

        pTxCharacteristic->setValue(byteArray, length);
        pTxCharacteristic->notify();
        Serial.println("Notification sent");
        delete[] byteArray;
    } else {
        Serial.println("Cannot send notification, device not connected");
    }
}

void BLEWiFiConfig::handleReceivedData(const std::string& data) {
    // Generic handler for commands unrelated to Wi‑Fi provisioning.
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    const char* url = doc["url"];
    if (url) {
        Serial.println(url);
        JsonDocument responseDoc;
        responseDoc["status"] = "success";
        char response[200];
        serializeJson(responseDoc, response);
        sendResponse(response);
    }
}

// ---------------------------------------------------------------------------
// BLE callbacks
// ---------------------------------------------------------------------------

void BLEWiFiConfig::MyServerCallbacks::onConnect(BLEServer* pServer) {
    Serial.println("Device connected");
    _config->deviceConnected = true;
}

void BLEWiFiConfig::MyServerCallbacks::onDisconnect(BLEServer* pServer) {
    Serial.println("Device disconnected");
    _config->deviceConnected = false;
    ESP.restart();
}

void BLEWiFiConfig::MyCallbacks::onWrite(BLECharacteristic *pCharacteristic) {
    // Called whenever the phone sends data to the RX characteristic. The data
    // can either be a JSON command or a simple "ssid:password" pair.
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() == 0) {
        return;
    }

    Serial.println("Received Value:");
    for (size_t i = 0; i < rxValue.length(); i++) {
        Serial.print(rxValue[i]);
    }
    Serial.println();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, rxValue);
    if (!error) {
        if (doc["command"].is<const char*>()) {
            const char* command = doc["command"];
            if (strcmp(command, "GET_TOKEN") == 0) {
                _config->gotGetTokenCommand = true;
                Serial.println("Received GET_TOKEN command");
            }
            handleRequest(rxValue); // Delegate to global handler.
        } else {
            Serial.println("Unknown JSON format");
        }
    } else if (rxValue.find(':') != std::string::npos) {
        // Legacy format "ssid:password" used to quickly provision credentials.
        String ssid = rxValue.substr(0, rxValue.find(':')).c_str();
        String password = rxValue.substr(rxValue.find(':') + 1).c_str();

        _config->saveWiFiCredentials(ssid.c_str(), password.c_str());
        _config->connectToWiFi(ssid.c_str(), password.c_str());

        JsonDocument responseDoc;
        responseDoc["command"] = "SERVER_DATA";
        responseDoc["byteReceived"] = rxValue.length();
        if (_config->isWiFiConnected()) {
            responseDoc["isSuccess"] = true;
            responseDoc["error"] = "";
        } else {
            responseDoc["isSuccess"] = false;
            responseDoc["error"] = "Failed to connect to WiFi";
        }
        char response[200];
        serializeJson(responseDoc, response);
        _config->sendResponse(response);
    } else {
        Serial.println("Received unknown data format");
    }
}

