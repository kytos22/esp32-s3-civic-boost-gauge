# Versión de referencia: firmware final con sensor digital

[Inicio](README.es.md) | **Español** | [English](GOLDEN_VERSION.md)

Fecha: 2026-07-28
Estado: verificado en hardware y publicado como v1.3.0

- Animación SHOW fluida tanto al avanzar como al retroceder.
- Sin artefactos visibles en el cursor ni en el arco.
- Planificación a 60 Hz mediante un periodo de 16.667 us por fotograma.
- Bloques de pantalla de 16x16 sin solapamiento.
- Caché pregenerada de arco y cursor con 541 estados.
- Recurso zlib generado al compilar, cargado y validado mediante CRC en PSRAM
  durante el arranque.
- Fotograma de diagnóstico máximo medido: 11,64 ms.
- Telemetría de diagnóstico desactivada durante el uso normal.
- El deslizador de brillo sigue exactamente el control rotado visualmente.
- Controles de brillo verificados en el panel táctil FT3168 de 466x466.
- Adquisición de presión y temperatura del XGZP6847D verificada en el bus I2C
  compartido.
- Mantenimiento del FT3168 verificado junto con la adquisición del sensor a
  100 Hz.
- Verificados en banco el cero, la presión positiva superior a 30 PSI y el
  rango de presión negativa disponible con la jeringuilla.
- Persistencia de PSI/BAR, brillo y visibilidad de la temperatura verificada
  tras reiniciar.
- Traducción persistente `EN`/`ES`, offset y controles de smoothing
  SIN/EMA/One Euro verificados en hardware.
- Documentación completa en inglés y español conectada mediante navegación en
  el mismo idioma desde cada sección traducida.
- Diagnósticos de producción desactivados; se conservan por USB serie los
  mensajes de arranque, error y recuperación.
- Imágenes finales de aplicación y flash completo almacenadas en
  `firmware/1.3.0/`.

El resultado digital calibrado de fábrica se utiliza directamente, sin
calibración de cero por software.

No modifiques las rutas de renderizado, transferencia de pantalla, control
táctil o adquisición del sensor sin crear una nueva versión y repetir las
pruebas de hardware correspondientes.
