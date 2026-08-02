# Golden Version: Categorized-Settings Firmware

[Home](README.md) | [Español](GOLDEN_VERSION.es.md) | **English**

Date: 2026-08-02
Status: Hardware boot-verified and released as v1.4.0

- Smooth forward and reverse SHOW animation.
- No visible cursor or arc artifacts.
- 60 Hz schedule using a 16,667 us frame period.
- Non-overlapping 16x16 display tiles.
- Prebaked 541-state arc and cursor cache.
- Build-time zlib asset loaded and CRC-validated into PSRAM at startup.
- Maximum measured diagnostic frame: 11.64 ms.
- Diagnostic telemetry is disabled for normal use.
- Brightness slider follows the visually rotated control 1:1.
- Brightness controls verified on the 466x466 FT3168 touch panel.
- XGZP6847D pressure and temperature acquisition verified on the shared I2C bus.
- FT3168 touch keepalive verified alongside 100 Hz sensor acquisition.
- Zero, positive pressure beyond 30 PSI and the available negative-pressure
  syringe range were verified on the bench.
- Persistent PSI/BAR unit, brightness and sensor-temperature visibility were
  verified across restarts.
- Persistent `EN`/`ES` menu translation, pressure offset and selectable
  OFF/EMA/One Euro smoothing controls were verified on hardware.
- Categorized HOME, Gauge and Pressure navigation, every editor route and the
  guarded general-reset confirmation passed structural validation. All touch
  targets are at least 44 pixels and simultaneous extended hit areas do not
  overlap.
- Startup-logo duration is persistent from 1 to 10 seconds and defaults to one
  second. One Euro is the fresh-install and general-reset default with
  `MIN 1.0` and `BETA 0.25`.
- The exact v1.4.0 application image was flashed on the target. Serial startup
  confirmed the FT3168, XGZP6847D, PSRAM and prebaked renderer initialized
  successfully while existing NVS preferences remained intact.
- Complete English and Spanish documentation is connected by same-language
  navigation from every translated section.
- Production diagnostics are disabled while boot, error and recovery messages
  remain available over USB serial.
- Final application and complete flash images are stored in `firmware/1.4.0/`.

The factory-calibrated digital pressure result is used directly without
software zero calibration.

Do not change the rendering, display-transfer, touch or sensor-acquisition paths
without creating a new version and repeating the relevant hardware checks.
