#ifndef MUTEXLOCK_H
#define MUTEXLOCK_H

#include <Arduino.h>

/**
 * @file MutexLock.h
 * @brief Small RAII wrapper around FreeRTOS semaphores.
 *
 * Creating an instance of this class will attempt to take the provided mutex
 * in the constructor and automatically release it when the object goes out of
 * scope.  This greatly simplifies error handling for critical sections and
 * ensures that mutexes are always released even if a function exits early.
 */

extern SemaphoreHandle_t mqttMutex;      ///< Protects access to MQTT client
extern SemaphoreHandle_t dataQueueMutex; ///< Guards the sensor data queue
extern SemaphoreHandle_t adsMutex;       ///< Used for ADS sensor access

class MutexLock {
public:
    explicit MutexLock(SemaphoreHandle_t& mutex) : _mutex(mutex), _locked(false) {
        if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
            _locked = true;
        } else {
            Serial.println("Failed to take mutex");
        }
    }

    ~MutexLock() {
        if (_locked) {
            xSemaphoreGive(_mutex);
        }
    }

    /** Manually release the mutex before destruction. */
    void unlock() {
        if (_locked) {
            xSemaphoreGive(_mutex);
            _locked = false;
        }
    }

    MutexLock(const MutexLock&) = delete;
    MutexLock& operator=(const MutexLock&) = delete;

private:
    SemaphoreHandle_t& _mutex;
    bool _locked;
};

#endif // MUTEXLOCK_H
