# Civic Boost Gauge 1.4.0 Firmware

[Home](../../README.md) | [Español](README.es.md) | **English**

Version 1.4.0 reorganizes the growing settings interface into clear categories,
adds a configurable startup-logo duration and introduces a guarded full reset.
It also makes One Euro the fresh-install and general-reset pressure-filter
default with stronger electronic-wastegate damping, while preserving the
hardware-tested sensor, touch bus and 60 Hz prebaked renderer from version
1.3.0.

## Highlights

- First settings page with persistent `EN`/`ES`, brightness, `GAUGE`,
  `PRESSURE` and guarded `RESET ALL` controls.
- Dedicated `GAUGE` page for PSI/BAR, sensor temperature and the persistent
  1 to 10-second startup-logo duration, defaulting to one second.
- Dedicated `PRESSURE` page for the persistent offset and smoothing editors.
- One Euro is the new fresh-install and general-reset default mode, using
  `MIN 1.0 Hz` and `BETA 0.25` per kPa for stronger anti-jitter damping.
- Filter `RESET` restores EMA `0.35` and One Euro `1.0/0.25` without changing
  the selected mode or any unrelated setting.
- Guarded `RESET ALL` restores English, 75% brightness, PSI, temperature off,
  one-second startup logo, zero offset and the new One Euro defaults.
- Centralized menu-page state and non-overlapping finger-sized controls prevent
  inaccessible or blank navigation states.

Firmware updates deliberately retain existing NVS preferences. A device that
already stores older filter values keeps them after flashing. Use the smoothing
editor's `RESET` to adopt only the new filter tuning, or `RESET ALL` to restore
every project default.

## Validation

The packaged application is the exact binary compiled and flashed to COM6 for
the release hardware boot test:

- Internal RAM: 65,488 bytes.
- Application flash usage: 1,205,525 bytes.
- Packaged application image: 1,205,888 bytes.
- Production diagnostic flags: disabled.
- FT3168 touch controller, XGZP6847D pressure sensor, PSRAM and prebaked
  renderer: initialized successfully on the target.
- Startup-logo duration: one second on the tested device.
- Menu audit: every page, editor, confirmation, close and back route is
  connected; all touch targets are at least 44 pixels; simultaneous extended
  touch areas do not overlap.
- Application SHA-256:
  `badf6b85de793493084736560eefe277a173202e383994496c3d150caa0e96dc`.

Existing saved preferences were intentionally preserved during the hardware
test, confirming that an update does not silently overwrite the user's setup.

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

Normal flashing preserves the NVS settings partition. Use `RESET ALL` from the
menu when the project defaults are wanted. Verify every download against
`SHA256SUMS.txt` before flashing.
