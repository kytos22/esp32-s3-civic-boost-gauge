# AGENTS.md

[Inicio](README.es.md) | **Español** | [English](AGENTS.md)

Este archivo es la guía operativa para los agentes de programación que trabajan
en este repositorio. Se aplica a todo el proyecto. Utiliza el idioma del usuario
durante la conversación, pero conserva la documentación y las notas de versión
en inglés salvo que el usuario solicite otra cosa.

## Estado del proyecto

Este es un reloj de presión de turbo probado en hardware para la Waveshare
ESP32-S3-Touch-AMOLED-1.43, con AMOLED SH8601 de 466x466 y control táctil
FT3168. La placa se monta con el conector USB orientado hacia abajo. La versión
`v1.4.0` es la referencia de producción actual.

El renderizador visual se considera estable y sin artefactos. Lee estos
archivos antes de modificar el renderizado, la transferencia de pantalla, las
transformaciones táctiles o los recursos:

1. [`README.es.md`](README.es.md)
2. [`GOLDEN_VERSION.es.md`](GOLDEN_VERSION.es.md)
3. `src/main.cpp`
4. `lib/Mylibrary/pin_config.h`

La adquisición del sensor XGZP6847D está integrada y probada en banco. Conserva
tanto el renderizador funcional como el comportamiento validado del bus
compartido por sensor y control táctil.

## Mapa del repositorio

- `src/main.cpp`: firmware, secuencia de arranque, interfaz táctil, unidades de
  presión, adquisición del sensor, renderizador pregenerado y transferencia a
  pantalla.
- `platformio.ini`: compilación ESP32-S3, CPU a 240 MHz, flash/QSPI a 80 MHz,
  PSRAM OPI y asignación de núcleo.
- `lib/Mylibrary/pin_config.h`: asignación de pines autoritativa de la placa.
- `src/prebaked_visuals.*`: imágenes estáticas dispersas del reloj y arranque.
- `src/prebaked_gauge_cache.*`: representación comprimida y generada de la
  caché dinámica de arco y cursor.
- `src/prebaked_cache_format.h`: ABI de caché compartida por firmware y
  herramientas.
- `tools/prebaked_gauge_cache.bin`: caché dinámica canónica sin comprimir.
- `tools/prebaked_capture.bin`: fotogramas estáticos canónicos capturados.
- `tools/platformio_prebuild.py`: actualiza la fuente de caché comprimida cuando
  cambia la caché canónica o su generador.
- `tools/create_demo_gif.js`: genera GIF de publicación PSI y BAR fieles a los
  recursos, la fuente numérica y la caché dinámica reales.
- `docs/xgzp6847d-wiring-diagram.*`: diagramas PNG y SVG revisados para la
  instalación del sensor de producción.
- `firmware/<version>/`: binarios, vistas previas, GIF y sumas de comprobación.
- `lib/`: bibliotecas incluidas con la compilación verificada; se consideran
  versiones fijadas.

## Configuración de hardware

- Pantalla: SH8601, 466x466, QSPI.
- Táctil: FT3168 por I2C.
- Pines I2C: SDA GPIO 47 y SCL GPIO 48.
- Sensor de presión: XGZP6847D300KPGPN I2C, bidireccional de
  `-100 a +300 kPa`, dirección `0x6D`, alimentado a 3,3 V y factor V2.x `K=16`.
- Conector I2C Waveshare SH1.0 de cuatro contactos: pin 1 SDA, pin 2 SCL,
  pin 3 GND y pin 4 3V3.
- Pines del XGZP6847D vistos desde arriba: pin 1 SCL, pin 2 NC, pin 3 GND,
  pin 4 VDD, pin 5 SDA y pin 6 NC. Deja ambos NC sin conectar.
- Coloca un condensador cerámico no polarizado de 100 nF y al menos 10 V entre
  VDD y GND, físicamente cerca del sensor.
- CPU: 240 MHz.
- `setup()`/`loop()` y los eventos de Arduino se ejecutan en el núcleo 1.
  Ninguna tarea del proyecto utiliza explícitamente el núcleo 0.

No expongas ningún GPIO del ESP32-S3 a más de 3,3 V. Mantén a 3,3 V las
resistencias pull-up del I2C compartido y la alimentación del sensor.

## Arquitectura de renderizado

El reloj no se vuelve a dibujar de forma convencional mediante LVGL en cada
fotograma.

1. El logotipo de arranque se muestra inmediatamente.
2. Se reservan en PSRAM dos framebuffers RGB565 completos.
3. Mientras se muestra la pantalla de arranque configurable, la caché
   comprimida de 4,90 MB se valida y descomprime directamente en PSRAM. La
   duración persistente va de 1 a 10 segundos y comienza en un segundo.
4. El fotograma estático proporciona marcas, etiquetas, unidad y logotipo Civic.
5. Una caché de 541 estados proporciona el arco móvil y el cursor rojo en pasos
   de 0,5 grados.
6. Sólo se transfieren bloques sucios de pantalla de 16x16 sin solapamiento.
7. Los glifos del valor central se rasterizan desde máscaras de opacidad
   preprocesadas con un anclaje decimal fijo.

El objetivo del reloj se planifica cada 16.667 microsegundos, equivalentes a
60 Hz. El texto central se actualiza cada 33 ms. PSI y BAR comparten la misma
caché dinámica. BAR sólo modifica etiquetas estáticas seguras y asigna el
sensor directamente a `-1..2 BAR`; PSI utiliza `-15..30 PSI`.

LVGL gestiona el menú táctil y sus eventos. El reloj visible, valor central,
arco y cursor utilizan el renderizador personalizado mediante framebuffer.

## Invariantes del renderizador

Estas reglas protegen el resultado verificado en hardware:

- No gires el framebuffer completo en el callback de transferencia ni en el
  controlador. La geometría ya está generada en orientación nativa para el
  montaje con USB hacia abajo.
- No cambies centro, ángulos, radios, grosor del arco, geometría del cursor,
  marcas exteriores, degradado, paso de caché, tamaño de bloque ni mezcla
  RGB565 sin regenerar y volver a validar la caché dinámica.
- Los píxeles del cursor incluyen su fondo estático esperado. Modificar píxeles
  estáticos cerca de su recorrido puede reintroducir líneas y artefactos.
- No sustituyas arco ni cursor almacenados por geometría LVGL por fotograma.
- No reserves memoria, decodifiques recursos, calcules trigonometría ni generes
  geometría dentro del bucle normal de fotogramas.
- Mantén anclado el punto decimal. No vuelvas a centrar la cadena completa,
  porque produce movimiento lateral visible.
- Conserva PSI como unidad inicial. Guarda la selección en el espacio
  Preferences `boost-gauge`, clave `unit`, y escribe sólo cuando cambie.
- Conserva el 75 % como brillo inicial. Guarda los cambios en `brightness`,
  guarda el deslizador sólo al soltarlo y evita escrituras redundantes.
- Conserva inglés como idioma inicial. Mantén el selector persistente `EN`/`ES`
  en la clave `language`, actualiza inmediatamente todas las etiquetas visibles
  sin reiniciar y utiliza texto ASCII en la interfaz porque las fuentes LVGL
  activas no incluyen todos los caracteres acentuados del español.
- Mantén desactivado inicialmente el modo SHOW/demostración.
- Las compilaciones de producción deben conservar a `0`
  `ENABLE_PERF_TELEMETRY`, `ENABLE_RENDER_DIAGNOSTICS`,
  `ENABLE_HARDWARE_TELEMETRY`, `DUMP_PREBAKED_FRAMES` y `DUMP_BAKED_CACHE`.
- Conserva el control persistente de pantalla de arranque entre 1 y 10 segundos
  mediante la clave `boot-seconds`, su valor inicial de un segundo y el barrido
  suave de subida y bajada. La transición a LIVE no debe parpadear a presión
  máxima.

Si un cambio visual puede permanecer estrictamente dentro de una zona estática
que nunca intersecta el arco ni el cursor, modifica sólo el fotograma estático.
En caso de duda, regenera la caché y prueba el barrido completo en ambos
sentidos.

## Invariantes táctiles

- Las coordenadas directas del FT3168 ya son correctas para la pantalla nativa.
- Sigue la lectura de Waveshare: escribe modo normal en `0x00`, consulta el
  número de dedos en `0x02` y lee juntos los cuatro bytes X/Y desde `0x03`.
  Mantén el modo activo `0xA5`, desactiva el monitor automático `0x86` y
  actualiza ambos cada segundo sobre el bus compartido a 300 kHz. La
  inicialización aislada de la demostración acababa dejando de responder. La
  demo de la placa no declara interrupción táctil; no asignes GPIO 16 a
  `TP_INT`.
- El menú está rotado visualmente `900` décimas de grado de LVGL.
- El deslizador de brillo proyecta el toque sobre su eje visual transformado.
  No lo sustituyas por una consulta X/Y sin transformar.
- Mantén controles aptos para el dedo y evita zonas de clic ampliadas que se
  solapen.
- Prueba toque Civic, pulsación larga Civic, cierre, las rutas
  INICIO/Medidor/Presión y ambos botones de vuelta. Verifica que `EN`/`ES`,
  brillo y reset general protegido permanezcan en INICIO; prueba PSI/BAR,
  temperatura y duración de arranque bajo Medidor; prueba acceso a offset y
  smoothing, todos los controles de sus editores y todas las rutas de
  cierre/cancelación bajo Presión. Verifica ambos idiomas, persistencia y que
  ninguna ruta deje una pantalla vacía o negra.

## Trabajo sobre el sensor

La integración del XGZP6847D inicia conversiones combinadas mediante `0x30`,
lee presión de 24 bits con signo y temperatura de 16 bits con signo, y aplica
la conversión documentada `K=16` para el rango positivo de `+300 kPa`. El
muestreo se ejecuta hasta 100 Hz mediante una máquina de estados no bloqueante
mientras la pantalla permanece a 60 Hz. Presión y temperatura se leen como un
bloque contiguo de cinco bytes.

- Mantén el filtrado en kPa canónicos y convierte sólo en el límite sensor/UI.
  Conserva tres modos persistentes: SIN pasa la muestra válida sin smoothing;
  EMA expone alfa `0,05..1,00` en pasos de 0,05, con valor inicial 0,35; One
  Euro utiliza el intervalo real, corte de derivada fijo en 1 Hz, corte mínimo
  `0,5..5,0 Hz`, inicialmente 1,0, y beta `0,00..3,00` por kPa, inicialmente
  0,25, en pasos de 0,05. One Euro es el modo inicial para instalaciones nuevas
  y reset general. Guarda `smooth-mode`, `ema-alpha`, `oe-min-hz` y `oe-beta`,
  conserva la migración de `smoothing` sin sobrescribir un modo ya guardado. El
  reset del filtro sólo restaura sus parámetros; el reset general protegido
  restaura todo el espacio de ajustes a los valores del proyecto.
- Usa el valor calibrado de fábrica sin cero automático. Conserva el offset
  opcional persistente en décimas de PSI mediante `psi-offset`, limitado a
  `-1,5..+1,5 PSI`, inicialmente cero y convertido mediante kPa canónicos antes
  de PSI/BAR. Conserva temperatura opcional cada cinco segundos y LIVE/SHOW.
- Mantén no bloqueantes las lecturas ausentes, antiguas, inválidas o
  inverosímiles.
- Conserva `ERR` después de un segundo sin lectura válida y `MAX` por encima de
  30 PSI.
- Mantén la lectura del FT3168 agrupada en las dos transacciones de Waveshare
  para que táctil y presión compartan el bus con fiabilidad.
- Las pruebas de banco verificaron cero, temperatura, presión positiva por
  encima de 30 PSI, rango negativo disponible y estabilidad táctil compartida.
  Sigue siendo recomendable validar colector, calor, vibración y ruido
  eléctrico en el coche.

## Compilación y flasheo

Comandos habituales:

```powershell
pio run
pio run --target upload --upload-port COM6
```

Si `pio` no está en PATH en Windows:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --target upload --upload-port COM6
```

Referencia esperada para la compilación de publicación `v1.4.0`:

- RAM interna: aproximadamente 65,5 KB.
- Flash: aproximadamente 1,206 MB de la partición de aplicación de 6,55 MB.
- Los avisos de Arduino_GFX sobre `SPI_MAX_PIXELS_AT_ONCE` son anteriores;
  investiga cualquier aviso nuevo.

## Lista de validación

Para cada cambio de firmware:

1. Ejecuta `git diff --check`.
2. Ejecuta `pio run` en modo de publicación y registra RAM/flash.
3. Confirma que los diagnósticos de producción están desactivados.
4. Si cambia el hardware, flashea COM6 y prueba la AMOLED real.

Para cambios de renderizador, táctil o unidad, verifica además:

- El logotipo permanece visible mientras se cargan los recursos.
- El barrido alcanza el máximo, vuelve suavemente al mínimo y entra en LIVE.
- SHOW es fluido en ambos sentidos sobre todo el rango.
- No aparecen artefactos en valor central, logotipo, marcas ni cursor.
- El cursor rojo permanece completo por todo el arco.
- Valores, etiquetas, tipografía y decimal de PSI/BAR son correctos.
- La unidad persiste tras reiniciar.
- El toque del brillo coincide exactamente con el deslizador visual.

Usa telemetría temporal sólo para medir y vuelve a `0` antes de recompilar.

## Flujo de la caché pregenerada

`tools/prebaked_gauge_cache.bin` es la fuente de verdad del renderizado dinámico.
Las compilaciones normales sólo la comprimen e incorporan.

Después de cualquier cambio de geometría o color:

1. Guarda o confirma el renderizador actual.
2. Establece `DUMP_BAKED_CACHE` a `1`.
3. Compila y carga el firmware exportador.
4. Ejecuta `python tools/capture_prebaked_cache.py --port COM6`.
5. Devuelve `DUMP_BAKED_CACHE` a `0`.
6. Compila normalmente para regenerar `src/prebaked_gauge_cache.*`.
7. Prueba en hardware el barrido completo en ambos sentidos.

No edites manualmente los arrays generados en
`src/prebaked_gauge_cache.cpp` ni `src/prebaked_visuals.cpp`.

## Flujo de GIF y publicación

ImageMagick es necesario para generar GIF. Genera ambas unidades con:

```powershell
node tools/create_demo_gif.js 1.4.0
```

Cambia el argumento para una versión nueva. El generador debe utilizar captura,
caché y fuentes canónicas. La salida normal contiene 601 fotogramas de 466x466.

Antes de publicar:

- Compila producción desde el commit previsto.
- Flashea y prueba exactamente la aplicación que se empaquetará.
- Incluye imágenes de aplicación (`0x10000`) y completa (`0x0`).
- Incluye GIF, PNG de arranque, diagramas, README y `SHA256SUMS.txt`.
- Verifica la aplicación mediante `esptool.py image_info`.
- Confirma antes de probar que el hash coincide con
  `.pio/build/src/firmware.bin`; conserva después exactamente ese binario.
  Una recompilación posterior puede diferir por metadatos u orden de enlace y
  no debe sustituirlo sin otra prueba.
- Mantén notas en inglés y describe con precisión las pruebas del sensor; no
  declares completa una integración antes de probarla en hardware.

## Disciplina de cambios

- Limita el alcance. No refactorices el renderizador verificado al añadir sensor
  o documentación.
- No actualices bibliotecas ni plataforma sólo porque exista una versión nueva;
  mide y prueba esos cambios por separado.
- Nunca sobrescribas la última publicación válida. Crea directorio y etiqueta
  nuevos.
- Conserva cambios del usuario y evita comandos Git destructivos.
- Confirma hitos relevantes antes de experimentar con renderizador o caché.
