from pathlib import Path
import binascii
import re
import struct
import zlib


ROOT = Path(__file__).resolve().parents[1]
CAPTURE = ROOT / "tools" / "prebaked_capture.bin"
WIDTH = 466
HEIGHT = 466


def parse_frame(data: bytes, name: str) -> list[tuple[int, int]]:
    match = re.search(rb"PBK1 " + name.encode() + rb" (\d+)\r?\n", data)
    if not match:
        raise RuntimeError(f"Missing {name} frame header")

    count = int(match.group(1))
    start = match.end()
    end = start + count * 6
    if end > len(data):
        raise RuntimeError(f"Incomplete {name} frame payload")

    tail = re.match(rb"\r?\nENDPBK1 " + name.encode() + rb"\r?\n", data[end:])
    if not tail:
        raise RuntimeError(f"Missing {name} frame footer")

    pixels = [struct.unpack_from("<IH", data, start + i * 6) for i in range(count)]
    if any(offset >= WIDTH * HEIGHT for offset, _ in pixels):
        raise RuntimeError(f"Out-of-range pixel in {name}")
    if any(pixels[i][0] >= pixels[i + 1][0] for i in range(len(pixels) - 1)):
        raise RuntimeError(f"Unsorted or duplicate pixels in {name}")
    return pixels


def create_runs(pixels: list[tuple[int, int]]):
    offsets: list[int] = []
    lengths: list[int] = []
    colors: list[int] = []
    run_offset = None
    previous_offset = None

    for offset, color in pixels:
        if run_offset is None or offset != previous_offset + 1:
            if run_offset is not None:
                lengths.append(previous_offset - run_offset + 1)
            offsets.append(offset)
            run_offset = offset
        colors.append(color)
        previous_offset = offset

    if run_offset is not None:
        lengths.append(previous_offset - run_offset + 1)
    return offsets, lengths, colors


def format_array(values, values_per_line=12, hex_width=None):
    lines = []
    for start in range(0, len(values), values_per_line):
        chunk = values[start:start + values_per_line]
        if hex_width:
            text = ", ".join(f"0x{value:0{hex_width}X}" for value in chunk)
        else:
            text = ", ".join(str(value) for value in chunk)
        lines.append("    " + text + ",")
    return "\n".join(lines)


def write_asset_source(frames):
    header = ROOT / "src" / "prebaked_visuals.h"
    source = ROOT / "src" / "prebaked_visuals.cpp"

    header.write_text(
        """#pragma once

#include <Arduino.h>

struct PrebakedVisual {
    const uint32_t *run_offsets;
    const uint16_t *run_lengths;
    const uint16_t *colors;
    uint32_t run_count;
    uint32_t pixel_count;
};

extern const PrebakedVisual PREBAKED_GAUGE_VISUAL;
extern const PrebakedVisual PREBAKED_STARTUP_VISUAL;
""",
        encoding="ascii",
    )

    sections = ['#include "prebaked_visuals.h"', ""]
    descriptors = []
    for name, pixels in frames.items():
        offsets, lengths, colors = create_runs(pixels)
        symbol = name.upper()
        sections.extend([
            f"static const uint32_t {name}_run_offsets[] PROGMEM = {{",
            format_array(offsets, 10),
            "};",
            f"static const uint16_t {name}_run_lengths[] PROGMEM = {{",
            format_array(lengths, 16),
            "};",
            f"static const uint16_t {name}_colors[] PROGMEM = {{",
            format_array(colors, 12, 4),
            "};",
            "",
        ])
        descriptors.append(
            f"const PrebakedVisual PREBAKED_{symbol}_VISUAL = {{\n"
            f"    {name}_run_offsets, {name}_run_lengths, {name}_colors,\n"
            f"    {len(offsets)}, {len(colors)}\n"
            "};"
        )
        print(f"{name}: {len(colors)} pixels, {len(offsets)} runs")

    sections.extend(descriptors)
    sections.append("")
    source.write_text("\n".join(sections), encoding="ascii")


def write_preview(name: str, pixels: list[tuple[int, int]]):
    rgb = bytearray(WIDTH * HEIGHT * 3)
    for offset, swapped in pixels:
        value = ((swapped & 0xFF) << 8) | (swapped >> 8)
        red = ((value >> 11) & 0x1F) * 255 // 31
        green = ((value >> 5) & 0x3F) * 255 // 63
        blue = (value & 0x1F) * 255 // 31
        pixel = offset * 3
        rgb[pixel:pixel + 3] = bytes((red, green, blue))

    scanlines = bytearray()
    row_bytes = WIDTH * 3
    for y in range(HEIGHT):
        scanlines.append(0)
        scanlines.extend(rgb[y * row_bytes:(y + 1) * row_bytes])

    def chunk(kind: bytes, payload: bytes) -> bytes:
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", binascii.crc32(body) & 0xFFFFFFFF)

    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)))
    png.extend(chunk(b"IDAT", zlib.compress(bytes(scanlines), 9)))
    png.extend(chunk(b"IEND", b""))
    (ROOT / "tools" / f"prebaked_{name}.png").write_bytes(png)


def main():
    data = CAPTURE.read_bytes()
    frames = {
        "gauge": parse_frame(data, "gauge"),
        "startup": parse_frame(data, "startup"),
    }
    write_asset_source(frames)
    for name, pixels in frames.items():
        write_preview(name, pixels)


if __name__ == "__main__":
    main()
