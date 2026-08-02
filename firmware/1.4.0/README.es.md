# Firmware 1.4.0 del reloj de presión de turbo Civic

[Inicio](../../README.es.md) | **Español** | [English](README.md)

La versión 1.4.0 reorganiza la creciente interfaz de ajustes en categorías
claras, permite configurar la duración del logotipo de arranque e incorpora un
reset general protegido. También establece One Euro como modo inicial del
filtro para instalaciones nuevas y reset general, con más amortiguación para
la wastegate electrónica. Conserva el sensor, el bus táctil y el renderizador
pregenerado a 60 Hz probados en hardware en la versión 1.3.0.

## Características destacadas

- Primera pantalla con `EN`/`ES`, brillo, `MEDIDOR`, `PRESION` y
  `RESET GENERAL` protegido.
- Pantalla `MEDIDOR` para PSI/BAR, temperatura del sensor y duración
  persistente del logotipo entre 1 y 10 segundos, inicialmente un segundo.
- Pantalla `PRESION` para los editores persistentes de offset y smoothing.
- One Euro es el nuevo modo inicial para instalaciones nuevas y reset general,
  con `MIN 1,0 Hz` y `BETA 0,25` por kPa para reducir más el jitter.
- `RESTAURAR` en el filtro recupera EMA `0,35` y One Euro `1,0/0,25` sin
  cambiar el modo seleccionado ni ningún ajuste ajeno al filtro.
- `RESET GENERAL` recupera inglés, brillo al 75 %, PSI, temperatura
  desactivada, logotipo de un segundo, offset cero y los nuevos valores One
  Euro.
- El estado centralizado de las pantallas y los controles grandes sin
  solapamiento evitan rutas inaccesibles o pantallas vacías.

Las actualizaciones conservan deliberadamente los ajustes NVS existentes. Si
el dispositivo ya tiene valores anteriores del filtro, los mantiene después
del flasheo. Utiliza `RESTAURAR` en el editor de smoothing para aplicar sólo el
nuevo filtrado, o `RESET GENERAL` para recuperar todos los valores iniciales.

## Validación

La aplicación incluida es exactamente el binario compilado y flasheado en COM6
para la prueba de arranque de esta publicación:

- RAM interna: 65.488 bytes.
- Uso de flash de aplicación: 1.205.525 bytes.
- Tamaño de la imagen de aplicación: 1.205.888 bytes.
- Diagnósticos de producción: desactivados.
- Controlador táctil FT3168, sensor XGZP6847D, PSRAM y renderizador pregenerado:
  inicializados correctamente en la placa.
- Duración del logotipo: un segundo en el dispositivo probado.
- Auditoría del menú: todas las pantallas, editores, confirmaciones, cierres y
  retrocesos están conectados; todos los controles táctiles miden al menos
  44 píxeles; las áreas ampliadas simultáneas no se solapan.
- SHA-256 de la aplicación:
  `badf6b85de793493084736560eefe277a173202e383994496c3d150caa0e96dc`.

Durante la prueba se conservaron intencionadamente los ajustes existentes, lo
que confirma que una actualización no sobrescribe silenciosamente la
configuración del usuario.

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

El flasheo normal conserva la partición NVS de ajustes. Utiliza
`RESET GENERAL` desde el menú cuando quieras recuperar los valores iniciales
del proyecto. Verifica cada descarga mediante `SHA256SUMS.txt` antes de
flashear.
