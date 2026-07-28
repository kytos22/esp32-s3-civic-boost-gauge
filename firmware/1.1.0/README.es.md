# Firmware 1.1 del reloj de presión de turbo Civic

[Inicio](../../README.es.md) | **Español** | [English](README.md)

Esta versión añadió un selector persistente PSI/BAR y conservó la ruta de
renderizado pregenerado a 60 Hz ya verificada.

La entrada del sensor de presión seguía en desarrollo hasta recibir el sensor
de sustitución. El sensor previsto era el módulo I2C XGZP6847D, variante
bidireccional de `-100 a +300 kPa`, alimentado entre `2,5 y 5,5 V` y con
dirección I2C `0x6D`.

- `civic-boost-gauge-full.bin`: imagen completa con bootloader, tabla de
  particiones y aplicación. Se flashea en la dirección `0x0`.
- `civic-boost-gauge-app.bin`: actualización de sólo aplicación para una placa
  que ya contiene el bootloader y la tabla de particiones correctos. Se flashea
  en `0x10000`.
- `civic-boost-gauge-psi-demo.gif`: barrido PSI generado desde la caché.
- `civic-boost-gauge-bar-demo.gif`: barrido BAR generado desde la misma caché.
- `civic-boost-gauge-boot.png`: vista previa exacta de la pantalla de arranque
  de 466x466.

Ejemplo de flash completo:

```powershell
esptool.py --chip esp32s3 --port COM6 write_flash 0x0 civic-boost-gauge-full.bin
```

Verifica las descargas mediante `SHA256SUMS.txt` antes de flashear.
