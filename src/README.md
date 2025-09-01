# Source Directory Overview

This folder contains the firmware sources for the ESP32 based controller.  The
code is organised into loosely coupled modules so that developers can pick the
pieces relevant for their projects:

* **BLEWiFiConfig** – BLE GATT service used during initial provisioning to pass
  Wi‑Fi credentials and exchange basic commands.
* **DataQueue / SmoothLED** – utility classes providing a thread‑safe sensor
  data queue and a non‑blocking LED animation helper.
* **MQTT helpers** – `mqttFunc.h` and `basic/mqttProcess.h` implement the cloud
  communication layer including OTA update support.
* **Task setup** – `setupTasks.h` contains functions that initialise hardware,
  start FreeRTOS tasks and periodically check connectivity.

Additional headers such as `callbacks.h` and `serverDataExchange.h` implement
specialised helpers for button handling and OAuth device flow respectively.

All modules are heavily commented to serve as a starting point for third‑party
developers integrating the SDK into their own applications.

