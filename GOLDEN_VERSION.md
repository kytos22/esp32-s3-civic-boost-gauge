# Golden Version: Smooth Tile Renderer

Date: 2026-07-15
Status: Verified by hardware observation

- Smooth forward and reverse SHOW animation.
- No visible cursor or arc artifacts.
- 60 Hz schedule using a 16,667 us frame period.
- Non-overlapping 16x16 display tiles.
- Prebaked 541-state arc and cursor cache.
- Build-time zlib asset loaded and CRC-validated into PSRAM at startup.
- Maximum measured diagnostic frame: 11.64 ms.
- Diagnostic telemetry is disabled for normal use.

Do not change the rendering or display-transfer path without making a new
backup first.
