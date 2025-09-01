#pragma once

/**
 * @file serverDataExchange.h
 * @brief Helper functions for HTTP communication with the cloud backend during
 *        OAuth device flow.  The routines here implement polling for tokens as
 *        well as retrieval of configuration files.  Functions are intentionally
 *        synchronous because they are executed in dedicated FreeRTOS tasks.
 */

unsigned long lastRequestTime = 0; // Переменная для хранения времени последнего запроса
extern WiFiClientSecure secureClient;

/**
 * @brief Perform an HTTPS POST request.
 * @param url           Target URL.
 * @param data          Form encoded body.
 * @param[out] response Response body on success.
 * @param[out] responseCode HTTP status code.
 * @return true on success, false otherwise.
 */
bool sendHttpPost(String url, String data, String &response, int &responseCode) {
    // Проверка наличия URL
    if (url.length() == 0) {
        Serial.println("Error: URL is empty");
        return false;
    }

    // Проверка подключения к Wi-Fi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Error: WiFi not connected");
        return false;
    }

    // Проверка частоты запросов
    unsigned long currentTime = millis();
    if (currentTime - lastRequestTime < 3000) { // 3000 миллисекунд = 3 секунды
    //    Serial.println("Error: Requests are too frequent");
        return false;
    }
    HTTPClient http;


    http.begin(secureClient, url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int httpResponseCode = http.POST(data);
    bool returnF;
    if (httpResponseCode > 0) {
        response = http.getString();
        responseCode = httpResponseCode;
        returnF = true;
    } else {
        returnF = false;
    }

    Serial.println("STEP1, URL: " + String(url) + " DATA: " + String(data) + " RESPONSE: " + String(response) + " RESPONSECODE: " + String(responseCode));
    http.end();
    
    // Обновление времени последнего запроса при успешной отправке
    if (returnF) {
        lastRequestTime = currentTime;
    }
    
    return returnF;
    }


unsigned long lastDeviceCodeExchangeTime = 0; // Переменная для хранения времени последнего вызова функции

/**
 * @brief Request a temporary device and user code from the authorization
 *        server.  This is the first step of the OAuth device flow.
 * @param authUrl         Endpoint for the device code request.
 * @param[out] deviceCode Received device code.
 * @param[out] userCode   Code that the user must enter on the verification page.
 * @param[out] verificationUrl Full URL where the user should enter the code.
 */
void performDeviceCodeExchange(String authUrl, String &deviceCode, String &userCode, String &verificationUrl) {
    unsigned long currentTime = millis();
    if (currentTime - lastDeviceCodeExchangeTime < 3000) { // 3000 миллисекунд = 3 секунды
      //  Serial.println("Error: Device code exchange requests are too frequent");
        return;
    }

    String responce;
    int responceCode;
    if (sendHttpPost(authUrl, "client_id=controller01&scope=mqtt-streaming", responce, responceCode)) {
        if (responceCode == 200) {
            JsonDocument jsonDoc; // Добавляем размер буфера для JSON-документа
            Serial.println(responce);
            DeserializationError error = deserializeJson(jsonDoc, responce);

            if (error) {
                Serial.println("Error parsing JSON response");
            } else {
                deviceCode = jsonDoc["device_code"].as<String>();
                userCode = jsonDoc["user_code"].as<String>();
                verificationUrl = jsonDoc["verification_uri_complete"].as<String>();
            }
        }
        Serial.println("STEP2, AUTHURL: " + String(authUrl) + " DEVICECODE: " + String(deviceCode) + " USERCODE: " + String(userCode) + " VERURL: " + String(verificationUrl));
    } else {
        Serial.println("Error on HTTP request!!!");
    }
    
    // Обновление времени последнего вызова функции
    lastDeviceCodeExchangeTime = currentTime;
}

/**
 * @brief Poll the token endpoint until the user authorises the device and an
 *        access token becomes available.
 * @param tokenUrl      Token endpoint URL.
 * @param[out] accessToken  Returned access token.
 * @param[out] refreshToken Returned refresh token.
 * @param deviceCode    Device code obtained earlier.
 */
void performTokenExchange(String tokenUrl, String &accessToken, String &refreshToken, String deviceCode)
{
    static unsigned long lastInvocationTime = 0;
    unsigned long currentMillis = millis();

    // Проверка, прошло ли 3 секунды с последнего вызова
    if (currentMillis - lastInvocationTime < 3000) {
        //Serial.println("Function called too frequently. Skipping execution.");
        return;
    }

    lastInvocationTime = currentMillis;

    int httpResponseCode;
    String responce;
    if (sendHttpPost(tokenUrl, "client_id=controller01&grant_type=urn:ietf:params:oauth:grant-type:device_code"
                               "&scopes=mqtt-streaming"
                               "&device_code=" +
                                   deviceCode,
                     responce, httpResponseCode))
    {

        Serial.println("Response: " + responce);

        if (httpResponseCode == 200)
        {
            JsonDocument jsonDoc;
            DeserializationError error = deserializeJson(jsonDoc, responce);

            if (error)
            {
                Serial.println("Error parsing JSON response");
            }
            else
            {
                accessToken = jsonDoc["access_token"].as<String>();
                refreshToken = jsonDoc["refresh_token"].as<String>();
                expiresIn = jsonDoc["expires_in"].as<int>();

                prefs.begin("nvs", false);

                // Проверяем, изменился ли refreshToken
                String existingRefreshToken = prefs.getString("refreshToken", "");
                if (refreshToken != existingRefreshToken) {
                    prefs.putString("refreshToken", refreshToken);
                }

                prefs.putString("accessToken", accessToken);
                prefs.putInt("expiresIn", expiresIn);

                Serial.println("TOKENURL: " + String(tokenUrl) + " ACCESSTOKEN: " + String(accessToken) + 
                            " REFRESHTOKEN: " + String(refreshToken) + " DEVICECOD!: " + String(deviceCode));
                prefs.end();                    

            }
        }
    }
}

/**
 * @brief Refresh an expiring access token using a refresh token.
 */
void performTokenUpdate(String tokenUrl, String &accessToken, String &refreshtoken)
{
    //mqttUrl = prefs.getString("mqttUrl", "");("Before HTTP request");
    int httpResponseCode;
    String responce;
    if (sendHttpPost(tokenUrl, "client_id=controller01&grant_type=refresh_token&refresh_token=" + refreshtoken,
                     responce, httpResponseCode))
    {

        Serial.println("Response: " + responce);
        if (httpResponseCode > 0)
        {
            
            Serial.println("HTTP Response code: " + String(httpResponseCode));
            Serial.println("Response: " + responce);
            if (httpResponseCode == 200)
            {
                // Разбор и обработка JSON-ответа
                JsonDocument jsonDoc;
                DeserializationError error = deserializeJson(jsonDoc, responce);

                if (error)
                {
                    Serial.println("Error parsing JSON response");
                }
                else
                {
                    // Извлечение значений из JSON
                    const char *localaccessToken = jsonDoc["access_token"];
                    const char *localrefreshToken = jsonDoc["refresh_token"];
                    int expiresIn = jsonDoc["expires_in"];

                    // Вывод полученных значений
                    Serial.println("Access Token: " + String(localaccessToken));
                    accessToken = String(localaccessToken);
                    Serial.println("Refresh Token: " + String(localrefreshToken));
                    refreshtoken = String(localrefreshToken);
                    Serial.println("Expires In: " + String(expiresIn));

                    prefs.begin("nvs", false);
                    // Чтение текущего значения expires_in из NVS
                    int currentExpiresIn = prefs.getInt("expiresIn", 0);

                    // Сравнение и обновление значений в NVS
                    if (currentExpiresIn != expiresIn)
                    {
                        Serial.println("Updating NVS with new expires_in value");
                        prefs.putInt("expiresIn", expiresIn);
                    }

                    prefs.putString("accessToken", accessToken);
                    prefs.putString("refreshToken", refreshtoken);
                    prefs.end();

                }
            }
        }
    }
}


/**
 * @brief Download the configuration file from the cloud and update locally
 *        stored URLs only when they changed.
 */
void fetchAndStoreConfig(const String& inputConfigUrl) {
    // Обновляем глобальную переменную configUrl
    configUrl = inputConfigUrl;

    // Создаем объект HTTPClient
    HTTPClient http;
    http.begin(configUrl);  // Открываем URL

    int httpCode = http.GET();  // Выполняем GET-запрос

    if (httpCode > 0) { // Проверяем код ответа
        String payload = http.getString();  // Получаем тело ответа

        // Создаем объект для парсинга JSON
        JsonDocument jsonDocument;
        DeserializationError error = deserializeJson(jsonDocument, payload);

        if (!error) {  // Проверяем на ошибки парсинга
        
            prefs.begin("nvs", false);

            // Получаем новые значения из JSON
            String newAuthUrl = jsonDocument["authUrl"].as<String>();
            String newTokenUrl = jsonDocument["tokenUrl"].as<String>();
            String newMqttUrl = jsonDocument["mqttUrl"].as<String>();
            String newCloudApiUrl = jsonDocument["cloudApiUrl"].as<String>();
            String newConfigUrl = jsonDocument["configUrl"].as<String>();

            // Флаг для отслеживания изменений
            bool updated = false;

            // Сравниваем и обновляем только если значения изменились
            if (authUrl != newAuthUrl) {
                authUrl = newAuthUrl;
                prefs.putString("authUrl", authUrl);
                Serial.printf("Updated authUrl: %s\n", authUrl.c_str());
                updated = true;
            }
            if (tokenUrl != newTokenUrl) {
                tokenUrl = newTokenUrl;
                prefs.putString("tokenUrl", tokenUrl);
                Serial.printf("Updated tokenUrl: %s\n", tokenUrl.c_str());
                updated = true;
            }
            if (mqttUrl != newMqttUrl) {
                mqttUrl = newMqttUrl;
                prefs.putString("mqttUrl", mqttUrl);
                Serial.printf("Updated mqttUrl: %s\n", mqttUrl.c_str());
                updated = true;
            }
            if (cloudApiUrl != newCloudApiUrl) {
                cloudApiUrl = newCloudApiUrl;
                prefs.putString("cloudApiUrl", cloudApiUrl);
                Serial.printf("Updated cloudApiUrl: %s\n", cloudApiUrl.c_str());
                updated = true;
            }
            if (configUrl != newConfigUrl) {
                configUrl = newConfigUrl;
                prefs.putString("configUrl", configUrl);
                Serial.printf("Updated configUrl: %s\n", configUrl.c_str());
                updated = true;
            }
            // Проверяем, были ли обновления
            if (!updated) {
                Serial.println("No changes in URLs.");
            }

            prefs.end();
        } else {
            Serial.println("Failed to parse JSON");
        }
    } else {
        Serial.printf("HTTP GET failed, code: %d\n", httpCode);
    }

    http.end();  // Закрываем HTTP соединение
}

