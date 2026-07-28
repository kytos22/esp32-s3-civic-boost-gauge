# Civic Boost Gauge 1.2 Firmware

[Home](../../README.md) | [Español](README.es.md) | **English**

This release completes the bench-tested XGZP6847D digital pressure-sensor
integration while preserving the verified prebaked 60 Hz rendering path.

Highlights:

- XGZP6847D300KPGPN `-100 to +300 kPa` digital sensor on the shared I2C bus.
- Non-blocking pressure acquisition at up to 100 Hz with smooth filtering.
- Stable FT3168 touch polling and active-mode keepalive on the shared bus.
- Factory-calibrated pressure readings without startup or menu zero calibration.
- Optional large sensor-temperature display, disabled by default, refreshed
  every five seconds and controlled by a persistent `TEMP: OFF/ON` menu button.
- Persistent PSI/BAR unit, temperature visibility and display brightness.

The sensor and I2C pull-ups must be powered at 3.3 V.
The release has been bench-tested; full in-car manifold validation is still
recommended before relying on it while driving.

- `civic-boost-gauge-full.bin`: complete image with bootloader, partition table
  and application. Flash it at address `0x0`.
- `civic-boost-gauge-app.bin`: application-only update for a board that already
  has the correct bootloader and partition table. Flash it at `0x10000`.
- `civic-boost-gauge-psi-demo.gif`: PSI sweep generated from the firmware cache.
- `civic-boost-gauge-bar-demo.gif`: BAR sweep generated from the same cache.
- `civic-boost-gauge-boot.png`: exact 466x466 boot-screen preview.

Example full flash:

```powershell
esptool.py --chip esp32s3 --port COM6 write_flash 0x0 civic-boost-gauge-full.bin
```

Verify downloads against `SHA256SUMS.txt` before flashing.
