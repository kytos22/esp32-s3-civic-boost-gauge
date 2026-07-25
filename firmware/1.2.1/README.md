# Civic Boost Gauge 1.2.1 Firmware

This is the production-hardened release of the bench-tested XGZP6847D digital
boost gauge. It preserves the verified 60 Hz prebaked renderer, 100 Hz sensor
acquisition and stable shared-bus FT3168 touch behavior from version 1.2.0.

Highlights:

- Visible `ERR` warning after one second without a valid pressure reading.
- Automatic recovery when XGZP6847D communication returns.
- Visible `MAX` warning above the 30 PSI display range, with hysteresis to
  prevent warning flicker near the limit.
- Continuous sensor and touch telemetry disabled in production.
- Boot, error and recovery messages retained over USB serial.
- Factory-calibrated pressure readings without software zero calibration.
- Persistent PSI/BAR unit, temperature visibility and display brightness.

The sensor and I2C pull-ups must be powered at 3.3 V. This release has been
bench-tested; full in-car manifold, heat, vibration and electrical-noise
validation remains recommended before relying on it while driving.

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
