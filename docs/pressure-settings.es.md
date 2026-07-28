# Configuración de presión, offset y smoothing

[Inicio](../README.es.md) | **Español** | [English](pressure-settings.md)

Esta guía explica cómo funcionan el cero atmosférico del XGZP6847D, el offset
opcional de visualización y los modos de smoothing. Son funciones diferentes y
no deben utilizarse como si fueran equivalentes.

## Orden del procesamiento de presión

Cada medición válida del sensor sigue este orden:

1. Leer en kPa la presión del XGZP6847D calibrado de fábrica.
2. Rechazar las mediciones ausentes, antiguas o físicamente inverosímiles.
3. Aplicar en kPa canónicos el modo de smoothing seleccionado.
4. Aplicar el offset opcional de visualización.
5. Convertir el resultado a PSI o BAR.
6. Aplicar la pequeña zona muerta visual alrededor de cero y los límites del
   reloj.

El smoothing y el offset sólo afectan al valor presentado por el reloj.
Ninguno modifica la calibración de fábrica del sensor.

## Cero atmosférico: no hace falta calibrarlo por software

El XGZP6847D300KPGPN es un sensor de presión relativa con rango bidireccional
de `-100 a +300 kPa`. La presión relativa se mide con respecto a la referencia
atmosférica del sensor. El
[datasheet oficial de CFSensor](https://cfsensor.com/wp-content/uploads/2022/11/XGZP6847D-Pressure-Sensor-V3.0.pdf)
identifica `GPN` como la configuración de presión relativa negativa y positiva:

```text
presión relativa = presión medida - presión atmosférica
```

Cuando el lado de medición y el lado de referencia están expuestos a la misma
presión atmosférica, la diferencia de presión es cero. Por ello, el sensor
calibrado de fábrica ya produce un valor próximo a `0 kPa` o `0 PSI`. La
presión atmosférica puede estar alrededor de `14,7 PSI` al nivel del mar, pero
un sensor de presión relativa descuenta físicamente esa referencia: el reloj
no debe mostrar `14,7 PSI` cuando está abierto a la atmósfera.

> **No utilices AJUSTE para poner a cero el sensor al aire.** Un sensor
> correctamente instalado ya establece su cero respecto a la atmósfera.
> AJUSTE sólo sirve para igualar o simular deliberadamente el valor calculado
> por el coche.

El firmware no incluye deliberadamente una rutina de cero automático durante
el arranque ni un botón para calibrar el cero. Esto evita un error grave:

- Con el motor apagado y ambos lados a presión atmosférica, el sensor ya se
  encuentra en su cero físico correcto.
- Con el motor al ralentí, el vacío del colector es una presión relativa
  negativa real y no debe capturarse como un cero nuevo.
- Con el motor bajo carga, la presión de turbo es una presión relativa positiva
  real y tampoco debe capturarse como un cero nuevo.

Un cero automático tomado en el momento equivocado desplazaría todo el rango
de vacío y turbo. Ocultaría una presión real en lugar de calibrar el sensor.

Antes de diagnosticar un problema de cero, desconecta del colector el manguito
de medición y déjalo abierto a la misma atmósfera que la referencia del sensor.
Espera a que la lectura se estabilice. Comprueba que no haya presión atrapada,
restricciones ni líquido en el manguito y confirma que se utilizan el rango y
el factor de transferencia correctos.

El reloj muestra deliberadamente como cero los valores menores de `±0,25 PSI`
o `±0,02 bar`. Se trata únicamente de una zona muerta visual para impedir que
el número parpadee alrededor de cero. No es un cero automático y no modifica
la lectura del sensor.

## Para qué sirve el offset

`AJUSTE` es una corrección deliberada y persistente de visualización entre
`-1,5 PSI` y `+1,5 PSI`, en pasos de `0,1 PSI`. Su valor predeterminado es
`+0.0 PSI`.

Para representar con la máxima fidelidad la presión física del colector,
mantén el offset a cero. El propio sensor ya establece el cero atmosférico, por
lo que normalmente no hace falta aplicar un offset para obtener cero al aire.

Utiliza el offset únicamente cuando quieras alinear el reloj con el valor de
turbo que el coche calcula internamente o presenta mediante un canal compatible
de la ECU/OBD. El coche puede aplicar su propia estimación barométrica,
corrección del sensor, redondeo o estrategia de visualización. El offset permite
que este reloj simule intencionadamente ese valor cuando la diferencia es un
desplazamiento pequeño y constante.

Ejemplos:

- El reloj marca `0,0 PSI` a presión atmosférica y el coche muestra
  intencionadamente `-0,3 PSI`: utiliza `-0,3 PSI` si prefieres igualar el
  valor del coche.
- El reloj marca repetidamente `10,2 PSI` y el valor de turbo del coche indica
  `10,6 PSI` bajo la misma condición estable: un offset de `+0,4 PSI` puede
  alinearlos.
- La diferencia es pequeña con poco turbo, pero grande con mucho turbo: no la
  corrijas mediante offset. Investiga las unidades, presión absoluta frente a
  relativa, recorrido del manguito, rango del sensor y fuente de datos.

Compara únicamente valores estables que representen el mismo tipo de presión.
Un valor `MAP` sin procesar suele ser presión absoluta e incluye la presión
atmosférica; un valor `boost` calculado por la ECU suele ser presión relativa.
El offset de `±1,5 PSI` no está diseñado para convertir MAP absoluta en turbo.

El offset se aplica después del smoothing y antes de convertir a la unidad
seleccionada. Se edita en PSI, pero genera la corrección equivalente cuando el
reloj está configurado en BAR. El botón `RESTAURAR` del editor devuelve el
offset a `+0.0 PSI`.

## Para qué sirve el smoothing

El sensor puede producir pequeñas variaciones entre muestras incluso cuando la
presión real permanece casi constante. Un número y una aguja rápidos pueden
hacer que este ruido normal parezca movimiento. El smoothing reduce ese
movimiento visible combinando mediciones a lo largo del tiempo.

El smoothing no:

- cambia la referencia atmosférica del sensor;
- corrige un error de offset o de escala;
- reduce la adquisición del sensor, que se mantiene hasta 100 Hz;
- reduce la planificación del renderizador, que permanece a 60 Hz;
- sustituye las comprobaciones de lecturas inválidas, antiguas o inverosímiles.

Abre el menú mediante una pulsación larga sobre el logotipo Civic. Selecciona
`EN` o `ES` en la parte superior si es necesario, pulsa `SMOOTH`/`SUAV.` y
elige `OFF`/`SIN`, `EMA` o `1 EURO`. El idioma, el modo y todos los parámetros
activos quedan guardados tras reiniciar.

## SIN: respuesta inmediata

`SIN` no aplica smoothing temporal. Cada muestra válida de presión se convierte
directamente en el valor de presión actual. Siguen aplicándose las
comprobaciones de validez, el offset opcional, la zona muerta visual y los
límites del reloj.

Utiliza SIN para:

- comparar el comportamiento directo del sensor con los modos filtrados;
- comprobar si un movimiento procede de la presión real o del filtro;
- dar prioridad a la respuesta máxima sobre la estabilidad visual.

SIN puede hacer que el último decimal y la aguja se muevan más. Es normal y no
indica necesariamente un problema del sensor.

## EMA: sencillo y predecible

EMA es una media móvil exponencial:

```text
filtrado = filtrado anterior + alfa * (muestra - filtrado anterior)
```

`ALFA` controla qué parte de cada muestra nueva se acepta:

| Alfa | Resultado |
| --- | --- |
| `0,05–0,15` | Muy estable, pero lento al seguir cambios de presión |
| `0,20–0,30` | Smoothing intenso con un retardo moderado |
| `0,35` | Respuesta equilibrada predeterminada |
| `0,40–0,60` | Respuesta más rápida con menos smoothing |
| `0,65–0,90` | Muy rápido; smoothing ligero |
| `1,00` | Matemáticamente equivale a aceptar directamente cada muestra |

El rango disponible es `0,05–1,00` en pasos de `0,05`. Un alfa menor produce
más smoothing y más retardo. Un alfa mayor genera menos smoothing y una
visualización más rápida, pero también más activa.

EMA resulta útil cuando se desea una respuesta fija y fácil de entender.
Ajusta un paso cada vez: reduce alfa si la lectura se mueve demasiado o
aumenta alfa si la aguja parece retrasada.

## One Euro: estable en reposo y rápido durante el turbo

El filtro One Euro cambia su frecuencia de corte según la velocidad con la que
se mueve la presión:

```text
corte de presión = corte mínimo + beta * velocidad de cambio de la presión
```

Con presión casi constante, el corte permanece cerca del mínimo configurable y
suprime las pequeñas fluctuaciones. Durante un cambio rápido de vacío o turbo,
el corte aumenta automáticamente para que el reloj siga el evento con menos
retardo.

### Frecuencia de corte mínima

`MIN` controla el comportamiento durante presiones estables y cambios lentos.

- `MIN` menor: lectura más estable en reposo, pero más retardo durante cambios
  pequeños o lentos.
- `MIN` mayor: sigue mejor los cambios pequeños, pero permite más ruido visible.

El rango es `0,5–5,0 Hz` en pasos de `0,1 Hz`. El valor predeterminado es
`2,0 Hz`. Se expresa en hercios porque es la frecuencia de corte del filtro
paso bajo, no la frecuencia de actualización del sensor ni de la pantalla.

### Beta

`BETA` controla cuánto se relaja el filtro durante cambios rápidos de presión.

- `0,00`: desactiva la parte adaptativa y deja un filtro paso bajo fijo en la
  frecuencia mínima seleccionada.
- Beta mayor: respuesta más rápida ante cambios repentinos de vacío o turbo.
- Beta excesiva: el ruido agudo y los pulsos manuales de presión pueden
  hacerse más visibles.

El rango es `0,00–3,00` por kPa en pasos de `0,05`. El valor predeterminado es
`1,00`. El corte del filtro de la derivada permanece fijo en `1 Hz` para
mantener estable la adaptación y no se expone en el menú.

## Procedimiento recomendado de ajuste

Empieza pulsando `RESTAURAR`. Esto recupera alfa EMA `0,35`, corte mínimo One
Euro `2,0 Hz` y beta `1,00` sin cambiar el modo seleccionado.

Para EMA:

1. Empieza con alfa `0,35`.
2. Observa el reloj con una presión estable.
3. Reduce alfa en pasos de `0,05` si la lectura se mueve demasiado.
4. Genera un cambio de presión representativo.
5. Aumenta ligeramente alfa si la respuesta parece retrasada.

Para One Euro:

1. Empieza con `MIN 2,0 Hz` y `BETA 1,00`.
2. Con presión estable, reduce `MIN` hasta lograr la estabilidad deseada.
3. Aplica un cambio de presión rápido y representativo.
4. Aumenta `BETA` hasta obtener una respuesta transitoria suficientemente
   inmediata.
5. Si aparece movimiento brusco durante los cambios, reduce ligeramente beta.
6. Si los cambios lentos y pequeños parecen retrasados, aumenta ligeramente el
   corte mínimo.

Modifica un solo valor cada vez. Una jeringuilla sirve para comprobar dirección,
rango y comportamiento general, pero normalmente produce transiciones más
lentas y menos representativas que un motor bajo carga. Confirma el ajuste
final en el coche sin manipular el reloj mientras conduces.

## Selección rápida de modo

| Objetivo | Punto de partida sugerido |
| --- | --- |
| Examinar directamente el sensor | SIN |
| Smoothing sencillo y ligero | EMA `0,50` |
| Smoothing sencillo y equilibrado | EMA `0,35` |
| Máxima estabilidad mediante EMA | EMA `0,15–0,25` |
| Estable en reposo y rápido con turbo | One Euro `MIN 2,0`, `BETA 1,00` |

Son puntos de partida, no valores de calibración. El mejor ajuste depende del
volumen del manguito, la instalación, el comportamiento del motor y las
preferencias personales.

## Persistencia y comportamiento de RESTAURAR

El modo seleccionado y sus valores se guardan de forma persistente:

- idioma de la interfaz: EN o ES;
- modo de smoothing: SIN, EMA o One Euro;
- alfa de EMA;
- frecuencia de corte mínima de One Euro;
- beta de One Euro.

`RESTAURAR` recupera los tres parámetros iniciales del filtro, pero mantiene
deliberadamente el modo seleccionado. Cambiar a SIN no borra los valores
guardados de EMA ni de One Euro, de modo que se puede volver a cualquiera de
los modos filtrados sin introducir de nuevo su configuración.
