# Firmware SDK

This repository contains ESP32 firmware intended to be used as a starting
point for building connected devices. The code is organised as an SDK so that
third‑party developers can integrate custom sensors or business logic.

## Project structure

- `src/` – main firmware sources. Notable modules:
  - `BLEFunc.*` – handles provisioning commands received via Bluetooth LE.
  - `DataQueue.*` – thread‑safe queue for sensor values and outgoing MQTT messages.
  - `SmoothLED.*` – non‑blocking driver for status LEDs with fade and blink modes.
  - `utilities.*` – miscellaneous helpers, watchdog wrapper and serial command
    interface.
  - `basic/main.cpp` – application entry point used by the `basic` PlatformIO
    environment.
- `include/`, `lib/` – conventional PlatformIO folders for headers and
  additional libraries.
- `platformio.ini` – build configuration. The default environment `basic` targets
  the ESP32 DevKit and uses custom partition tables.

## Building

The project uses [PlatformIO](https://platformio.org/). To build the firmware
run:

```bash
pio run
```

To upload to an attached board and open the serial monitor:

```bash
pio run --target upload
pio device monitor
```

## Coding style

All newly added code is documented using brief Doxygen‑style comments. When
extending the SDK please follow the same convention and keep modules focused on
single responsibilities. Utility functions shared between modules belong in
`utilities.*`.

## License

This project is provided as‑is without warranty. You are free to adapt it for
your hardware or products.

