# AGENTS.md

This file is the operating guide for coding agents working in this repository.
It applies to the entire project. Match the user's language in conversation,
but keep repository documentation and release notes in English unless asked
otherwise.

## Project State

This is a hardware-tested turbo boost gauge for the Waveshare
ESP32-S3-Touch-AMOLED-1.43 (466x466 SH8601 AMOLED with FT3168 touch). The board
is mounted with the USB connector facing down. Release `v1.2.0` is the current
public baseline. The production-hardening target is `v1.2.1`.

The visual renderer is considered stable and artifact-free. Read these before
changing rendering, display transfer, touch transforms, or assets:

1. `README.md`
2. `GOLDEN_VERSION.md`
3. `src/main.cpp`
4. `lib/Mylibrary/pin_config.h`

The XGZP6847D pressure-sensor acquisition is integrated and bench-tested.
Preserve both the working renderer and the validated sensor/touch bus behavior.

## Repository Map

- `src/main.cpp`: firmware, startup sequence, touch UI, pressure units, sensor
  acquisition, prebaked renderer and display flush path.
- `platformio.ini`: ESP32-S3 build, 240 MHz CPU, 80 MHz flash/QSPI settings,
  OPI PSRAM and core assignment.
- `lib/Mylibrary/pin_config.h`: authoritative board pin mapping.
- `src/prebaked_visuals.*`: sparse static gauge and startup frames.
- `src/prebaked_gauge_cache.*`: generated compressed firmware representation
  of the dynamic arc/cursor cache.
- `src/prebaked_cache_format.h`: cache ABI shared by firmware and tools.
- `tools/prebaked_gauge_cache.bin`: canonical raw dynamic cache.
- `tools/prebaked_capture.bin`: canonical captured static frames.
- `tools/platformio_prebuild.py`: refreshes compressed cache source when the
  canonical cache or generator changes.
- `tools/create_demo_gif.js`: generates faithful PSI and BAR release GIFs from
  the real static assets, value font and dynamic cache.
- `firmware/<version>/`: release binaries, previews, GIFs and checksums.
- `lib/`: vendored libraries used by the verified build. Treat them as pinned.

## Hardware Configuration

- Display: SH8601, 466x466, QSPI.
- Touch: FT3168 over I2C.
- I2C pins: SDA GPIO 47, SCL GPIO 48.
- Pressure sensor: XGZP6847D300KPGPN I2C, bidirectional `-100 to +300 kPa`,
  address `0x6D`, powered from 3.3 V, V2.x transfer factor `K=16`.
- CPU: 240 MHz.
- Arduino `setup()`/`loop()` and Arduino events run on core 1. No project task
  currently uses core 0 explicitly.

Never expose an ESP32-S3 GPIO to more than 3.3 V. Keep the shared I2C pull-ups
and the pressure sensor supply at 3.3 V.

## Rendering Architecture

The gauge is not rendered conventionally by LVGL every frame.

1. The startup logo is displayed immediately.
2. Two full RGB565 framebuffers are allocated in PSRAM.
3. During the five-second startup screen, the compressed 4.90 MB cache is
   validated and decompressed directly into PSRAM.
4. The static frame supplies ticks, labels, unit and Civic logo.
5. A 541-state cache supplies the moving arc and red cursor at 0.5-degree
   increments.
6. Only dirty, non-overlapping 16x16 display tiles are transferred.
7. Central value glyphs are rasterized from preprocessed opacity masks with a
   fixed decimal anchor.

The gauge target is scheduled every 16,667 microseconds (60 Hz). Central text
updates every 33 ms. PSI and BAR share the same dynamic cache. BAR changes only
safe static labels and maps the sensor directly to `-1..2 BAR`; PSI maps to
`-15..30 PSI`.

LVGL owns the touch menu and input dispatch. The visible gauge, central value,
arc and cursor use the custom framebuffer renderer.

## Renderer Invariants

These rules protect the hardware-verified result:

- Do not rotate the full framebuffer in the flush callback or display driver.
  Geometry is already generated in native framebuffer orientation for USB-down
  mounting.
- Do not change gauge center, angles, radii, arc width, cursor geometry, outer
  ticks, color gradient, cache step, tile size or RGB565 blending without
  regenerating and revalidating the dynamic cache.
- Cursor pixels include their expected static background. Changing static
  pixels near the cursor path can reintroduce the historical lines and
  artifacts around the scale.
- Do not replace the cached cursor or arc with per-frame LVGL geometry.
- Do not allocate memory, decode assets, calculate trigonometry, or generate
  geometry in the normal frame loop.
- Keep the decimal point anchored. Do not return to centering the whole value
  string, which causes visible lateral movement.
- Keep PSI as the default unit. Store the selection in Preferences namespace
  `boost-gauge`, key `unit`, and write only when it changes.
- Keep 75% as the default brightness. Store menu changes in Preferences key
  `brightness`; save slider changes only on release and avoid redundant writes.
- Keep SHOW/demo mode disabled by default.
- Production builds must leave `ENABLE_PERF_TELEMETRY`,
  `ENABLE_RENDER_DIAGNOSTICS`, `ENABLE_HARDWARE_TELEMETRY`,
  `DUMP_PREBAKED_FRAMES` and `DUMP_BAKED_CACHE` set to `0`.
- Preserve the five-second boot screen and smooth up/down startup sweep. The
  transition to LIVE must not flash briefly to maximum pressure.

If a requested visual change can stay strictly inside a static area that never
intersects the arc/cursor path, prefer rebuilding only the static frame. If in
doubt, regenerate the cache and test both directions of the full sweep.

## Touch Invariants

- Raw FT3168 coordinates are already correct for the native display setup.
- Follow Waveshare's polling layout: write normal mode to register `0x00`,
  poll finger count at `0x02`, then read the four X/Y bytes as one block from
  `0x03`. Keep power mode `0xA5` active and automatic monitor register `0x86`
  disabled, and refresh both every second on the 300 kHz shared bus; pure demo
  initialization eventually NACKed on the shared live bus. The board demo
  declares no touch interrupt; do not assign GPIO 16 as `TP_INT`.
- The menu is visually rotated by `900` LVGL tenths of a degree.
- The brightness slider projects the touch point onto its transformed visual
  axis. Do not replace this with an untransformed X/Y lookup.
- Keep controls finger-sized and avoid overlapping extended click areas.
- Test Civic click, Civic long press, close, PSI/BAR buttons, slider, `+`/`-`
  buttons and the persistent `TEMP: OFF/ON` control after touch changes.

## Sensor Work

The XGZP6847D integration starts combined conversions through register `0x30`,
reads signed 24-bit pressure and signed 16-bit temperature, and applies the
documented `K=16` conversion for the `+300 kPa` positive range. Sampling runs
at up to 100 Hz through a non-blocking state machine while the display remains
at 60 Hz. Pressure and temperature are read as one contiguous five-byte block.

- Keep pressure filtering in canonical kPa and convert only at the sensor/UI
  boundary.
- Use the factory-calibrated pressure value directly. Do not add startup or
  menu zero calibration; preserve the opt-in, persistent five-second
  sensor-temperature display and LIVE/SHOW switching.
- Keep missing, stale, invalid and implausible readings non-blocking.
- Keep the delayed `ERR` warning for sensor readings unavailable for more than
  one second and the `MAX` warning above the 30 PSI display range.
- Keep FT3168 polling grouped into Waveshare's two I2C transactions so touch
  and pressure acquisition can reliably share the bus.
- Bench tests verified zero-pressure stability, temperature acquisition,
  positive and negative pressure response, and shared-bus touch stability
  without XGZP or FT3168 errors. Full automotive manifold validation remains
  recommended.

## Build And Flash

Standard commands:

```powershell
pio run
pio run --target upload --upload-port COM6
```

If `pio` is not on PATH on Windows:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --target upload --upload-port COM6
```

Expected release-build baseline near `v1.2.0`:

- Internal RAM: about 65.3 KB.
- Flash: about 1.180 MB of the 6.55 MB application partition.
- Normal warnings from the vendored Arduino_GFX library about
  `SPI_MAX_PIXELS_AT_ONCE` are pre-existing; investigate new warnings.

## Validation Checklist

For every firmware change:

1. Run `git diff --check`.
2. Run a release `pio run` and record RAM/flash changes.
3. Confirm production diagnostic flags are off.
4. If hardware behavior is affected, flash COM6 and test on the real AMOLED.

For renderer, touch or unit changes, also verify:

- Boot logo remains visible while assets load.
- Startup sweep reaches maximum, returns smoothly to minimum, then enters LIVE.
- SHOW sweep is smooth in both directions across the complete range.
- No artifacts appear around the central value, Civic logo, ticks or cursor.
- The red cursor remains intact around the full arc.
- PSI and BAR values, labels, unit typography and fixed decimal are correct.
- Unit selection survives a reboot.
- Brightness touch position matches the visual slider 1:1.

Use temporary telemetry only for measurement, then return its flag to `0` and
rebuild the production firmware.

## Prebaked Cache Workflow

`tools/prebaked_gauge_cache.bin` is the source of truth for dynamic rendering.
Normal builds only compress/embed it; they do not regenerate geometry.

After any cache-affecting geometry or color change:

1. Back up or commit the current working renderer.
2. Set `DUMP_BAKED_CACHE` to `1` in `src/main.cpp`.
3. Build and upload the exporter firmware.
4. Run `python tools/capture_prebaked_cache.py --port COM6`.
5. Set `DUMP_BAKED_CACHE` back to `0`.
6. Build normally. The PlatformIO prebuild script regenerates
   `src/prebaked_gauge_cache.*` from the canonical binary.
7. Test the complete forward and reverse sweep on hardware before accepting the
   new cache.

Do not hand-edit generated byte arrays in `src/prebaked_gauge_cache.cpp` or
`src/prebaked_visuals.cpp`.

## GIF And Release Workflow

ImageMagick is required for GIF generation. Generate both unit demos with:

```powershell
node tools/create_demo_gif.js 1.1.0
```

Replace the version argument for a new release. The generator must continue to
use the canonical capture, cache and fonts rather than a visually approximate
reimplementation. A normal output contains 601 frames with a 466x466 canvas.

Before publishing a release:

- Build production firmware from the intended commit.
- Package both application-only (`0x10000`) and complete (`0x0`) images.
- Include both GIFs, boot PNG, release README and `SHA256SUMS.txt`.
- Verify the application image with `esptool.py image_info`.
- Verify the packaged application hash matches `.pio/build/src/firmware.bin`.
- Keep release notes in English and describe sensor validation accurately;
  never call integration complete before it has been hardware-tested.

## Change Discipline

- Keep edits scoped. Do not refactor the verified renderer while adding sensor
  support or documentation.
- Do not update vendored libraries or the Espressif platform merely because a
  newer version exists. Benchmark and hardware-test such changes separately.
- Never overwrite the latest known-good release assets. Create a new versioned
  directory and tag.
- Preserve user changes in a dirty worktree and avoid destructive Git commands.
- Commit meaningful milestones before risky renderer/cache experiments.
