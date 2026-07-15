# Golden Firmware

These images were built from the hardware-verified golden source on
2026-07-15.

- `civic-boost-gauge-full.bin`: complete image with bootloader, partition
  table and application. Flash it at address `0x0`.
- `civic-boost-gauge-app.bin`: application-only update for a board that
  already has the correct bootloader and partition table. Flash it at
  address `0x10000`.

Example full flash:

```powershell
esptool.py --chip esp32s3 --port COM6 write_flash 0x0 civic-boost-gauge-full.bin
```

Verify downloads against `SHA256SUMS.txt` before flashing.
