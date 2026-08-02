# Pressure Setup, Offset and Smoothing

[Home](../README.md) | [Español](pressure-settings.es.md) | **English**

This guide explains how the XGZP6847D atmospheric zero, the optional display
offset and the pressure-smoothing modes work. These are separate functions and
should not be used interchangeably.

## Pressure processing order

Every valid sensor measurement follows this order:

1. Read the factory-calibrated XGZP6847D pressure in kPa.
2. Reject missing, stale or implausible measurements.
3. Apply the selected smoothing mode in canonical kPa.
4. Apply the optional display offset.
5. Convert the result to PSI or BAR.
6. Apply the small visual zero deadband and the gauge display limits.

Smoothing and offset only affect the value presented by the gauge. Neither one
changes the sensor's factory calibration.

## Atmospheric zero: no software calibration is required

The XGZP6847D300KPGPN is a gauge-pressure sensor with a bidirectional
`-100 to +300 kPa` range. Gauge pressure is pressure measured relative to the
sensor's atmospheric reference. The
[official CFSensor datasheet](https://cfsensor.com/wp-content/uploads/2022/11/XGZP6847D-Pressure-Sensor-V3.0.pdf)
identifies `GPN` as its negative-and-positive gauge-pressure configuration:

```text
gauge pressure = measured pressure - atmospheric pressure
```

When the measurement side and the reference side are exposed to the same
atmospheric pressure, the pressure difference is zero. The factory-calibrated
sensor therefore already produces a value close to `0 kPa` or `0 PSI`.
Atmospheric pressure itself may be roughly `14.7 PSI` at sea level, but a
gauge-pressure sensor subtracts that atmospheric reference physically; the
display should not show `14.7 PSI` while open to the atmosphere.

> **Do not use OFFSET to zero the sensor in free air.** A correctly installed
> sensor already establishes its zero relative to atmosphere. OFFSET is only
> for intentionally matching or simulating the value calculated by the car.

The firmware intentionally has no startup auto-zero routine and no zero
calibration button. This avoids a dangerous measurement error:

- If the engine is off and both sides are at atmosphere, the sensor is already
  at its correct physical zero.
- If the engine is running at idle, manifold vacuum is a real negative gauge
  pressure and must not be captured as a new zero.
- If the engine is under load, boost is a real positive gauge pressure and
  must not be captured as a new zero.

An automatic zero taken at the wrong moment would shift the complete vacuum and
boost range. It would hide a real pressure rather than calibrate the sensor.

Before diagnosing a zero problem, disconnect the measurement hose from the
manifold and leave it open to the same atmosphere as the sensor reference.
Allow the reading to settle. Check the hose for trapped pressure, restriction
or liquid and confirm that the correct sensor range and transfer factor are in
use.

The gauge intentionally displays values smaller than `±0.25 PSI` or
`±0.02 bar` as zero. This is only a visual deadband that prevents the displayed
number from flickering around zero. It is not an automatic zero and it does not
modify the sensor reading.

## What the offset is for

`OFFSET` is a persistent, deliberate display adjustment from `-1.5 PSI` to
`+1.5 PSI` in `0.1 PSI` steps. Its default is `+0.0 PSI`.

For the most faithful physical manifold-pressure measurement, keep the offset
at zero. The sensor already establishes atmospheric zero, so an offset is not
normally required to make the gauge read zero in free air.

Use the offset only when the goal is to align the gauge with the boost value
that the car calculates internally or presents through a compatible ECU/OBD
channel. The car may apply its own barometric estimate, sensor correction,
rounding or display strategy. The offset lets this gauge intentionally simulate
that displayed value when the difference is a small, consistent shift.

Examples:

- The gauge reads `0.0 PSI` at atmosphere and the car intentionally reports
  `-0.3 PSI`: use an offset of `-0.3 PSI` if matching the car is preferred.
- The gauge repeatedly reads `10.2 PSI` while the car's boost value reads
  `10.6 PSI` under the same stable condition: a `+0.4 PSI` offset can align
  them.
- The difference is small at low boost but large at high boost: do not correct
  it with offset. Investigate units, absolute versus gauge pressure, hose
  routing, sensor range and the data source being compared.

Make comparisons only when both values are stable and represent the same type
of pressure. A raw `MAP` value is commonly absolute pressure and includes
atmospheric pressure; an ECU `boost` value is normally relative pressure.
The `±1.5 PSI` offset is not intended to convert absolute MAP into boost.

The offset is applied after smoothing and before conversion to the selected
display unit. It is edited in PSI but produces the equivalent correction when
the gauge is set to BAR. Pressing the offset editor's `RESET` button returns it
to `+0.0 PSI`.

## What smoothing does

The sensor can produce small sample-to-sample variations even when the real
pressure is nearly constant. A fast numerical display and needle can make this
normal noise look like movement. Smoothing reduces that visible jitter by
combining measurements over time.

Smoothing does not:

- change the sensor's atmospheric reference;
- correct a pressure offset or scale error;
- lower the sensor acquisition rate, which remains up to 100 Hz;
- lower the renderer schedule, which remains 60 Hz;
- replace the invalid, stale and implausible-reading checks.

Open the settings menu with a long press on the Civic logo. Select `EN` or
`ES` on the first page if necessary, open `PRESSURE`/`PRESION`, then press the
`SMOOTH`/`SUAV.` button and select `OFF`/`SIN`, `EMA` or `1 EURO`. One Euro is
the factory-default mode. Existing persisted selections remain unchanged by a
firmware update; the active language, mode and all parameters survive restarts.
To adopt the v1.4.0 filter defaults on an upgraded device without changing its
other settings, open the smoothing editor and press `RESET`.

## OFF: maximum immediacy

`OFF` applies no temporal smoothing. Every valid pressure sample becomes the
current pressure value directly. Normal validity checks, the optional offset,
the visual zero deadband and the gauge limits still apply.

Use OFF when:

- comparing the raw sensor behavior with the filtered modes;
- checking whether movement comes from real pressure or from the filter;
- maximum response is more important than visible stability.

OFF may make the last decimal and the needle visibly busier. That is expected
and does not necessarily indicate a sensor problem.

## EMA: simple and predictable

EMA is an exponential moving average:

```text
filtered = previous filtered + alpha * (sample - previous filtered)
```

`ALPHA` controls how much of each new sample is accepted:

| Alpha | Result |
| --- | --- |
| `0.05–0.15` | Very stable, but slow to follow pressure changes |
| `0.20–0.30` | Strong smoothing with moderate lag |
| `0.35` | Default balanced response |
| `0.40–0.60` | Faster response with less smoothing |
| `0.65–0.90` | Very responsive; only light smoothing |
| `1.00` | Mathematically equivalent to passing each sample directly |

The available range is `0.05–1.00` in `0.05` steps. Lower alpha means more
smoothing and more delay. Higher alpha means less smoothing and a faster,
busier display.

EMA is useful when a fixed, easy-to-understand response is preferred. Adjust
one step at a time: lower alpha if the reading flickers, or raise alpha if the
needle feels delayed.

## One Euro: stable at rest and fast during boost changes

The One Euro filter changes its pressure cutoff according to how quickly the
pressure is moving:

```text
pressure cutoff = minimum cutoff + beta * pressure-change speed
```

At nearly constant pressure, the cutoff remains close to the configurable
minimum and suppresses small fluctuations. During a rapid vacuum or boost
change, the cutoff rises automatically so that the gauge can follow the event
with less lag.

### Minimum cutoff

`MIN` controls behavior during stationary and slow pressure changes.

- Lower `MIN`: steadier reading at rest, but more delay during small or slow
  changes.
- Higher `MIN`: follows small changes more readily, but allows more visible
  noise.

The range is `0.5–5.0 Hz` in `0.1 Hz` steps. The default is `1.0 Hz`.
The value is expressed in hertz because it is the cutoff frequency of the
low-pass filter, not the sensor or display update rate.

### Beta

`BETA` controls how strongly the filter relaxes during rapid pressure changes.

- `0.00`: disables the adaptive part, leaving a fixed low-pass filter at the
  selected minimum cutoff.
- Higher beta: quicker response to sudden vacuum or boost changes.
- Excessively high beta: sharp noise and hand-generated pressure pulses can
  pass through more visibly.

The range is `0.00–3.00` per kPa in `0.05` steps. The default is `0.25`.
The derivative-filter cutoff remains fixed at `1 Hz` to keep the adaptation
stable and is intentionally not exposed in the menu.

## Recommended tuning procedure

Start by pressing `RESET`. This restores EMA alpha `0.35`, One Euro minimum
cutoff `1.0 Hz` and beta `0.25` without changing the selected mode. These One
Euro defaults deliberately add more damping than the previous `2.0/1.00`
tuning to reduce visible electronic-wastegate jitter.

For EMA:

1. Begin at alpha `0.35`.
2. Observe the gauge at a stable pressure.
3. Lower alpha in `0.05` steps if it is too busy.
4. Create a representative pressure change.
5. Raise alpha slightly if the response feels delayed.

For One Euro:

1. Begin at `MIN 1.0 Hz` and `BETA 0.25`.
2. At a stable pressure, lower `MIN` until the desired stability is reached.
3. Apply a quick, representative pressure change.
4. Raise `BETA` until the transient response feels immediate enough.
5. If sharp jitter appears during changes, reduce beta slightly.
6. If slow, low-amplitude changes feel delayed, raise the minimum cutoff
   slightly.

Change only one value at a time. A syringe is useful for checking direction,
range and general behavior, but it usually produces slower and less
representative transitions than an engine under load. Final tuning should be
confirmed in the car without interacting with the gauge while driving.

## Quick mode selection

| Goal | Suggested starting point |
| --- | --- |
| Inspect raw sensor behavior | OFF |
| Simple light smoothing | EMA `0.50` |
| Simple balanced smoothing | EMA `0.35` |
| Maximum stability from EMA | EMA `0.15–0.25` |
| Default electronic-wastegate damping | One Euro `MIN 1.0`, `BETA 0.25` |
| Faster adaptive boost response | One Euro `MIN 2.0`, `BETA 1.00` |

These are starting points, not calibration values. The best setting depends on
hose volume, installation, engine behavior and personal preference.

## Persistence and reset behavior

The interface language, selected mode and filter values are stored
persistently:

- interface language: EN or ES;
- smoothing mode: OFF, EMA or One Euro;
- EMA alpha;
- One Euro minimum cutoff;
- One Euro beta.

`RESET` restores the three filter parameters to their defaults but deliberately
keeps the currently selected mode. Switching to OFF does not erase the saved
EMA or One Euro values, so either filtered mode can be selected again without
re-entering its configuration. The guarded `RESET ALL` action on the first
settings page is different: after confirmation it restores every setting,
including One Euro as the selected mode with `MIN 1.0` and `BETA 0.25`.
