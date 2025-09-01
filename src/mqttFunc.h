#pragma once

//************************************************************************
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

#include "globalConfig.h"
extern void buttonTaskDelete();
void saveTimeToNVS(time_t currentTime);
void restoreTimeFromRTC();
bool otaInProgress = false;

/**
 * @file mqttFunc.h
 * @brief Core networking helpers including OTA update logic, NTP time
 *        synchronisation and MQTT connection management.  The functions here
 *        are shared between different firmware flavours.
 */


enum update_result_t
{
  UPDATE_OK,
  UPDATE_NONE,
  UPDATE_BADURL,
  UPDATE_NO_SPACE,
  UPDATE_FAIL
};

typedef void (*UpdateProgressCallback)(int);

static bool updateRunning = false;    // CHANGE: флаг, что Update уже начат
static int totalLength = 0;          // CHANGE: общий размер прошивки
static int completedLength = 0;      // CHANGE: сколько байт уже записали

// ...

/**
 * @brief Download and apply a firmware update from the given URL.
 *        The function supports resuming partial downloads and reports progress
 *        through a user supplied callback.
 */
update_result_t doOTAUpdate(const char *firmwareUrl, UpdateProgressCallback progressCallback)
{
    otaInProgress = true;

    // Сбросим attempts на каждый вызов:
    int attempts = 0;
    const int maxAttempts = 20;

    if (!firmwareUrl) return UPDATE_BADURL;

    HTTPClient http;

    // Цикл по количеству попыток, не привязываемся к totalLength == 0
    while (attempts < maxAttempts) {
        http.begin(firmwareUrl);
        http.setTimeout(30000);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        if (updateRunning && completedLength > 0) {
            // Уже качали раньше, докачиваем
            String rangeHeader = "bytes=" + String(completedLength) + "-";
            http.addHeader("Range", rangeHeader.c_str());
            Serial.printf("[OTA] Resuming from byte %d\n", completedLength);
        }

        int httpCode = http.GET();
        Serial.printf("[OTA] Attempt %d, HTTP code: %d\n", attempts + 1, httpCode);

        // Если ещё не начинали Update, проверяем HTTP 200/206 и инициализируем всё
        if (!updateRunning) {
            // Проверяем успешный код
            if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_PARTIAL_CONTENT) {
                http.end();
                attempts++;
                delay(3000);
                continue;
            }
            // Узнаём общий размер
            totalLength = http.getSize();
            Serial.printf("[OTA] totalLength: %d\n", totalLength);

            // Стартуем Update один раз
            if (!Update.begin(totalLength, U_FLASH)) {
                http.end();
                return UPDATE_NO_SPACE;
            }
            updateRunning = true; 
            completedLength = 0; 
        } else {
            // Update уже идёт, должны получить 200 или 206
            if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_PARTIAL_CONTENT) {
                http.end();
                attempts++;
                delay(3000);
                continue;
            }
        }

        // Получаем поток данных
        WiFiClient *client = http.getStreamPtr();
        uint8_t buff[1024];

        while (http.connected()) {
            int len = client->readBytes(buff, sizeof(buff));
            if (len <= 0) break;

            size_t written = Update.write(buff, len);
            if (written != (size_t)len) {
                Serial.println("[OTA] Write error; not aborting. Will try again.");
                // Не вызывать Update.abort(), чтобы не терять уже записанное
                break;
            }

            completedLength += len;
            if (progressCallback && totalLength > 0) {
                int percent = (completedLength * 100) / totalLength;
                progressCallback(percent);
            }
            vTaskDelay(1);
        }
        http.end();

        // Проверяем, докачали ли полностью
        if (completedLength >= totalLength) {
            Serial.println("[OTA] Download completed. Calling Update.end()");
            if (!Update.end(true)) {
                Serial.println("[OTA] Update.end() failed!");
                return UPDATE_FAIL;
            }
            Serial.println("[OTA] Update successful, restarting...");
            ESP.restart();
            return UPDATE_OK; 
        }

        // Иначе не докачали — попробуем ещё раз
        attempts++;
        delay(3000);
    }

    // Если вышли, значит не успели докачать
    Serial.println("[OTA] Failed after all attempts.");

    // Если хочешь очистить раздел при полном фейле — сотри его тут
    // (но тогда весь докачанный кусок потеряется)
    //
    // const esp_partition_t* part = esp_ota_get_next_update_partition(NULL);
    // if (part) {
    //     esp_partition_erase_range(part, 0, part->size);
    //     Serial.println("[OTA] Erased OTA partition.");
    // }
    // updateRunning = false; 
    // totalLength = 0;
    // completedLength = 0;

    return UPDATE_FAIL;
}




//*****************************************************************************



#include <time.h>
#include "TZ.h"
#include "cert.h"

// Объявление функций из mqttProcess.h. Можно дописывать любые другие функции
extern void subscribeTo();
extern void subscribeToTimeExchange();

#define MQTT_CONNECTION_TIMEOUT -4
#define MQTT_CONNECTION_LOST -3
#define MQTT_CONNECT_FAILED -2
#define MQTT_DISCONNECTED -1
#define MQTT_CONNECTED 0
#define MQTT_CONNECT_BAD_PROTOCOL 1
#define MQTT_CONNECT_BAD_CLIENT_ID 2
#define MQTT_CONNECT_UNAVAILABLE 3
#define MQTT_CONNECT_BAD_CREDENTIALS 4
#define MQTT_CONNECT_UNAUTHORIZED 5

WebSocketsClient mqttclient;
MQTTPubSub::PubSubClient<MQTT_BUFFER_SIZE> mqtt;
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (1024)
char msg[MSG_BUFFER_SIZE];


void setDateTime() {
    const int NTP_TIMEOUT = 15; // Тайм-аут в секундах
    const char* ntpServers[] = {
    "time.nist.gov",            // USA
    "pool.ntp.org",             // Global
    "time.windows.com",         // USA
    "europe.pool.ntp.org",      // Europe
    "asia.pool.ntp.org",        // Asia
    "oceania.pool.ntp.org",     // Oceania
    "time.google.com",          // Global
    "time.cloudflare.com",      // Global
    "time.apple.com",           // USA
    "ru.pool.ntp.org",          // Russia
    "ntp1.stratum2.ru",         // Russia
    "ntp2.stratum2.ru",         // Russia
    "ntp3.stratum2.ru",         // Russia
    "ntp1.gldn.net",            // Russia (Golden Telecom)
    "ntp2.gldn.net",            // Russia (Golden Telecom)
    "ntp3.gldn.net",            // Russia (Golden Telecom)
    "de.pool.ntp.org",          // Germany
    "fr.pool.ntp.org",          // France
    "uk.pool.ntp.org",          // UK
    "jp.pool.ntp.org"           // Japan
};
    const int numServers = sizeof(ntpServers) / sizeof(ntpServers[0]);
    bool timeSynced = false;

    // Проверка подключения к WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi is not connected. Cannot sync time.");
        restoreTimeFromRTC();  // Пытаемся восстановить время из NVS, если нет WiFi
        return;
    }

    // Параллельный опрос NTP серверов
    for (int i = 0; i < numServers; i++) {
        configTime(0, 0, ntpServers[i]);
        Serial.printf("Trying NTP server: %s\n", ntpServers[i]);
    }

    time_t now = time(nullptr);
    time_t startTime = millis() / 1000; // Время начала в секундах

    while (now < 8 * 3600 * 2 && (millis() / 1000 - startTime < NTP_TIMEOUT)) {
        delay(200); // Увеличение задержки для уменьшения нагрузки
        Serial.print(".");
        now = time(nullptr);
    }

    if (now >= 8 * 3600 * 2) {
        Serial.println("\nNTP time sync successful.");
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);
        Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
        timeSynced = true; // Успешная синхронизация

        // Сохранение метки времени в NVS (через Preferences)
        saveTimeToNVS(now);
    } else {
        Serial.println("\nNTP time sync failed. Restoring time from RTC if available.");
        restoreTimeFromRTC();  // Если не удалось синхронизироваться, восстанавливаем время из NVS
    }
    
    if (!timeSynced) {
        Serial.println("All NTP servers failed to sync time.");
    }
}

// Функция для сохранения метки времени в NVS через Preferences
/**
 * @brief Persist the current UNIX time and millis offset so that the clock can
 *        be restored after a reboot without immediate NTP access.
 */
void saveTimeToNVS(time_t currentTime) {
    prefs.begin("time", false);
    prefs.putULong("saved_time", currentTime);
    prefs.putULong("rtc_offset", millis() / 1000);
    prefs.end();
}

/**
 * @brief Restore system time from values previously stored by saveTimeToNVS().
 */
void restoreTimeFromRTC() {
    prefs.begin("time", true);
    unsigned long saved_time = prefs.getULong("saved_time", 0);
    unsigned long rtc_offset = prefs.getULong("rtc_offset", 0);

    if (saved_time > 0 && rtc_offset > 0) {
        time_t restoredTime = saved_time + (millis() / 1000 - rtc_offset);
        struct timeval tv = { restoredTime, 0 };
        settimeofday(&tv, NULL);
        Serial.printf("Time restored from NVS: %ld\n", restoredTime);
    } else {
        Serial.println("No valid time found in NVS.");
    }

    prefs.end();
}


/**
 * @brief Throttled progress callback used during OTA updates.
 *        Prints the completion percentage at most once every three seconds to
 *        avoid cluttering the serial output.
 */
void updateProgress(int percentage)
{
  static unsigned long lastCallbackTime = 0;
  if (millis() - lastCallbackTime >= 3000)
  {
    Serial.print("Процент обновления: ");
    Serial.println(percentage);
    lastCallbackTime = millis();
  }
}

/**
 * @brief FreeRTOS task wrapper around the OTA update procedure. Retries the
 *        download a few times and restarts the device on persistent failure.
 */
void otaTask(void *pvParameters) {
    if (pvParameters == NULL) {
        Serial.println("OTA task called with NULL parameters");
        vTaskDelete(NULL);
        return;
    }

    String urlParam = *(String*) pvParameters;  // Преобразуем pvParameters обратно в String
    Serial.println("Запуск OTA с URL: " + urlParam);

    int maxAttempts = 5;  // Максимальное количество попыток обновления
    int attempt = 0;
    update_result_t result;

    while (attempt < maxAttempts) {
        Serial.printf("Попытка обновления номер %d, URL: %s\n", attempt + 1, urlParam.c_str());
        result = doOTAUpdate(urlParam.c_str(), [](int percentage) {
            updateProgress(percentage);
        });
        Serial.print("Результат обновления OTA: ");
        Serial.println(result);

        if (result == UPDATE_OK) {
            Serial.println("Обновление прошло успешно.");
            break;
        } else {
            Serial.println("Обновление не удалось, планирование следующей попытки...");
            attempt++;
            for (int i = 0; i < 50; i++) {
                vTaskDelay(100 / portTICK_PERIOD_MS); // 100 мс
            }
        }
    }

    if (result != UPDATE_OK && attempt == maxAttempts) {
        Serial.println("Критическая ошибка: Обновление не удалось после всех попыток.");
        ESP.restart();
    }

    vTaskDelete(NULL);  // Завершить задачу после выполнения
}
///
void reconnect(String mqttL, String mqttP) {
 
    if (!mqtt.isConnected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "ESP32#" + getChipID();

        // Попытка подключения
        if (mqtt.connect(clientId.c_str(), mqttL.c_str(), mqttP.c_str())) {
            Serial.println("MQTT connected");
            //mqtt.publish("stream/" + getChipID() + "/version", versionf);
            String topic = "stream/" + getChipID() + "/version";
            String payload = versionf;

            // Добавляем сообщение в очередь MQTT
            enqueueMQTTMessage(topic, payload, false, 0);
            //Serial.printf("Enqueued MQTT version message: Topic: %s, Payload: %s\n", topic.c_str(), payload.c_str());
            
            // Вызов функций из mqttProcess.h
            subscribeTo();
            subscribeToTimeExchange();

            // Подписка на команды обновления
            mqtt.subscribe("command/" + getChipID() + "/upgrade", 1, [](const String &payload, const size_t size) {
                static String url = payload;  // Сохраняем URL в статической переменной для доступности в задаче
                buttonTaskDelete();
                xTaskCreate(otaTask, "OTA Update", 10000, &url, 4, NULL);  // Создаем задачу с высоким приоритетом
            });
            // Подписка на команду /restart
            mqtt.subscribe("command/" + getChipID() + "/reboot", 1, [](const String &payload, const size_t size) {
                Serial.println("Received /restart command. Restarting ESP...");
                delay(1000);  // Небольшая задержка для выполнения всех операций
                ESP.restart();  // Перезагрузка ESP
            });

            // Подписка на команду /reset
            mqtt.subscribe("command/" + getChipID() + "/reset", 1, [](const String &payload, const size_t size) {
                Serial.println("Received /reset command. Performing full reset...");

                // Очистка всех пространств NVS через Preferences
                Preferences prefs;
                Serial.println("Clearing all NVS namespaces...");
                
                const char* namespaces[] = {"sensors", "nvs", "wifi-config"};  // Укажи здесь свои пространства
                for (const char* ns : namespaces) {
                    prefs.begin(ns, false);  // Открываем пространство для записи
                    prefs.clear();            // Очищаем все ключи
                    prefs.end();              // Закрываем пространство
                    Serial.printf("Cleared NVS namespace: %s\n", ns);
                }

                Serial.println("Reset complete. Restarting ESP...");
                delay(1000);  // Небольшая задержка для завершения операций
                ESP.restart();  // Перезагрузка после очистки
            });   
        } else {
            Serial.print("Failed to connect, rc = ");
            Serial.println(mqtt.getReturnCode());
        }
    }
  
}
