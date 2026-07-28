# Civic Boost Gauge 1.3.0 Firmware

[Home](../../README.md) | [Español](README.es.md) | **English**

Version 1.3.0 adds persistent bilingual controls, a deliberate display offset
and configurable pressure smoothing while preserving the hardware-verified
XGZP6847D acquisition, shared FT3168 touch bus and prebaked 60 Hz renderer from
version 1.2.1.

## Highlights

- Persistent `EN`/`ES` selector that translates the complete settings menu
  immediately without rebooting.
- Persistent display offset from `-1.5` to `+1.5 PSI` in `0.1 PSI` steps.
- Smoothing OFF mode with no temporal pressure filtering.
- Configurable EMA alpha from `0.05` to `1.00`, default `0.35`.
- Configurable One Euro minimum cutoff from `0.5` to `5.0 Hz`, default
  `2.0 Hz`, and beta from `0.00` to `3.00` per kPa, default `1.00`.
- One-button restoration of the filter defaults without changing the selected
  smoothing mode.
- Complete linked English and Spanish documentation for the project, pressure
  setup, development guide and every published firmware version.
- Existing PSI/BAR, brightness and sensor-temperature preferences remain
  persistent.

The XGZP6847D is a factory-calibrated gauge-pressure sensor. It already
establishes zero relative to atmosphere; the firmware does not perform startup
auto-zero. Leave the offset at `+0.0 PSI` for physical manifold pressure. Use
it only when deliberately matching a compatible boost value calculated
internally by the car.

## Validation

The packaged application is the exact binary compiled and flashed to COM6 for
the final hardware test:

- Internal RAM: 65,392 bytes.
- Application flash: 1,201,329 bytes.
- Sensor acquisition: up to 100 Hz.
- Display schedule: 60 Hz.
- Production diagnostic flags: disabled.
- XGZP6847D, FT3168, PSRAM and prebaked renderer: detected successfully.
- Offset, smoothing modes and bilingual touch controls: verified on hardware.
- Application SHA-256:
  `ef682fba4cfa15469d9e818a32ef80c7ab225861a839811e2b9b4fda3f6e60e0`.

## Package contents

- `civic-boost-gauge-full.bin`: complete image with bootloader, partition table
  and application. Flash it at address `0x0`.
- `civic-boost-gauge-app.bin`: application-only update for a board that already
  has the correct bootloader and partition table. Flash it at `0x10000`.
- `civic-boost-gauge-psi-demo.gif`: PSI sweep generated from the canonical
  firmware cache.
- `civic-boost-gauge-bar-demo.gif`: BAR sweep generated from the same cache.
- `civic-boost-gauge-boot.png`: exact 466x466 boot-screen preview.
- `xgzp6847d-wiring-diagram.png`: ready-to-view wiring diagram.
- `xgzp6847d-wiring-diagram.svg`: editable vector wiring diagram.
- `README.md` and `README.es.md`: release instructions in both languages.
- `SHA256SUMS.txt`: checksums for every packaged release file.

## Wiring summary

| Waveshare four-pin I2C connector | XGZP6847D |
| --- | --- |
| Pin 1 / SDA / GPIO 47 | Pin 5 / SDA |
| Pin 2 / SCL / GPIO 48 | Pin 1 / SCL |
| Pin 3 / GND | Pin 3 / GND |
| Pin 4 / 3V3 | Pin 4 / VDD |

Leave sensor pins 2 and 6 unconnected. Fit a non-polarized 100 nF ceramic
capacitor rated for at least 10 V between sensor pins 4 and 3, close to the
sensor. Keep the sensor and shared I2C pull-ups at 3.3 V.

## Flashing

Example complete installation:

```powershell
esptool.py --chip esp32s3 --port COM6 write_flash 0x0 civic-boost-gauge-full.bin
```

Example application-only update:

```powershell
esptool.py --chip esp32s3 --port COM6 write_flash 0x10000 civic-boost-gauge-app.bin
```

Verify every download against `SHA256SUMS.txt` before flashing.
