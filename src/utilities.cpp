    
#include "utilities.h"
#include "globalConfig.h"

/**
 * @brief Return the device's unique chip identifier as a hexadecimal string.
 */
String getChipID() {
  uint8_t mac[6];      // Buffer for MAC address bytes
  char chipIdStr[13];  // String buffer for MAC in hex without separators

  // Obtain MAC address and store in array
  esp_efuse_mac_get_default(mac);

  // Convert MAC address into hexadecimal string
  snprintf(chipIdStr, sizeof(chipIdStr), "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  return chipIdStr;
}

/**
 * @brief FreeRTOS task that listens for commands on the serial port.
 */
void readSerialCommands(void* pvParameters) {
    while (true) {
        if (Serial.available() > 0) {
            String command = Serial.readStringUntil('\n');
            command.trim();
            processCommand(command);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Handle a single command entered over the serial console.
 */
void processCommand(const String& command) {
    if (command == "rst") {
        Serial.println("Restarting...");
        delay(100);
        ESP.restart();
    } else if (command == "help") {
        Serial.println("Available commands:");
        Serial.println("rst - Restart the device");
        Serial.println("km - Disconnect MQTT");
        Serial.println("rm - Reconnect to WiFi and MQTT");
        Serial.println("ka - Clear access token");
        Serial.println("cln - Clean NVS data and restart for pairing");
        Serial.println("sr - Send response test");
    } else if (command == "km") {
        mqttDisconnectTask();
        Serial.println("MQTT disconnected");
    } else if (command == "sr") {
        sendNvsSuccessResponse(1234567890);
        Serial.println("send Response Test");
    } else if (command == "rm") {
        checkWiFiAndMQTTConnection();
        Serial.println("Checking WiFi and MQTT connection");
    } else if (command == "ka") {
        prefs.begin("nvs", false);
        prefs.putString("accessToken", "692DBF7C3763793FE3F0563A3B3F5C7CF7EF5ECF370A9B54DC0625A38B960F42");
        prefs.end();
        accessToken = "692DBF7C3763793FE3F0563A3B3F5C7CF7EF5ECF370A9B54DC0625A38B960F42";
        Serial.println("Access token cleared");
    } else if (command == "cln") {
        prefs.begin("nvs", false);
        prefs.putString("accessToken", "");
        prefs.putString("refreshToken", "");
        prefs.putString("authUrl", "");
        prefs.putString("mqttUrl", "");
        prefs.putString("tokenUrl", "");
        prefs.end();

        WiFi.disconnect(false, true);
        preferences.begin("wifi-config", false);
        preferences.putString("ssid", "");
        preferences.putString("password", "");
        preferences.end();
        delay(300);
        ESP.restart();
    } else {
        Serial.println("Unknown command");
    }
}


const char *stringToConstChar(String str)
{
  char *buffer = (char *)malloc(str.length() + 1); // выделение памяти
  if (buffer == NULL)
    return NULL;                             // проверка на успешное выделение памяти
  str.toCharArray(buffer, str.length() + 1); // копирование данных из String в массив символов
  return buffer;                             // возвращение указателя на массив символов
}


//*****************
//Проверка свободной памяти 
void checkMemory(const char* stage) {
    Serial.print(stage);
    Serial.print(": Free memory = ");
    Serial.println(ESP.getFreeHeap());
}

// Определение глобального массива для состояния портов
uint8_t portStates[2] = {0};  // 2 байта для 16 пинов

// Функция для обновления состояния порта и установки конфигурации портов на ВЫХОД
void updatePortExpander(uint8_t reg, uint8_t value) {
    // 1. Принудительно настраиваем ВСЕ пины как выходы
    Wire.beginTransmission(0x20);
    Wire.write(0x06);  // Регистр конфигурации Port 0 (0x06)
    Wire.write(0x00);  // Все пины Port 0 — выходы
    Wire.write(0x00);  // Все пины Port 1 — выходы (регистр 0x07)
    Wire.endTransmission();

    // 2. Обновляем выходной регистр
    Wire.beginTransmission(0x20);
    Wire.write(reg);   // Указываем регистр (0x02 или 0x03)
    Wire.write(value); // Записываем новое состояние
    Wire.endTransmission();
}


// Проверка на наличие устройства по адресу I2C_EXPANDER_ADDR
void setupPortExpander() {
    Wire.beginTransmission(0x20);
    if (Wire.endTransmission() != 0) {
        Serial.println("❌ PCA9555 не найден! Пропускаем инициализацию.");
        return;
    }

    Serial.println("✅ PCA9555 найден! Инициализируем...");

    // Инициализируем все пины как выходы
    Wire.beginTransmission(0x20);
    Wire.write(0x06);  // Регистр конфигурации порта 0 (устанавливаем как выходы)
    Wire.write(0x00);  // Все пины порта 0 - выходы
    Wire.endTransmission();

    Wire.beginTransmission(0x20);
    Wire.write(0x07);  // Регистр конфигурации порта 1 (устанавливаем как выходы)
    Wire.write(0x00);  // Все пины порта 1 - выходы
    Wire.endTransmission();

    // Устанавливаем все реле в выключенное состояние (LOW)
    Wire.beginTransmission(0x20);
    Wire.write(0x02);  // Регистр выхода порта 0
    Wire.write(0x00);  // Все пины в LOW
    Wire.endTransmission();

    Wire.beginTransmission(0x20);
    Wire.write(0x03);  // Регистр выхода порта 1
    Wire.write(0x00);  // Все пины в LOW
    Wire.endTransmission();

    Serial.println("⚡ PCA9555 полностью инициализирован.");
}




//Функция ддя проверки мультиплексора перед записью состояния регистров.
bool checkPCA9555() {
    Wire.beginTransmission(0x20);
    if (Wire.endTransmission() == 0) {
        return true; // PCA9555 отвечает
    }

    Serial.println("⚠ PCA9555 завис! Попытка перезапуска...");
    resetPCA9555();

    // Проверяем ещё раз после перезапуска
    Wire.beginTransmission(0x20);
    if (Wire.endTransmission() == 0) {
        Serial.println("✅ PCA9555 восстановился!");
        return true;
    }

    Serial.println("⛔ Критическая ошибка: PCA9555 НЕ восстановился!");
    return false;
}

// Функция для пперезаписи регистров мультиплексора
void resetPCA9555() {
    Serial.println("♻ Перезапуск PCA9555...");

    // Перезапуск шины I2C
    Wire.end();
    delay(50);
    Wire.begin(19, 21);
    Wire.setClock(50000); // Можно менять на нужную скорость

    // Проверяем, восстановился ли PCA9555
    Wire.beginTransmission(0x20);
    if (Wire.endTransmission() != 0) {
        Serial.println("⛔ Ошибка: PCA9555 НЕ восстановился после перезапуска!");
        return;
    }

    // Если восстановился, инициализируем заново
    Serial.println("✅ PCA9555 снова работает! Перезапускаем конфигурацию...");
    setupPortExpander();

    Serial.println("✅ PCA9555 успешно перезапущен и настроен!");
}




/*void sendNvsSuccessResponse(int64_t id) {
    if (id == 0) return;  // Если ID равен 0, не отправляем ответ.

    JsonDocument doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = id;
    //doc["result"] = JsonObject();  // Пустой объект в поле "result"
    doc["result"] = "success";
    String response;
    serializeJson(doc, response);
    Serial.println(response);

    // Используем параметр для отправки через очередь
    String paramName = "code";  // Упрощаем имя параметра
    MutexLock lock(dataQueueMutex);
    dataQueue[paramName] = std::unique_ptr<DataItem>(new GenericDataItem(paramName, response, "success"));
    Serial.printf("Queue: Queued success response: Param: %s, Message: %s\n", paramName.c_str(), response.c_str());
}*/


void sendNvsSuccessResponse(int64_t id) {
    static int64_t lastSentId = 0;  // Хранит последний отправленный ID
    if (id == 0 || id == lastSentId) return;  // Пропускаем, если ID равен 0 или уже отправлен

    lastSentId = id;  // Обновляем последний ID

    // Строка-шаблон JSON
    static String jsonTemplate = "{\"jsonrpc\":\"2.0\",\"id\":ID_PLACEHOLDER,\"result\":\"success\"}";

    // Формируем JSON
    String response = jsonTemplate;
    response.replace("ID_PLACEHOLDER", String(id));
    Serial.printf("Generated JSON: %s\n", response.c_str());

    // Формируем MQTT-топик
    String topic = "stream/" + String(getChipID()) + "/rpcout";
    Serial.printf("MQTT Topic: %s\n", topic.c_str());

    // Добавляем сообщение в очередь MQTT
    enqueueMQTTMessage(topic, response, false, 0);
    //Serial.println("NVS Success Response enqueued for MQTT processing.");
}

void sendErrorResponse(int64_t id, const char* errorMessage) {
    static int64_t lastSentId = 0;  // Хранит последний отправленный ID
    if (id == 0 || id == lastSentId) return;  // Пропускаем, если ID равен 0 или уже отправлен

    lastSentId = id;  // Обновляем последний ID

    // Строка-шаблон JSON с двумя плейсхолдерами
    static String jsonTemplate = "{\"jsonrpc\":\"2.0\",\"id\":ID_PLACEHOLDER,\"error\":{\"code\":\"codeError\",\"message\":\"MESSAGE_PLACEHOLDER\"}}";

    // Формируем JSON, заменяя плейсхолдеры
    String response = jsonTemplate;
    response.replace("ID_PLACEHOLDER", String(id));
    response.replace("MESSAGE_PLACEHOLDER", String(errorMessage));
    Serial.printf("Generated JSON: %s\n", response.c_str());

    // Формируем MQTT-топик
    String topic = "stream/" + String(getChipID()) + "/rpcout";
    Serial.printf("MQTT Topic: %s\n", topic.c_str());

    // Добавляем сообщение в очередь MQTT
    enqueueMQTTMessage(topic, response, false, 0);
    //Serial.println("Error Response enqueued for MQTT processing.");
}


int extractIndex(const char* target) {
    int len = strlen(target);
    int startIndex = -1;

    // Ищем первую цифру
    for (int i = 0; i < len; i++) {
        if (isdigit(target[i])) {
            startIndex = i;
            break;
        }
    }

    // Если цифра найдена, конвертируем оставшуюся часть строки в число
    if (startIndex != -1) {
        return atoi(&target[startIndex]) - 1;  // Вычитаем 1, так как индексация с 0
    }

    return -1;  // Если цифры не найдено, возвращаем -1
}



#include <LittleFS.h>
#define FORMAT_LITTLEFS_IF_FAILED true

void initFileSystem() {
    // Пытаемся смонтировать файловую систему
    if (!LittleFS.begin(false)) {  // false, чтобы избежать автоматического форматирования
        Serial.println("Не удалось смонтировать файловую систему, пробуем форматировать...");

        // Попытка форматирования
        if (!LittleFS.format()) {
            Serial.println("Форматирование не удалось.");
            return;
        }

        // Повторная попытка монтирования после форматирования
        if (!LittleFS.begin()) {
            Serial.println("Не удалось смонтировать после форматирования.");
            return;
        }

        Serial.println("Файловая система успешно отформатирована и смонтирована.");
    } else {
        Serial.println("Файловая система успешно смонтирована.");
    }

    // Проверяем наличие директорий /schedules и /scenarios
    const char *dirs[] = {"/schedules", "/scenarios"};

    for (int i = 0; i < 2; i++) {
        if (!LittleFS.exists(dirs[i])) {
            if (LittleFS.mkdir(dirs[i])) {
                Serial.printf("Директория %s успешно создана.\n", dirs[i]);
            } else {
                Serial.printf("Не удалось создать директорию %s.\n", dirs[i]);
            }
        } else {
            Serial.printf("Директория %s уже существует.\n", dirs[i]);
        }
    }
}
