
#ifndef SETUPTASK_H
#define SETUPTASK_H
#include "globalConfig.h"
#include "MutexLock.h"

/**
 * @file setupTasks.h
 * @brief Collection of helper routines that configure hardware peripherals,
 *        spawn FreeRTOS tasks and perform periodic maintenance of network
 *        connections.  Functions are grouped into logical sections to provide
 *        an easy to follow setup sequence for the firmware.
 */

//=========== Объявление функций ===========
void sensorsPolling();
void ticker10secCallback();
void oneMinPolling();
void ticker1minCallback();
extern void processMQTTQueue();

volatile unsigned long lastLoopTime = 0;
const unsigned long loopTimeout = 300000; 
volatile bool dataReadyForSend = false;
volatile bool oneMinCallback = false;


// Задача мониторинга Loop
void monitorLoopTask(void* parameter) {
    for (;;) {
        unsigned long currentTime = millis();

        // Проверяем, было ли обновлено время активности loop
        if ((currentTime - lastLoopTime) > loopTimeout) {
            Serial.println("Loop function seems to be stuck!");
            
            // Логирование состояния перед перезагрузкой
            Serial.println("Текущее время: " + String(currentTime));
            Serial.println("Последнее обновление loop: " + String(lastLoopTime));
            
            // Отложенный перезапуск для завершения логирования
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            //esp_restart(); // Перезагрузка ESP32
        }

        // Задержка мониторинговой задачи (например, 1000 мс)
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}



//=========== Общие функции инициализации ===========
void initializeSerial() {
    Serial.begin(115200);    
}

void initializeSensors() {
    pinMode(buttonPin, INPUT_PULLUP);
}

void initializePreferences() {
    prefs.begin("nvs", true);
    authUrl = prefs.getString("authUrl", "");
    Serial.println(authUrl);
    tokenUrl = prefs.getString("tokenUrl", "");
    Serial.println(tokenUrl);
    mqttUrl = prefs.getString("mqttUrl", "");
    Serial.println(mqttUrl);
    cloudApiUrl = prefs.getString("cloudApiUrl", "");
    Serial.println(cloudApiUrl);
    configUrl = prefs.getString("configUrl", "");
    Serial.println(configUrl);
    accessToken = prefs.getString("accessToken", "");
    Serial.println(accessToken);
    refreshToken = prefs.getString("refreshToken", "");
    Serial.println(refreshToken);
    expiresIn = prefs.getInt("expiresIn");
    prefs.end();
   
   Serial.println(">>>>>>>>>>>>> VERSION FIRMWARE: " + String(versionf));
   Serial.println(">>>>>>>>>>>>> DeviceID: " + getChipID());
   Serial.println(configUrl);
}

void initializeWiFi() {
    WiFi.mode(WIFI_STA);  // Режим вайфай станции
    WiFi.setAutoReconnect(true);
    char ssid[32] = {0};  // Буфер для SSID
    char password[32] = {0};  // Буфер для пароля
    bool credentialsLoaded = bleWiFiConfig.loadWiFiCredentials(ssid, password);
    bleWiFiConfig.connectToWiFi(ssid, password);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
TaskHandle_t readSerialCommandsHandle;
TaskHandle_t prepareForPairingHandle;
TaskHandle_t updateLEDsHandle;

Ticker setTimeTicker;
Ticker ticker1min;
Ticker ticker10sec;

void initializeTasks() {
    mqttMutex = xSemaphoreCreateMutex();
    dataQueueMutex = xSemaphoreCreateMutex();
    
    xTaskCreate(readSerialCommands, "readSerialCommands", 2750, NULL, 2, &readSerialCommandsHandle);
    xTaskCreate(prepareForPairing, "prepareForPairing", 2000, NULL, 1, &prepareForPairingHandle);
    xTaskCreate(updateLEDs, "updateLEDs", 2500, NULL, 1, &updateLEDsHandle);
    //xTaskCreate(processMQTTQueueTask, "MQTTQueueTask", 4096, NULL, 1, NULL);
    //xTaskCreate(monitorLoopTask, "MonitorLoop", 2048, NULL, 1, NULL);
    prefs.begin("nvs", false);
    float pollingInterval = prefs.getFloat("pollingInterval", 10.0);
    prefs.end();
    ticker10sec.attach(pollingInterval, ticker10secCallback); 
    ticker1min.attach(60, ticker1minCallback);
    setTimeTicker.attach(432000, setDateTime);
    WDTWrapper::init(30);

    if (configUrl.length() != 0 && WiFi.status() == WL_CONNECTED) {
    fetchAndStoreConfig(configUrl);  // Вызываем функцию только при наличии подключения
}

    
}

void initializeIndication() {
    smoothLED1.begin();
    smoothLED1.setState(_OFF);
    smoothLED2.begin();
    smoothLED2.setState(_OFF);
        
}

void initializeMQTT() {
    setDateTime();
    mqtt.setKeepAliveTimeout(15);
    mqttclient.beginSslWithCA(mqttUrl.c_str(), 443, "/", cert_bundle, "arduino");
    mqttclient.setReconnectInterval(2000);
    mqttclient.enableHeartbeat(15000, 3000, 3);
    mqtt.begin(mqttclient);
    Serial.println(mqttUrl);
    checkMemory("После initializeMQTT");
}



//=========== Общие функции из Loop ===========

void checkWiFiAndMQTTConnection() {
    static unsigned long lastCheckTime = 0;  // Время последней проверки подключения
    const unsigned long timeout = 300000;   // Таймаут ожидания подключения, 100 секунд
    unsigned long currentTime = millis();

    // Проверяем состояние подключения WiFi и MQTT
    if (WiFi.status() != WL_CONNECTED || !mqtt.isConnected()) {
        if (lastCheckTime == 0) {
            // Запоминаем время начала попыток подключения
            lastCheckTime = currentTime;
        } else if (currentTime - lastCheckTime > timeout) {
            // Если время ожидания истекло
            Serial.println("Не удалось подключиться к WiFi в течение 5 минут, повторная инициализация WiFi...");

            // Отключаем MQTT-клиент
            if (mqtt.isConnected()) {
                mqtt.disconnect();
                Serial.println("MQTT client disconnected.");
            }

          

            // Отключаем WiFi
           if (WiFi.isConnected()) {
                WiFi.disconnect(true);
                Serial.println("WiFi отключен.");
            }
            //vTaskDelay(1000 / portTICK_PERIOD_MS);

            if (!secureClient.connected()) {
            secureClient.stop(); // Остановка перед переинициализацией
            secureClient = WiFiClientSecure();
}


            // Инициализируем WiFi заново
            initializeWiFi();

            // Сбрасываем таймер проверки
            lastCheckTime = currentTime;
            return;
        }
    } else {
        // Если подключены, сбрасываем таймер
        lastCheckTime = 0;
    }
   
}

void updateMQTT() {
//MutexLock lock(mqttMutex);
   mqtt.update();      
}



void handleTokenExchange() {
    if (WiFi.status() == WL_CONNECTED && accessToken.isEmpty()) {
        if (deviceCode.isEmpty()) {
            performDeviceCodeExchange(authUrl, deviceCode, userCode, verificationUrl);
        } else {
            if (accessToken.isEmpty()) {
                performTokenExchange(tokenUrl, accessToken, refreshToken, deviceCode);  
                             
            }
        }        
    }
}

void manageTokenRefresh() {
    static unsigned long lastTokenUpdate = 0;  // Статическая переменная для отслеживания времени последнего обновления токена
    // Проверяем подключение к WiFi и наличие рефреш-токена
    if (WiFi.status() != WL_CONNECTED || refreshToken.isEmpty()) {
        return;
    }

    unsigned long currentTime = millis();
    // Проверка с защитой от переполнения millis()
    if ((long)(currentTime - lastTokenUpdate) >= (expiresIn * 1000L) / 1.1) {
        performTokenUpdate(tokenUrl, accessToken, refreshToken);
        lastTokenUpdate = currentTime;  // Сброс таймера после успешного обновления токенов
        // Serial.println("Token updated.");
    }
}


void handleMQTTConnection() {
    static byte reconnectCount = 0;
    static unsigned long lastCheckTime = 0;
    const unsigned long checkInterval = 5000; // Проверяем подключение каждые 10 секунд
    unsigned long currentMillis = millis();

    // Проверяем наличие accessToken и refreshToken    
    if (accessToken.length() == 0) {
       return; // Пропускаем попытки подключения, если токены отсутствуют
    }

    manageTokenRefresh();

    if (currentMillis - lastCheckTime > checkInterval) {
        lastCheckTime = currentMillis;

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        if (!mqtt.isConnected()) {
            Serial.println("MQTT disconnected. Attempting to reconnect...");            
            reconnect("device-token", accessToken);

            if (mqtt.isConnected()) {
                Serial.println("MQTT reconnected successfully.");                
                reconnectCount = 0;
            } else {
                Serial.println("Reconnect attempt failed.");
                if (reconnectCount >= 2) {
                    Serial.println("Maximum reconnect attempts reached. Updating tokens...");
                    performTokenUpdate(tokenUrl, accessToken, refreshToken);
                    reconnectCount = 0;
                    reconnect("device-token", accessToken); // Попытка переподключения с новыми токенами
                } else {
                    reconnectCount++;
                }
            }
        }
    }
}



void mqttDisconnectTask()
{
    mqtt.disconnect();
    Serial.println("MQTT disconnected by command");
}

void processMQTTQueueTask(void* pvParameters) {
    unsigned long lastMgmtCall = 0;
    for (;;) {
        if (otaInProgress) {
            mqtt.disconnect();
            mqttclient.disconnect();
            vTaskDelete(NULL);
            return;
        }

        unsigned long now = millis();
        // Раз в 15 секунд вызываем "тяжелые" сервисные функции
        if (now - lastMgmtCall >= 20000) {
            handleTokenExchange();
            handleMQTTConnection();
            #ifndef AQUASYNC1
            checkWiFiAndMQTTConnection();
            #endif
            lastMgmtCall = now;
        }

        if (mqtt.isConnected()) {
            updateMQTT();
            processMQTTQueue();
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}


void loopTasks() {
    WDTWrapper::addThisTask(); // Первый вызов — регистрирует задачу loop
    WDTWrapper::reset();       // Сбросить WDT (каждая итерация)
    // Если обновление в процессе, функция не выполняется
    if (otaInProgress) {
        mqtt.disconnect();
        mqttclient.disconnect();
        return;  // Прекращаем выполнение, если запущена задача обновления
    }

    // Выполняем задачи только если обновление не запущено
    #ifndef AQUASYNC1
    checkWiFiAndMQTTConnection();
    #endif // AQUASYNC
    updateMQTT();
    handleTokenExchange();
    handleMQTTConnection();
    processMQTTQueue();

    if (dataReadyForSend) {
        Serial.println("Processing data queue...");
        sensorsPolling();
        dataReadyForSend = false;  // Сбрасываем флаг
    }
    if (oneMinCallback) {
        Serial.println("Processing data queue...");
        oneMinPolling();
        oneMinCallback = false;  // Сбрасываем флаг
    }

    lastLoopTime = millis();


}


//=========== Общие функции для опроса датчиков и отправки в MQTT ===========
void ticker10secCallback() {
    dataReadyForSend = true; // Устанавливаем флаг
}
void ticker1minCallback() {
    oneMinCallback = true; // Устанавливаем флаг
}


void sensorsPolling() { 
if (accessToken.isEmpty()) {
        return;  // Пропускаем опрос датчиков, если токен отсутствует
    }
    sendWifiRSSI();   
    processQueue();    
}


void oneMinPolling() { 
if (accessToken.isEmpty()) {
        return;  // Пропускаем опрос датчиков, если токен отсутствует
    }
}


#endif // SETUPTASK_H