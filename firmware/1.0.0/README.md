# Civic Boost Gauge 1.0 Firmware

These images were built from the hardware-verified renderer on 2026-07-15.

The pressure-sensor input is WIP until the replacement sensor arrives. The
planned sensor is the XGZP6847D I2C module, `-100 to +300 kPa` bidirectional
variant, powered from `2.5 to 5.5 V` at I2C address `0x6D`.

- `civic-boost-gauge-full.bin`: complete image with bootloader, partition
  table and application. Flash it at address `0x0`.
- `civic-boost-gauge-app.bin`: application-only update for a board that
  already has the correct bootloader and partition table. Flash it at
  address `0x10000`.
- `civic-boost-gauge-demo.gif`: visual demonstration of the gauge sweep.
- `civic-boost-gauge-boot.png`: exact 466x466 boot-screen preview.

Example full flash:

```powershell
esptool.py --chip esp32s3 --port COM6 write_flash 0x0 civic-boost-gauge-full.bin
```

Verify downloads against `SHA256SUMS.txt` before flashing.
