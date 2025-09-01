#ifndef BLE_WIFI_CONFIG_H
#define BLE_WIFI_CONFIG_H

#include "settings.h"
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "SmoothLED.h"

/**
 * @file BLEWiFiConfig.h
 * @brief BLE provisioning helper that accepts Wi‑Fi credentials from a mobile
 *        application and stores them in NVS.
 *
 * The class exposes a minimal GATT service with a single RX characteristic for
 * incoming JSON commands and a TX characteristic for status notifications.  It
 * also owns the global TLS client so that once Wi‑Fi is connected the proper
 * certificate bundle can be configured.
 */

/** UUID of the BLE service used during provisioning. */
extern const char* SERVICE_UUID;
/** UUID of the characteristic through which the phone sends data to the device. */
extern const char* CHARACTERISTIC_UUID_RX;
/** UUID of the characteristic used to notify the phone with responses. */
extern const char* CHARACTERISTIC_UUID_TX;

/** Shared TLS client instance used for secure HTTP communication. */
extern WiFiClientSecure secureClient;

/** Provisioning command handler defined in BLEFunc.cpp. */
void handleRequest(const std::string& data);

/**
 * @class BLEWiFiConfig
 * @brief Handles all actions required to configure Wi‑Fi over BLE.
 */
class BLEWiFiConfig {
public:
    BLEWiFiConfig();

    /** Periodic polling from loop() printing a waiting animation. */
    void loop();

    /** @return true if a Wi‑Fi connection has been successfully established. */
    bool isWiFiConnected();

    /** Remove any stored Wi‑Fi credentials from NVS. */
    void clearWiFiCredentials();

    /** @return true once valid credentials were stored in NVS. */
    bool isConfigured() const;

    /** Parse and react to a JSON message received via BLE. */
    void handleReceivedData(const std::string& data);

    /** Persist Wi‑Fi credentials to the ESP32 Preferences storage. */
    void saveWiFiCredentials(const char* ssid, const char* password);

    /**
     * @brief Load previously stored Wi‑Fi credentials.
     * @param[out] ssid     Buffer that receives the SSID.
     * @param[out] password Buffer that receives the password.
     * @return true if both values were found.
     */
    bool loadWiFiCredentials(char* ssid, char* password);

    /** Send a raw JSON response to the connected phone. */
    void sendResponse(const char* response);
    void sendResponseInChunks(const String& response);
    void sendPeriodicMessage();

    /** Flag that is set when the GET_TOKEN command is received. */
    bool gotGetTokenCommand = false;

    /** Attempt to connect to the specified Wi‑Fi network. */
    void connectToWiFi(const char* ssid, const char* password);

private:
    BLECharacteristic *pRxCharacteristic;   ///< Incoming command characteristic
    BLECharacteristic *pTxCharacteristic;   ///< Outgoing notification characteristic
    BLEServer *pServer;                     ///< Pointer to the NimBLE server
    Preferences preferences;                ///< NVS storage for credentials
    bool deviceConnected = false;           ///< True while a BLE client is connected
    bool wifiConnected = false;             ///< True once Wi‑Fi is up
    bool configured = false;                ///< Tracks successful credential save

    /** Start advertising the provisioning BLE service. */
    void startBLE();

    /** BLE server callbacks used to update connection state flags. */
    class MyServerCallbacks : public BLEServerCallbacks {
    public:
        explicit MyServerCallbacks(BLEWiFiConfig* config) : _config(config) {}
        void onConnect(BLEServer* pServer) override;
        void onDisconnect(BLEServer* pServer) override;
    private:
        BLEWiFiConfig* _config;
    };

    /** Characteristic callbacks that deliver received data to the handler. */
    class MyCallbacks : public BLECharacteristicCallbacks {
    public:
        explicit MyCallbacks(BLEWiFiConfig* config) : _config(config) {}
        void onWrite(BLECharacteristic *pCharacteristic) override;
    private:
        BLEWiFiConfig* _config;
    };

    MyServerCallbacks serverCallbacks;      ///< Instance of server callbacks
    MyCallbacks characteristicCallbacks;    ///< Instance of characteristic callbacks
};

#endif // BLE_WIFI_CONFIG_H

