/**
 * @file SmoothLED.h
 * @brief Simple non‑blocking LED driver with fade and blink effects.
 */

#ifndef SMOOTHLED_H
#define SMOOTHLED_H

#include <Arduino.h>
#include <Ticker.h>
#include "settings.h"

// Predefined LED states used across the project
const byte _FADE = 1;     ///< Smooth fading effect
const byte _BLINK_2 = 2;  ///< Blink at 2 Hz
const byte _BLINK_1 = 5;  ///< Blink at 1 Hz
const byte _OFF = 4;      ///< LED off
const byte _ON = 3;       ///< LED fully on

void updateLEDs(void *pvParameters);
extern bool normalMode;
extern String accessToken;

/**
 * @brief Wrapper around a PWM controlled LED that supports several animation
 *        modes. The class uses an internal Ticker to periodically update the
 *        LED without blocking the main loop.
 */
class SmoothLED {
public:
  /**
   * @brief Construct a new SmoothLED instance.
   * @param pin           GPIO pin controlling the LED
   * @param minBrightness Minimum PWM value
   * @param maxBrightness Maximum PWM value
   * @param fadeInterval  Timer period in milliseconds
   * @param fadeStep      PWM increment/decrement step
   */
  SmoothLED(int pin, int minBrightness, int maxBrightness, int fadeInterval, int fadeStep);

  void begin();            ///< Initialize GPIO and start timer
  void update();           ///< Placeholder (timer does the work)

  void setState(int state); ///< Change LED animation state
  int getState();           ///< Retrieve current state

private:
  int ledPin;
  int minBrightness;
  int maxBrightness;
  int fadeInterval;
  int fadeStep;
  int ledBrightness;
  bool fadingUp;
  int ledState;          ///< Current animation state
  bool isBlinking;       ///< Used by 1 Hz blinking
  unsigned long lastBlinkTime; ///< Last toggle timestamp
  const unsigned long blinkInterval = 500; ///< Interval for 1 Hz blinking

  Ticker ticker;         ///< Periodic timer invoking animation callbacks

  void fadeLED();        ///< Execute fading animation
  void blink2Hz();       ///< Blink twice per second
  void blink1Hz();       ///< Blink once per second

  // Static wrapper passed to Ticker since it accepts plain function pointers
  static void tickerCallback(SmoothLED* instance) {
        if (instance->ledState == _FADE)
            instance->fadeLED();
        else if (instance->ledState == _BLINK_2)
            instance->blink2Hz();
        else if (instance->ledState == _BLINK_1)
            instance->blink1Hz();
    }
};

#endif // SMOOTHLED_H
