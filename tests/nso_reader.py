"""Read hash-checked NSO sections from the user's local game executable dump."""
import hashlib
import struct


def lz4_block(data,expected):
    output = bytearray()
    offset = 0
    def length(value):
        nonlocal offset
        if value == 15:
            while True:
                extra = data[offset]; offset += 1
                value += extra
                if extra != 255: break
        return value
    while offset < len(data):
        token = data[offset]; offset += 1
        literals = length(token >> 4)
        output.extend(data[offset:offset+literals]); offset += literals
        if offset == len(data): break
        distance = int.from_bytes(data[offset:offset+2],'little'); offset += 2
        count = length(token & 15) + 4
        assert 0 < distance <= len(output) and len(output)+count <= expected
        for _ in range(count): output.append(output[-distance])
    assert len(output) == expected
    return bytes(output)


def image(path,build_id):
    data = path.read_bytes()
    assert data[:4] == b'NSO0' and data[0x40:0x60].hex() == build_id
    flags = struct.unpack_from('<I',data,12)[0]
    segments = []
    for index in range(3):
        file,memory,size = struct.unpack_from('<3I',data,16+16*index)
        stored = struct.unpack_from('<I',data,0x60+4*index)[0]
        content = data[file:file+stored] if flags & (1<<index) else data[file:file+size]
        if flags & (1<<index): content = lz4_block(content,size)
        assert len(content) == size
        assert hashlib.sha256(content).digest() == data[0xa0+32*index:0xc0+32*index]
        segments.append((memory,content))
    size = max(address+len(content) for address,content in segments)
    output = bytearray(size)
    for address,content in segments: output[address:address+len(content)] = content
    return bytes(output)
