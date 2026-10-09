"""Extend the hash-checked currency BNTX with two RGBA8 UI pictures.

Restricted to this asset's little-endian, single-mip, 2D textures. Rebuild the
pointer tables/dictionary/relocations, retaining original image payloads and
texture settings. Binary fields cross-checked with the local original and
https://github.com/Tyulis/3DSkit/blob/master/doc/BNTX.md . No external source
implementation is embedded; Tegra GOB addressing follows its byte/row bit order.
"""
from dataclasses import dataclass
import struct
from shopping_card_texture import NAMES, WIDTH, HEIGHT, pixels


def integer(data, offset, fmt='I'):
    return struct.unpack_from('<' + fmt, data, offset)[0]


def string(data, offset):
    size = integer(data, offset, 'H')
    assert data[offset + 2 + size] == 0
    return data[offset + 2:offset + 2 + size].decode('ascii')


@dataclass
class Texture:
    name: str
    info: bytes
    data: bytes


def textures(data):
    assert data[:8] == b'BNTX\0\0\0\0' and data[12:14] == b'\xff\xfe'
    assert integer(data, 28) == len(data) and data[32:36] == b'NX  '
    count, table = integer(data, 36), integer(data, 40, 'Q')
    result = []
    for i in range(count):
        offset = integer(data, table + i * 8, 'Q')
        assert data[offset:offset + 4] == b'BRTI'
        assert integer(data, offset + 22, 'H') == 1  # One mip level.
        assert integer(data, offset + 44) == integer(data, offset + 48) == 1
        assert all(integer(data, offset + field, 'Q') == 0 for field in (0x78,0x90,0x98))
        name = string(data, integer(data, offset + 0x60, 'Q'))
        image = integer(data, integer(data, offset + 0x70, 'Q'), 'Q')
        size = integer(data, offset + 0x50)
        result.append(Texture(name, data[offset:offset + 0x2a0], data[image:image + size]))
    return result


def bit(key, reference):
    # nn::util::ResDic indexes low bits first, starting at the END of the key.
    index = reference // 8
    return ((key[-index - 1] if index < len(key) else 0) >> (reference % 8)) & 1


def dictionary(names):
    keys = [b''] + [name.encode('ascii') for name in names]
    nodes = [[-1,0,0]]
    for index, key in enumerate(keys[1:], 1):
        parent, leaf = 0, nodes[0][1]
        while nodes[parent][0] < nodes[leaf][0]:
            parent, leaf = leaf, nodes[leaf][1 + bit(key, nodes[leaf][0])]
        other = keys[leaf]
        reference = next(r for r in range(max(len(key),len(other))*8) if bit(key,r) != bit(other,r))
        parent, child = 0, nodes[0][1]
        while nodes[parent][0] < nodes[child][0] < reference:
            parent, child = child, nodes[child][1 + bit(key, nodes[child][0])]
        branches = [child, child]
        branches[bit(key,reference)] = index
        nodes.append([reference, *branches])
        side = 0 if parent == 0 else bit(key,nodes[parent][0])
        nodes[parent][1 + side] = index
    return nodes


def rgba_address(x, y, width):
    """Byte address in a block-linear RGBA8 surface with one-GOB block height."""
    byte_x = x * 4
    gob = (y // 8) * (width * 4 // 64) + byte_x // 64
    # A GOB holds 64 byte-columns by 8 rows, permuted for the GPU cache.
    return gob * 512 + ((byte_x >> 5) & 1) * 256 + ((y >> 1) & 3) * 64 + ((byte_x >> 4) & 1) * 32 + (y & 1) * 16 + (byte_x & 15)


def swizzle_rgba(raw, width, height):
    assert width % 16 == height % 8 == 0 and len(raw) == width * height * 4
    output = bytearray(len(raw))
    for y in range(height):
        for x in range(width):
            source = (y * width + x) * 4
            destination = rgba_address(x,y,width)
            output[destination:destination+4] = raw[source:source+4]
    return bytes(output)


def append_card(original, palette_name='green'):
    source = textures(original)
    assert len(source) == 7 and not set(NAMES).intersection(t.name for t in source)
    # Start from the native icon descriptor, retaining opaque GPU object storage.
    # Fields describe uncompressed RGBA8, one-GOB block height, one 2D image.
    for name, backing in zip(NAMES, (True, False)):
        info = bytearray(source[1].info)
        struct.pack_into('<I', info, 0x1c, 0x0b01)  # RGBA8_UNORM, same linear color domain as pane colors.
        struct.pack_into('<2I', info, 0x24, WIDTH, HEIGHT)
        struct.pack_into('<I', info, 0x34, 0)  # Block-height exponent zero.
        struct.pack_into('<I', info, 0x50, WIDTH * HEIGHT * 4)
        info[0x58:0x5c] = bytes((2,3,4,5))  # RGBA channel sources.
        source.append(Texture(name, bytes(info), swizzle_rgba(pixels(backing, palette_name),WIDTH,HEIGHT)))
    names = [texture.name for texture in source]
    filename = string(original, integer(original, 16) - 2)
    output = bytearray(original[:0x198])
    table = len(output)
    output.extend(bytes(8 * len(source)))
    output.extend(bytes(-len(output) % 16))
    strings = len(output)
    output.extend(bytes(16))
    output.extend(struct.pack('<I', len(source) + 1))
    empty_string = len(output)
    output.extend(bytes(4))
    string_positions = {}
    for name in [filename, *names]:
        string_positions[name] = len(output)
        encoded = name.encode('ascii')
        output.extend(struct.pack('<H', len(encoded)) + encoded + b'\0')
        output.extend(bytes(-len(output) % 2))
    output.extend(bytes(-len(output) % 8))
    dic = len(output)
    output.extend(struct.pack('<4sI', b'_DIC', len(source)))
    metadata_pointers = [0x28,0x38,0x40]
    for i, (reference,left,right) in enumerate(dictionary(names)):
        metadata_pointers.append(len(output) + 8)
        key = string_positions[names[i-1]] if i else empty_string
        output.extend(struct.pack('<IHHQ', reference & 0xffffffff,left,right,key))
    struct.pack_into('<4sII', output, strings, b'_STR', len(output)-strings, len(output)-strings)
    infos = []
    image_pointers = [0x30]
    for i, texture in enumerate(source):
        offset = len(output)
        infos.append(offset)
        struct.pack_into('<Q', output, table + 8*i, offset)
        metadata_pointers.append(table + 8*i)
        info = bytearray(texture.info)
        struct.pack_into('<II', info, 4, 0x2a8, 0x2a8)
        for field, value in ((0x60,string_positions[texture.name]), (0x68,0x20),
                              (0x70,offset+0x2a0), (0x80,offset+0xa0), (0x88,offset+0x1a0)):
            struct.pack_into('<Q', info, field, value)
            metadata_pointers.append(offset + field)
        output.extend(info)
        image_pointers.append(len(output))
        output.extend(bytes(8))
    # The native format places the BRTD header just before page-aligned images.
    data_start = (len(output) + 16 + 4095) & ~4095
    data_header = data_start - 16
    struct.pack_into('<II', output, infos[-1] + 4, data_header-infos[-1], data_header-infos[-1])
    output.extend(bytes(data_start - len(output)))
    for offset, texture in zip(infos,source):
        output.extend(bytes(-len(output) % integer(texture.info,0x54)))
        struct.pack_into('<Q', output, offset+0x2a0, len(output))
        output.extend(texture.data)
    output.extend(bytes(-len(output) % 4096))
    reloc = len(output)
    struct.pack_into('<4sIQ', output, data_header, b'BRTD', 0, reloc-data_header)
    groups = [sorted(metadata_pointers), sorted(image_pointers)]
    output.extend(struct.pack('<4sIII', b'_RLT', reloc, 2, 0))
    output.extend(struct.pack('<Q4I', 0,0,data_header,0,len(groups[0])))
    output.extend(struct.pack('<Q4I', 0,data_header,reloc-data_header,len(groups[0]),len(groups[1])))
    for group in groups:
        for offset in group:
            output.extend(struct.pack('<IHBB', offset,1,1,0))
    struct.pack_into('<IHHII', output, 16, string_positions[filename]+2, 0,strings,reloc,len(output))
    struct.pack_into('<IQQQ', output, 36,len(source),table,data_header,dic)
    return bytes(output)
