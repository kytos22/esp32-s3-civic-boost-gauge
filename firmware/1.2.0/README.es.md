# Firmware 1.2 del reloj de presión de turbo Civic

[Inicio](../../README.es.md) | **Español** | [English](README.md)

Esta versión completó la integración del sensor digital de presión XGZP6847D,
probada en banco, y conservó la ruta verificada de renderizado pregenerado a
60 Hz.

Características destacadas:

- Sensor digital XGZP6847D300KPGPN de `-100 a +300 kPa` en el bus I2C
  compartido.
- Adquisición de presión no bloqueante hasta 100 Hz con filtrado suave.
- Lectura táctil FT3168 estable y mantenimiento del modo activo en el bus
  compartido.
- Lecturas de presión calibradas de fábrica, sin calibración de cero durante el
  arranque ni desde el menú.
- Temperatura grande opcional, desactivada inicialmente, actualizada cada cinco
  segundos y controlada por el botón persistente `TEMP: OFF/ON`.
- Persistencia de unidad PSI/BAR, visibilidad de temperatura y brillo.

El sensor y las resistencias pull-up del I2C deben alimentarse a 3,3 V. Esta
versión se probó en banco; seguía recomendándose una validación completa en el
colector del coche antes de depender de ella durante la conducción.

- `civic-boost-gauge-full.bin`: imagen completa con bootloader, tabla de
  particiones y aplicación. Se flashea en `0x0`.
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
