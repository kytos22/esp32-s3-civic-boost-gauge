# Civic Boost Gauge 1.1 Firmware

This release adds a persistent PSI/BAR selector while preserving the verified
prebaked 60 Hz rendering path.

The pressure-sensor input remains WIP until the replacement sensor arrives. The
planned sensor is the XGZP6847D I2C module, `-100 to +300 kPa` bidirectional
variant, powered from `2.5 to 5.5 V` at I2C address `0x6D`.

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
