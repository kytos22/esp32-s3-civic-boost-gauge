# Firmware 1.2.1 del reloj de presión de turbo Civic

[Inicio](../../README.es.md) | **Español** | [English](README.md)

Esta es la versión reforzada para producción del reloj digital de turbo con
XGZP6847D probado en banco. Conserva el renderizador pregenerado a 60 Hz, la
adquisición del sensor a 100 Hz y el comportamiento táctil estable del FT3168
sobre el bus compartido de la versión 1.2.0. Las imágenes de aplicación y
completa incluidas son exactamente los binarios utilizados durante la
confirmación final en hardware.

Características destacadas:

- Aviso visible `ERR` después de un segundo sin una lectura válida.
- Recuperación automática cuando vuelve la comunicación con el XGZP6847D.
- Aviso visible `MAX` por encima de 30 PSI, con histéresis para evitar
  parpadeos cerca del límite.
- Telemetría continua de sensor y táctil desactivada en producción.
- Mensajes de arranque, error y recuperación conservados por USB serie.
- Lecturas de presión calibradas de fábrica sin calibración de cero por
  software.
- Persistencia de PSI/BAR, visibilidad de temperatura y brillo.

El sensor y las resistencias pull-up del I2C deben alimentarse a 3,3 V. Esta
versión se probó en banco a presión cero, dentro del rango negativo disponible
con la jeringuilla y por encima del límite visual de 30 PSI. Sigue siendo
recomendable validar completamente el colector, la temperatura, las vibraciones
y el ruido eléctrico en el coche antes de depender de ella durante la
conducción.

- `civic-boost-gauge-full.bin`: imagen completa con bootloader, tabla de
  particiones y aplicación. Se flashea en `0x0`.
- `civic-boost-gauge-app.bin`: actualización de sólo aplicación para una placa
  que ya contiene el bootloader y la tabla de particiones correctos. Se flashea
  en `0x10000`.
- `civic-boost-gauge-psi-demo.gif`: barrido PSI generado desde la caché.
- `civic-boost-gauge-bar-demo.gif`: barrido BAR generado desde la misma caché.
- `civic-boost-gauge-boot.png`: vista previa exacta de la pantalla de arranque.
- `xgzp6847d-wiring-diagram.png`: diagrama de cableado listo para ver.
- `xgzp6847d-wiring-diagram.svg`: diagrama vectorial editable.

Resumen de cableado:

| Conector I2C Waveshare de cuatro contactos | XGZP6847D |
| --- | --- |
| Pin 1 / SDA / GPIO 47 | Pin 5 / SDA |
| Pin 2 / SCL / GPIO 48 | Pin 1 / SCL |
| Pin 3 / GND | Pin 3 / GND |
| Pin 4 / 3V3 | Pin 4 / VDD |

Deja los pines 2 y 6 del sensor desconectados. Instala un condensador cerámico
no polarizado de 100 nF y al menos 10 V entre los pines 4 y 3, cerca del sensor.

Ejemplo de flash completo:

```powershell
esptool.py --chip esp32s3 --port COM6 write_flash 0x0 civic-boost-gauge-full.bin
```

Verifica las descargas mediante `SHA256SUMS.txt` antes de flashear.
