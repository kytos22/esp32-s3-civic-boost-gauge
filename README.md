# ESP32-S3 Civic Boost Gauge

[Español](README.es.md) | **English**

Turbo boost gauge for the Waveshare ESP32-S3-Touch-AMOLED-1.43. The interface
is designed for the 466x466 AMOLED with the USB connector facing down.

The current production release is **v1.3.0**, using the hardware-tested
XGZP6847D digital pressure sensor and the stable FT3168 shared-I2C touch path.

The renderer uses pre-generated static visuals and a build-time gauge cache for
smooth 60 Hz updates. The 4.90 MB cache is stored in the firmware as a 465 KB
zlib asset, validated and decompressed directly into PSRAM during the startup
screen. An initial sweep runs before live sensor readings begin.

## PSI gauge demo

![Civic boost gauge PSI demo](firmware/1.3.0/civic-boost-gauge-psi-demo.gif)

## BAR gauge demo

![Civic boost gauge BAR demo](firmware/1.3.0/civic-boost-gauge-bar-demo.gif)

## Boot screen

![Honda Civic boot screen](firmware/1.3.0/civic-boost-gauge-boot.png)

## Features

- Selectable gauge range: -15 to 30 PSI or -1 to 2 BAR.
- PSI is the default; the selected unit is saved across restarts.
- Smooth pressure-dependent arc color.
- Prebaked arc, cursor, number and logo rendering.
- Versioned cache format with size, ABI and CRC validation.
- Five-second Honda/Civic startup screen and initial sweep.
- Capacitive touch on the Civic logo to toggle SHOW mode.
- Long press on the Civic logo for unit selection, brightness, pressure offset
  and persistent sensor-temperature and smoothing controls.
- Persistent `EN`/`ES` selector that translates the complete settings menu
  immediately without rebooting.
- Persistent pressure offset adjustable from -1.5 to +1.5 PSI in 0.1 PSI
  steps, with consistent conversion when the gauge is shown in BAR.
- Selectable raw, configurable EMA or configurable One Euro pressure
  processing, with one-tap restoration of the filter defaults.
- 75% default display brightness with persistent menu adjustments.
- Hardware-tested XGZP6847D digital pressure and temperature acquisition.
- Visible `ERR` warning after one second without a valid sensor reading.
- Visible `MAX` warning when pressure exceeds the 30 PSI display range.

## Pressure sensor

The turbo sensor is the **XGZP6847D300KPGPN I2C**, using the bidirectional
`-100 to +300 kPa` range. It is powered from 3.3 V, uses address `0x6D` and the
V2.x `K=16` transfer factor. The firmware samples it independently at up to
100 Hz and converts its signed 24-bit result to canonical kPa. The selected
smoothing mode is applied in kPa, followed by the optional display offset and
the conversion to PSI or BAR. The renderer remains on its independent 60 Hz
schedule.

### Atmospheric zero and display offset

The XGZP6847D is a factory-calibrated gauge-pressure sensor. Its pressure
reference is atmospheric pressure, so when the measurement side is also open
to the same atmosphere the sensor already reports approximately `0 kPa`
(`0 PSI`). This atmospheric zero comes from the sensor and its physical
reference; the firmware deliberately does not capture or subtract a new zero
at startup. The [official CFSensor datasheet](https://cfsensor.com/wp-content/uploads/2022/11/XGZP6847D-Pressure-Sensor-V3.0.pdf)
identifies `GPN` as the negative-and-positive gauge-pressure type and lists the
`-100 to +300 kPa` variant used by this project.

Automatic software zeroing would be harmful in a car. If the gauge booted
while the engine was producing manifold vacuum or boost, that real pressure
could be mistaken for zero and every later reading would be shifted. For a
physical manifold-pressure reading, leave `OFFSET` at its default `+0.0 PSI`.

> **Important:** do not use `OFFSET` to zero a correctly installed sensor in
> free air. The sensor already establishes atmospheric zero. Use the offset
> only when intentionally matching the car's internally calculated value.

The persistent `OFFSET` control is therefore not a sensor calibration. Its
intended use is to deliberately align or simulate the boost value calculated
internally by the car or shown through a compatible ECU/OBD value. It can add
or subtract `1.5 PSI` in `0.1 PSI` steps. Use it only after confirming that the
value being compared is gauge/boost pressure rather than absolute MAP pressure.
It cannot convert absolute pressure into gauge pressure and it cannot correct
a scale, hose or sensor-range error.

The offset is applied after smoothing and before the PSI/BAR conversion, so
the correction remains consistent in either display unit. A small display
deadband shows values within `±0.25 PSI` or `±0.02 bar` as zero; this only
prevents visual flicker and does not recalibrate or alter the sensor zero.

### Pressure smoothing

Smoothing reduces visible needle and number jitter caused by normal sample
noise. It does not calibrate pressure, change the atmospheric zero or reduce
the sensor acquisition rate. `OFF` uses every valid sample without temporal
smoothing. `EMA` offers a simple configurable response, while `1 EURO`
automatically smooths slow changes more strongly and reacts faster to rapid
boost changes.

The persistent editor provides EMA alpha from `0.05` to `1.00` in `0.05`
steps (default `0.35`). One Euro provides a minimum cutoff from `0.5` to
`5.0 Hz` in `0.1 Hz` steps (default `2.0 Hz`) and beta from `0.00` to `3.00`
per kPa in `0.05` steps (default `1.00`). Its derivative cutoff remains fixed
at `1 Hz`. `RESET` restores these parameter defaults without changing the
selected mode. Mode and values are retained across restarts.

See [Pressure setup, offset and smoothing](docs/pressure-settings.md) for a
detailed explanation, recommended tuning procedure and practical examples.

Select `EN` or `ES` at the top of the settings menu to change its language.
The choice is stored across restarts. Pressure units, numeric values and the
universal `ERR`/`MAX` status indicators remain unchanged.

The sensor's internal temperature display is disabled by default and can be
enabled permanently with the menu's `TEMP: OFF/ON` button. When enabled, its
large readout is shown between the Civic logo and pressure unit and is
refreshed every five seconds.

Invalid, stale or missing readings remain non-blocking and show `ERR` after one
second, while the sensor is periodically reprobed after a bus failure. Pressure
above the 30 PSI display limit is clamped safely and marked with `MAX`.
Production builds keep continuous touch and sensor telemetry disabled; boot,
error and recovery messages remain available over USB serial.

## Hardware

- Waveshare ESP32-S3-Touch-AMOLED-1.43.
- XGZP6847D300KPGPN connected to the shared I2C bus on GPIO 47/48.
- Sensor VDD connected to 3.3 V.
- Common ground between the sensor and the ESP32-S3.

## Wiring

![Waveshare ESP32-S3 to XGZP6847D wiring diagram](docs/xgzp6847d-wiring-diagram.png)

Use the board's four-pin SH1.0 I2C connector. The connector contact order and
sensor pin mapping are:

| Waveshare connector | Signal | XGZP6847D pin |
| --- | --- | --- |
| Pin 1 | SDA / GPIO 47 | Pin 5 / SDA |
| Pin 2 | SCL / GPIO 48 | Pin 1 / SCL |
| Pin 3 | GND | Pin 3 / GND |
| Pin 4 | 3V3 | Pin 4 / VDD |

Leave XGZP6847D pins 2 and 6 completely unconnected. Place a 100 nF ceramic
capacitor rated for at least 10 V between sensor pins 4 (VDD) and 3 (GND), as
close to the sensor as practical. The capacitor has no polarity. Cable colors
may vary, so follow the connector pin numbers rather than color alone.

The editable vector diagram is available in
[`docs/xgzp6847d-wiring-diagram.svg`](docs/xgzp6847d-wiring-diagram.svg).

The onboard FT3168 uses Waveshare's grouped polling method on the 300 kHz shared
bus: normal mode, one finger-count read and one grouped X/Y read per active
touch. Active power mode is refreshed every second and automatic monitor entry
is disabled because the pure demo initialization can stop acknowledging reads
on the shared live bus.

The sensor and I2C pull-ups must remain at 3.3 V. Do not apply 5 V to the SDA
or SCL lines.

## Firmware download

Download the complete v1.3.0 package from the
[GitHub release page](https://github.com/kytos22/esp32-s3-civic-boost-gauge/releases/tag/v1.3.0).
Use the full image at flash address `0x0` for a complete installation. Use the
application-only image at `0x10000` only when the board already has the matching
bootloader and partition table. Verify downloaded files against
`SHA256SUMS.txt`.

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

## Development guide

Coding agents and contributors should read [`AGENTS.md`](AGENTS.md) before
changing the renderer, display transfer, touch mapping, generated assets or
sensor path. It documents the verified architecture, invariants and validation
workflows used by this project.

The validated renderer snapshot is documented in `GOLDEN_VERSION.md`. Version
1.3.0 firmware images and both gauge GIFs are in `firmware/1.3.0/`.

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
