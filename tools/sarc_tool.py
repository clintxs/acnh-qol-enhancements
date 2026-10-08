#!/usr/bin/env python3
"""Minimal SARC archive list/extract/repack helper for the Grab 5 mod."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def align(value: int, amount: int) -> int:
    return (value + amount - 1) & -amount


def read_sarc(path: Path) -> tuple[str, list[tuple[str, bytes]]]:
    blob = path.read_bytes()
    if blob[:4] != b"SARC":
        raise ValueError(f"{path} is not a SARC archive")

    bom = blob[6:8]
    endian = "<" if bom == b"\xff\xfe" else ">" if bom == b"\xfe\xff" else None
    if endian is None:
        raise ValueError(f"unsupported SARC byte-order marker {bom.hex()}")

    header_size, _, file_size, data_offset, _ = struct.unpack_from(endian + "HHIII", blob, 4)
    if file_size != len(blob):
        raise ValueError(f"file-size mismatch: header={file_size}, actual={len(blob)}")
    if blob[header_size:header_size + 4] != b"SFAT":
        raise ValueError("missing SFAT section")

    sfat_size, node_count, _ = struct.unpack_from(endian + "HHI", blob, header_size + 4)
    nodes_offset = header_size + sfat_size
    sfnt_offset = nodes_offset + node_count * 16
    if blob[sfnt_offset:sfnt_offset + 4] != b"SFNT":
        raise ValueError("missing SFNT section")
    sfnt_size = struct.unpack_from(endian + "H", blob, sfnt_offset + 4)[0]
    names_offset = sfnt_offset + sfnt_size

    entries: list[tuple[str, bytes]] = []
    for index in range(node_count):
        node = nodes_offset + index * 16
        _, name_field, start, end = struct.unpack_from(endian + "IIII", blob, node)
        if not name_field >> 24:
            name = f"{index:04d}.bin"
        else:
            name_start = names_offset + (name_field & 0x00FFFFFF) * 4
            name_end = blob.index(0, name_start)
            name = blob[name_start:name_end].decode("utf-8")
        entries.append((name, blob[data_offset + start:data_offset + end]))
    return endian, entries


def sarc_hash(name: str, multiplier: int = 0x65) -> int:
    result = 0
    for value in name.encode("utf-8"):
        result = (result * multiplier + value) & 0xFFFFFFFF
    return result


def write_sarc(path: Path, endian: str, entries: list[tuple[str, bytes]]) -> None:
    entries = sorted(entries, key=lambda entry: sarc_hash(entry[0]))
    names = bytearray()
    name_offsets: dict[str, int] = {}
    for name, _ in entries:
        name_offsets[name] = len(names) // 4
        names.extend(name.encode("utf-8") + b"\0")
        names.extend(b"\0" * (align(len(names), 4) - len(names)))

    header_end = 0x14 + 0x0C + len(entries) * 16 + 8 + len(names)
    data_offset = align(header_end, 0x100)
    data = bytearray()
    ranges: dict[str, tuple[int, int]] = {}
    for name, contents in entries:
        data.extend(b"\0" * (align(len(data), 0x100) - len(data)))
        start = len(data)
        data.extend(contents)
        ranges[name] = (start, len(data))

    bom = 0xFEFF
    file_size = data_offset + len(data)
    output = bytearray(struct.pack(endian + "4sHHIIHH", b"SARC", 0x14, bom,
                                   file_size, data_offset, 0x100, 0))
    output.extend(struct.pack(endian + "4sHHI", b"SFAT", 0x0C, len(entries), 0x65))
    for name, _ in entries:
        start, end = ranges[name]
        name_field = 0x01000000 | name_offsets[name]
        output.extend(struct.pack(endian + "IIII", sarc_hash(name), name_field, start, end))
    output.extend(struct.pack(endian + "4sHH", b"SFNT", 8, 0))
    output.extend(names)
    output.extend(b"\0" * (data_offset - len(output)))
    output.extend(data)
    if len(output) != file_size:
        raise AssertionError(f"SARC size mismatch: {len(output)} != {file_size}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("--extract", type=Path)
    parser.add_argument("--replace", action="append", default=[], metavar="NAME=PATH")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    endian, entries = read_sarc(args.archive)
    if args.replace:
        if not args.output:
            parser.error("--replace requires --output")
        replacements = {}
        for value in args.replace:
            name, separator, replacement = value.partition("=")
            if not separator:
                parser.error(f"invalid replacement {value!r}; expected NAME=PATH")
            replacements[name] = Path(replacement).read_bytes()
        known = {name for name, _ in entries}
        missing = replacements.keys() - known
        if missing:
            raise ValueError(f"archive entries not found: {', '.join(sorted(missing))}")
        write_sarc(args.output, endian,
                   [(name, replacements.get(name, data)) for name, data in entries])
    elif args.extract:
        args.extract.mkdir(parents=True, exist_ok=True)
        for name, data in entries:
            output = args.extract / name
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(data)
    else:
        for name, data in entries:
            print(f"{len(data):10d}  {name}")


if __name__ == "__main__":
    main()
