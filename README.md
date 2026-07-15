# ESP32-S3 Civic Boost Gauge

Turbo boost gauge for the Waveshare ESP32-S3-Touch-AMOLED-1.43. The interface
is designed for the 466x466 AMOLED with the USB connector facing down.

The renderer uses pre-generated static visuals and a runtime cache in PSRAM for
smooth 60 Hz gauge updates. The startup screen remains visible while that cache
is prepared, followed by an initial sweep before live sensor readings begin.

## Features

- Gauge range from -15 to 30 PSI.
- Smooth pressure-dependent arc color.
- Prebaked arc, cursor, number and logo rendering.
- Five-second Honda/Civic startup screen and initial sweep.
- Capacitive touch on the Civic logo to toggle SHOW mode.
- Long press on the Civic logo for brightness and zero calibration.
- 75% default display brightness.
- Analog pressure input on GPIO 1.

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

The validated renderer snapshot is documented in `GOLDEN_VERSION.md`.

## Notes

Honda and Civic names and logos are trademarks of Honda Motor Co., Ltd. This is
an independent enthusiast project and is not affiliated with or endorsed by
Honda.
