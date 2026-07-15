import argparse
import struct
import sys
import time
import zlib
from pathlib import Path

import serial


MAGIC = 0x31434742
FORMAT_VERSION = 1
HEADER = struct.Struct("<14I")
START = b"BGCACHE "


def read_until_marker(port: serial.Serial, timeout: float) -> int:
    deadline = time.monotonic() + timeout
    pending = bytearray()
    while time.monotonic() < deadline:
        chunk = port.read(max(1, port.in_waiting))
        if not chunk:
            continue
        pending.extend(chunk)
        marker = pending.find(START)
        if marker < 0:
            if len(pending) > len(START) * 2:
                del pending[:-len(START)]
            continue
        line_end = pending.find(b"\n", marker)
        if line_end < 0:
            continue
        return int(pending[marker + len(START):line_end].strip())
    raise TimeoutError("Cache marker not received; reset the board and retry")


def read_exact(port: serial.Serial, size: int, timeout: float) -> bytes:
    deadline = time.monotonic() + timeout
    data = bytearray()
    while len(data) < size and time.monotonic() < deadline:
        chunk = port.read(min(65536, size - len(data)))
        if chunk:
            data.extend(chunk)
            if len(data) % (512 * 1024) < len(chunk):
                print(f"Captured {len(data):,}/{size:,} bytes", flush=True)
    if len(data) != size:
        raise TimeoutError(f"Cache capture stopped at {len(data):,}/{size:,} bytes")
    return bytes(data)


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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM6")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).with_name("prebaked_gauge_cache.bin"),
    )
    args = parser.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.2) as port:
        port.reset_input_buffer()
        port.dtr = False
        port.rts = True
        time.sleep(0.1)
        port.rts = False
        port.dtr = True
        total_size = read_until_marker(port, args.timeout)
        data = read_exact(port, total_size, max(args.timeout, total_size / 50000))

    fields = validate_cache(data)
    args.output.write_bytes(data)
    print(
        "Saved "
        f"{args.output} ({len(data):,} bytes, "
        f"{fields[4]} states, {fields[7]:,} arc commands, "
        f"{fields[9]:,} cursor pixels)"
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"capture_prebaked_cache: {error}", file=sys.stderr)
        raise SystemExit(1)

