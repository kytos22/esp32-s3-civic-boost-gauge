# ESP32-S3 Civic Boost Gauge

Turbo boost gauge for the Waveshare ESP32-S3-Touch-AMOLED-1.43. The interface
is designed for the 466x466 AMOLED with the USB connector facing down.

The renderer uses pre-generated static visuals and a build-time gauge cache for
smooth 60 Hz updates. The 4.90 MB cache is stored in the firmware as a 465 KB
zlib asset, validated and decompressed directly into PSRAM during the startup
screen. An initial sweep runs before live sensor readings begin.

## Gauge demo

![Civic boost gauge demo](firmware/1.0.0/civic-boost-gauge-demo.gif?rev=942f434)

## Boot screen

![Honda Civic boot screen](firmware/1.0.0/civic-boost-gauge-boot.png)

## Features

- Selectable gauge range: -15 to 30 PSI or -1 to 2 BAR.
- PSI is the default; the selected unit is saved across restarts.
- Smooth pressure-dependent arc color.
- Prebaked arc, cursor, number and logo rendering.
- Versioned cache format with size, ABI and CRC validation.
- Five-second Honda/Civic startup screen and initial sweep.
- Capacitive touch on the Civic logo to toggle SHOW mode.
- Long press on the Civic logo for unit selection, brightness and zero calibration.
- 75% default display brightness.
- Sensor input is currently WIP while the replacement sensor is in transit.

## Pressure sensor

The planned turbo sensor is the **XGZP6847D I2C**, using the bidirectional
`-100 to +300 kPa` variant. It accepts `2.5 to 5.5 V`, uses the I2C address
`0x6D`, and is suitable for the gauge's `-15 to 30 PSI` range. The final
firmware integration and calibration will be completed when the sensor arrives.

Until then, the live firmware keeps the existing analog input path on GPIO 1
for bench testing; the SHOW mode is available without a connected sensor.
The analog path maps millivolts directly into the selected display unit, so BAR
mode does not perform a PSI-to-BAR conversion on every frame.

## Hardware

- Waveshare ESP32-S3-Touch-AMOLED-1.43.
- Analog pressure sensor connected to GPIO 1.
- Common ground between the sensor and the ESP32-S3.

The ESP32-S3 ADC must never receive more than 3.3 V. Use a suitable voltage
divider or signal conditioner when powering a 5 V analog sensor.

## Build

The project includes the exact library versions used by the working firmware.
Install PlatformIO, open this directory and run:

```powershell
pio run
```

To upload to a specific serial port:

```powershell
pio run --target upload --upload-port COM6
```

The validated renderer snapshot is documented in `GOLDEN_VERSION.md`. Version
1.0 firmware images and the demo GIF are in `firmware/1.0.0/`.

## Prebaked cache

`tools/prebaked_gauge_cache.bin` is the canonical cache captured from the
artifact-free renderer. Before compilation, `tools/platformio_prebuild.py`
checks whether it changed and runs `tools/generate_prebaked_cache.py` when the
compressed C++ asset needs to be refreshed.

To regenerate the canonical cache after changing gauge geometry or colors:

1. Set `DUMP_BAKED_CACHE` to `1` in `src/main.cpp`.
2. Build and upload the temporary exporter.
3. Run `python tools/capture_prebaked_cache.py --port COM6`.
4. Set `DUMP_BAKED_CACHE` back to `0`.
5. Build normally; PlatformIO regenerates the compressed firmware asset.

The capture tool validates the binary header and CRC before replacing the
canonical file. Normal firmware never performs the geometric cache generation.

## Notes

Honda and Civic names and logos are trademarks of Honda Motor Co., Ltd. This is
an independent enthusiast project and is not affiliated with or endorsed by
Honda.
