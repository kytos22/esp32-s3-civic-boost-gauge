import argparse
import struct
import zlib
from pathlib import Path


MAGIC = 0x31434742
FORMAT_VERSION = 1
HEADER = struct.Struct("<14I")


def validate_cache(data: bytes) -> tuple[int, ...]:
    if len(data) < HEADER.size:
        raise ValueError("Cache is smaller than its header")
    fields = HEADER.unpack_from(data)
    magic, version, total_size, expected_crc = fields[:4]
    if magic != MAGIC or version != FORMAT_VERSION:
        raise ValueError("Unexpected cache magic or format version")
    if total_size != len(data):
        raise ValueError(f"Header size {total_size} does not match {len(data)}")
    actual_crc = zlib.crc32(data[HEADER.size:]) & 0xFFFFFFFF
    if actual_crc != expected_crc:
        raise ValueError(
            f"CRC mismatch: expected {expected_crc:08X}, got {actual_crc:08X}"
        )
    return fields


def format_bytes(data: bytes, per_line: int = 20) -> str:
    lines = []
    for start in range(0, len(data), per_line):
        chunk = data[start:start + per_line]
        lines.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    return "\n".join(lines)


def write_if_changed(path: Path, content: str) -> None:
    encoded = content.encode("ascii")
    if path.exists() and path.read_bytes() == encoded:
        path.touch()
        return
    path.write_bytes(encoded)


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input",
        type=Path,
        default=root / "tools" / "prebaked_gauge_cache.bin",
    )
    parser.add_argument(
        "--header",
        type=Path,
        default=root / "src" / "prebaked_gauge_cache.h",
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=root / "src" / "prebaked_gauge_cache.cpp",
    )
    args = parser.parse_args()

    raw = args.input.read_bytes()
    fields = validate_cache(raw)
    compressed = zlib.compress(raw, 9)

    header = """#pragma once

#include <Arduino.h>

extern const uint8_t PREBAKED_GAUGE_CACHE_ZLIB[];
extern const size_t PREBAKED_GAUGE_CACHE_ZLIB_SIZE;
extern const size_t PREBAKED_GAUGE_CACHE_RAW_SIZE;
"""
    source = f"""#include \"prebaked_gauge_cache.h\"

const uint8_t PREBAKED_GAUGE_CACHE_ZLIB[] PROGMEM = {{
{format_bytes(compressed)}
}};

const size_t PREBAKED_GAUGE_CACHE_ZLIB_SIZE =
    sizeof(PREBAKED_GAUGE_CACHE_ZLIB);
const size_t PREBAKED_GAUGE_CACHE_RAW_SIZE = {len(raw)};
"""
    write_if_changed(args.header, header)
    write_if_changed(args.source, source)
    print(
        "Prebaked cache: "
        f"{len(raw):,} -> {len(compressed):,} bytes "
        f"({len(compressed) / len(raw):.1%}), "
        f"{fields[4]} states"
    )


if __name__ == "__main__":
    main()
