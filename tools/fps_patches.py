"""Read locally supplied ACNH 3.0.3 SpeedFix patches without distributing them."""
from pathlib import Path
import re

BUILD_ID = 'FF1D1C05670DB6021C85B624A710B963'

def read_patch(path: Path) -> dict[int, int]:
    data = path.read_bytes()
    output = {}
    if path.suffix == '.pchtxt':
        lines = data.decode().splitlines()
        assert lines[0] == '@nsobid-' + BUILD_ID, path
        assert '@flag offset_shift 0x100' in lines, path
        enabled = False
        for line in lines:
            line = line.strip()
            if line == '@stop': break
            if line == '@enabled': enabled = True; continue
            if line == '@disabled': enabled = False; continue
            match = re.fullmatch(r'([0-9a-fA-F]{8})\s+([0-9a-fA-F]+)', line)
            if enabled and match:
                offset, payload = int(match[1], 16), bytes.fromhex(match[2])
                output.update((offset+i, byte) for i, byte in enumerate(payload))
    else:
        assert path.stem == BUILD_ID and data[:5] == b'IPS32', path
        cursor = 5
        while data[cursor:cursor+4] != b'EEOF':
            assert cursor + 6 <= len(data), path
            offset = int.from_bytes(data[cursor:cursor+4], 'big') - 0x100
            size = int.from_bytes(data[cursor+4:cursor+6], 'big')
            cursor += 6
            if size:
                payload = data[cursor:cursor+size]; cursor += size
            else:
                size = int.from_bytes(data[cursor:cursor+2], 'big')
                payload = data[cursor+2:cursor+3] * size; cursor += 3
            assert len(payload) == size and offset >= 0, path
            output.update((offset+i, byte) for i, byte in enumerate(payload))
    assert output, path
    return output

def variants(external: Path) -> list[Path]:
    return sorted(path for path in external.rglob('*') if path.is_file() and
                  (path.name.endswith('main.pchtxt') or path.name == BUILD_ID + '.ips'))
