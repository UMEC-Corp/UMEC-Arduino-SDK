/**
 * @file SmoothLED.cpp
 * @brief Implementation of the SmoothLED class defined in SmoothLED.h.
 */

#include "SmoothLED.h"

extern SmoothLED smoothLED2;
extern SmoothLED smoothLED1;
extern MQTTPubSub::PubSubClient<MQTT_BUFFER_SIZE> mqtt;

SmoothLED::SmoothLED(int pin, int minBrightness, int maxBrightness,
                     int fadeInterval, int fadeStep)
    : ledPin(pin),
      minBrightness(minBrightness),
      maxBrightness(maxBrightness),
      fadeInterval(fadeInterval),
      fadeStep(fadeStep),
      ledBrightness(minBrightness),
      fadingUp(true),
      ledState(_FADE),
      isBlinking(false),
      lastBlinkTime(0) {}

void SmoothLED::begin() {
  pinMode(ledPin, OUTPUT);
  analogWrite(ledPin, ledBrightness);

  // Start periodic timer that triggers animation callbacks
  ticker.attach_ms(fadeInterval, &SmoothLED::tickerCallback, this);
}

// Private helper performing a smooth fade in/out
void SmoothLED::fadeLED() {
  if (fadingUp) {
    ledBrightness += fadeStep;
    if (ledBrightness >= maxBrightness) {
      ledBrightness = maxBrightness;
      fadingUp = false;
    }
  } else {
    ledBrightness -= fadeStep;
    if (ledBrightness <= minBrightness) {
      ledBrightness = minBrightness;
      fadingUp = true;
    }
  }
  analogWrite(ledPin, ledBrightness);
}

// Blink twice per second
void SmoothLED::blink2Hz() {
  if (ledBrightness == 0) {
    ledBrightness = maxBrightness;
  } else {
    ledBrightness = 0;
  }
  analogWrite(ledPin, ledBrightness);
}

// Blink once per second using non-blocking timing
void SmoothLED::blink1Hz() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastBlinkTime >= blinkInterval) {
    lastBlinkTime = currentMillis;
    if (isBlinking) {
      analogWrite(ledPin, maxBrightness);
    } else {
      analogWrite(ledPin, minBrightness);
    }
    isBlinking = !isBlinking;
  }
}

void SmoothLED::setState(int state) {
  ledState = state;
  if (ledState == _FADE || ledState == _BLINK_2 || ledState == _BLINK_1) {
    // Reconfigure timer for animation states
    ticker.attach_ms(fadeInterval, &SmoothLED::tickerCallback, this);
  } else {
    ticker.detach();
    analogWrite(ledPin, ledState == _ON ? maxBrightness : minBrightness);
  }
}

int SmoothLED::getState() {
  return ledState;
}

void SmoothLED::update() {
  // Timer-driven; nothing to do here
}

/**
 * @brief Task loop that updates status LEDs based on system state.
 */
void updateLEDs(void *pvParameters) {
    preferences.begin("wifi-config", true);
    String storedSSID = preferences.getString("ssid", "");
    preferences.end();

    while (true) {
        if (!normalMode) {
            continue;
        }
        if (storedSSID.isEmpty()) {
            smoothLED1.setState(_BLINK_2);
            smoothLED2.setState(_BLINK_2);
        } else if (WiFi.status() != WL_CONNECTED) {
            smoothLED1.setState(_BLINK_2);
            smoothLED2.setState(_OFF);
        } else {
            if (accessToken.isEmpty()) {
                smoothLED1.setState(_FADE);
                smoothLED2.setState(_OFF);
            } else if (!mqtt.isConnected()) {
                smoothLED1.setState(_FADE);
                smoothLED2.setState(_BLINK_2);
            } else {
                smoothLED1.setState(_FADE);
                smoothLED2.setState(_FADE);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

