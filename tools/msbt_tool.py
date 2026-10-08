#!/usr/bin/env python3
"""List labels and text from Nintendo MSBT files used by ACNH."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def sections(blob: bytes, endian: str):
    offset = 0x20
    while offset + 16 <= len(blob):
        magic = blob[offset:offset + 4]
        size = struct.unpack_from(endian + "I", blob, offset + 4)[0]
        yield magic, blob[offset + 16:offset + 16 + size]
        offset = (offset + 16 + size + 15) & ~15


def read_msbt(path: Path) -> list[tuple[str, str]]:
    blob = path.read_bytes()
    if blob[:8] != b"MsgStdBn":
        raise ValueError(f"{path} is not an MSBT file")
    bom = blob[8:10]
    endian = "<" if bom == b"\xff\xfe" else ">" if bom == b"\xfe\xff" else None
    if endian is None:
        raise ValueError(f"unsupported MSBT byte-order marker {bom.hex()}")

    labels: dict[int, str] = {}
    texts: list[str] = []
    encoding = "utf-16-le" if endian == "<" else "utf-16-be"
    for magic, payload in sections(blob, endian):
        if magic == b"LBL1":
            group_count = struct.unpack_from(endian + "I", payload, 0)[0]
            for group in range(group_count):
                count, offset = struct.unpack_from(endian + "II", payload, 4 + group * 8)
                cursor = offset
                for _ in range(count):
                    length = payload[cursor]
                    cursor += 1
                    label = payload[cursor:cursor + length].decode("utf-8")
                    cursor += length
                    index = struct.unpack_from(endian + "I", payload, cursor)[0]
                    cursor += 4
                    labels[index] = label
        elif magic == b"TXT2":
            count = struct.unpack_from(endian + "I", payload, 0)[0]
            offsets = struct.unpack_from(endian + f"{count}I", payload, 4)
            for index, start in enumerate(offsets):
                end = offsets[index + 1] if index + 1 < count else len(payload)
                data = payload[start:end]
                if data.endswith(b"\0\0"):
                    data = data[:-2]
                texts.append(data.decode(encoding, errors="backslashreplace"))

    return [(labels.get(index, f"#{index}"), text) for index, text in enumerate(texts)]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path)
    parser.add_argument("--find", default="")
    args = parser.parse_args()

    paths = [args.path] if args.path.is_file() else sorted(args.path.rglob("*.msbt"))
    needle = args.find.casefold()
    for path in paths:
        for label, value in read_msbt(path):
            if not needle or needle in label.casefold() or needle in value.casefold():
                print(f"{path}\t{label}\t{value!r}")


if __name__ == "__main__":
    main()
