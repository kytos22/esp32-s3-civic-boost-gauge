# Firmware 1.3.0 del reloj de presión de turbo Civic

[Inicio](../../README.es.md) | **Español** | [English](README.md)

La versión 1.3.0 incorpora controles bilingües persistentes, un offset
deliberado de visualización y smoothing configurable. Conserva la adquisición
del XGZP6847D, el bus táctil FT3168 compartido y el renderizador pregenerado a
60 Hz verificados en hardware en la versión 1.2.1.

## Características destacadas

- Selector persistente `EN`/`ES` que traduce inmediatamente todo el menú sin
  reiniciar.
- Offset persistente de `-1.5` a `+1.5 PSI` en pasos de `0.1 PSI`.
- Modo SIN smoothing temporal de la presión.
- Alfa EMA configurable de `0.05` a `1.00`, inicialmente `0.35`.
- Corte mínimo One Euro configurable de `0.5` a `5.0 Hz`, inicialmente
  `2.0 Hz`, y beta de `0.00` a `3.00` por kPa, inicialmente `1.00`.
- Restauración de los valores iniciales del filtro sin cambiar el modo.
- Documentación completa y enlazada en inglés y español para el proyecto,
  configuración de presión, desarrollo y todas las versiones publicadas.
- Persistencia de las preferencias existentes de PSI/BAR, brillo y temperatura.

El XGZP6847D es un sensor de presión relativa calibrado de fábrica. El propio
sensor ya establece el cero respecto a la atmósfera; el firmware no realiza un
cero automático al arrancar. Mantén el offset en `+0.0 PSI` para representar la
presión física del colector. Úsalo únicamente cuando quieras igualar de forma
deliberada un dato compatible de turbo calculado internamente por el coche.

## Validación

La aplicación incluida es exactamente el binario compilado y flasheado en COM6
para la prueba final de hardware:

- RAM interna: 65.392 bytes.
- Flash de aplicación: 1.201.329 bytes.
- Adquisición del sensor: hasta 100 Hz.
- Planificación de pantalla: 60 Hz.
- Diagnósticos de producción: desactivados.
- XGZP6847D, FT3168, PSRAM y caché del renderizador: detectados correctamente.
- Offset, modos de smoothing y controles bilingües: verificados en hardware.
- SHA-256 de la aplicación:
  `ef682fba4cfa15469d9e818a32ef80c7ab225861a839811e2b9b4fda3f6e60e0`.

## Contenido del paquete

- `civic-boost-gauge-full.bin`: imagen completa con bootloader, tabla de
  particiones y aplicación. Se flashea en la dirección `0x0`.
- `civic-boost-gauge-app.bin`: actualización de sólo aplicación para una placa
  que ya contiene el bootloader y la tabla de particiones correctos. Se flashea
  en `0x10000`.
- `civic-boost-gauge-psi-demo.gif`: barrido PSI generado desde la caché canónica
  del firmware.
- `civic-boost-gauge-bar-demo.gif`: barrido BAR generado desde la misma caché.
- `civic-boost-gauge-boot.png`: vista previa exacta del arranque de 466x466.
- `xgzp6847d-wiring-diagram.png`: diagrama de cableado listo para consultar.
- `xgzp6847d-wiring-diagram.svg`: diagrama vectorial editable.
- `README.md` y `README.es.md`: instrucciones en ambos idiomas.
- `SHA256SUMS.txt`: sumas de todos los archivos de la publicación.

## Resumen de cableado

| Conector I2C Waveshare de cuatro contactos | XGZP6847D |
| --- | --- |
| Pin 1 / SDA / GPIO 47 | Pin 5 / SDA |
| Pin 2 / SCL / GPIO 48 | Pin 1 / SCL |
| Pin 3 / GND | Pin 3 / GND |
| Pin 4 / 3V3 | Pin 4 / VDD |

Deja los pines 2 y 6 del sensor desconectados. Instala un condensador cerámico
no polarizado de 100 nF y al menos 10 V entre los pines 4 y 3, cerca del sensor.
Mantén a 3,3 V el sensor y las resistencias pull-up del I2C compartido.

## Flasheo

Ejemplo de instalación completa:

```powershell
esptool.py --chip esp32s3 --port COM6 write_flash 0x0 civic-boost-gauge-full.bin
```

Ejemplo de actualización de sólo aplicación:

```powershell
esptool.py --chip esp32s3 --port COM6 write_flash 0x10000 civic-boost-gauge-app.bin
```

Verifica cada descarga mediante `SHA256SUMS.txt` antes de flashear.
