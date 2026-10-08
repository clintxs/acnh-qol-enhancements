"""Local SARC/BFLYT utilities adapted from Catalog Aware Shops (MIT)."""
import struct
from dataclasses import dataclass
PANE_NAME=12
PANE_NAME_BYTES=24
HEADER_BYTES=20
def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]

def pane_name(section):
    return section[PANE_NAME:PANE_NAME + PANE_NAME_BYTES].split(b'\0', 1)[0].decode('ascii')

def layout_sections(data):
    if data[:8] != b'FLYT\xff\xfe\x14\0' or u32(data, 12) != len(data):
        raise ValueError('Unexpected layout header or file length')
    sections = []
    position = HEADER_BYTES
    while position < len(data):
        size = u32(data, position + 4)
        if size < 8 or size % 4 or position + size > len(data):
            raise ValueError('Malformed layout section')
        sections.append(data[position:position + size])
        position += size
    if len(sections) != struct.unpack_from('<H', data, 16)[0]:
        raise ValueError('Layout section count does not match header')
    return sections

@dataclass
class ArchiveEntry:
    name: str
    node_offset: int
    start: int
    end: int
    content: bytes

def archive_entries(data):
    """Read this little-endian SARC variant without changing its name/hash table."""
    if data[:8] != b'SARC\x14\0\xff\xfe' or u32(data, 8) != len(data):
        raise ValueError('Unexpected archive header or length')
    if data[20:24] != b'SFAT':
        raise ValueError('Missing SFAT')
    payload_start = u32(data, 12)
    count = struct.unpack_from('<H', data, 26)[0]
    names_start = 40 + count * 16
    entries = []
    for index in range(count):
        node = 32 + index * 16
        _, name_field, start, end = struct.unpack_from('<4I', data, node)
        name_start = names_start + (name_field & 0xffffff) * 4
        name = data[name_start:data.index(0, name_start)].decode('utf-8')
        if name_field >> 24 != 1 or not 0 <= start <= end <= len(data) - payload_start:
            raise ValueError('Unexpected archive node')
        entries.append(ArchiveEntry(name, node, start, end, data[payload_start + start:payload_start + end]))
    return payload_start, entries

def resources(data, prefix=''):
    result = {}
    for entry in archive_entries(data)[1]:
        name = prefix + entry.name
        if entry.content.startswith(b'SARC'):
            result.update(resources(entry.content, name + '/'))
        else:
            result[name] = entry.content
    return result

