"""Check packaged textures, GPU byte order and the game's texture-name search.

Native SDK relocation is checked against the serialized RLT model. Native main
code performs dictionary traversal and string comparisons are modeled. GPU
sampling/drawing still requires an Eden playtest.
"""
from pathlib import Path
from compression import zstd
import hashlib
import struct
import sys
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import *
sys.path.insert(0,str(Path(__file__).resolve().parents[1] / 'tools'))
import build_shopping_card_layout as card
import shopping_card_texture as art
from layout_io import resources
from shopping_bntx import textures


def read(data, offset, fmt='I'):
    return struct.unpack_from('<'+fmt,data,offset)[0]


def decode_rgba(data, width, height):
    # Independent inverse: walk GOB physical addresses in memory order. The
    # intra-GOB bits alternate byte-column/row fields (Tegra block-linear).
    raw = bytearray(len(data))
    for address in range(0,len(data),4):
        gob, local = divmod(address,512)
        byte_x = (local & 15) | ((local >> 1) & 16) | ((local >> 3) & 32)
        y = ((local >> 4) & 1) | ((local >> 5) & 6)
        x = (gob % (width//16))*16 + byte_x//4
        y += (gob//(width//16))*8
        assert x < width and y < height
        destination = 4*(y*width+x)
        raw[destination:destination+4] = data[address:address+4]
    return bytes(raw)


def relocate(data, base):
    result = bytearray(data)
    table = read(data,24)
    assert data[table:table+4] == b'_RLT'
    assert read(data,table+4) == table and read(data,table+8) == 2
    sections = [struct.unpack_from('<Q4I',data,table+16+i*24) for i in range(2)]
    entries = table+16+len(sections)*24
    seen = set()
    for zero, position, size, first, count in sections:
        assert zero == 0 and position+size <= table
        for i in range(first,first+count):
            offset, arrays, pointers, padding = struct.unpack_from('<IHBB',data,entries+i*8)
            for array in range(arrays):
                for pointer in range(pointers):
                    location = offset + 8*(array*(pointers+padding)+pointer)
                    assert location not in seen
                    seen.add(location)
                    target = read(data,location,'Q')
                    if target:
                        assert position <= target < position+size
                        struct.pack_into('<Q',result,location,base+target)
    # Check actual required fields rather than just comparing our writer's list.
    required = {40,48,56,64}
    count, pointers = read(data,36), read(data,40,'Q')
    dictionary = read(data,56,'Q')
    required.update(dictionary+16+i*16 for i in range(count+1))
    for i in range(count):
        required.add(pointers+8*i)
        info = read(data,pointers+8*i,'Q')
        required.update(info+j for j in (0x60,0x68,0x70,0x80,0x88))
        required.add(read(data,info+0x70,'Q'))
    assert required <= seen
    return bytes(result)


original = resources(zstd.decompress((card.ROOT.parent / f'vanilla-3.0.3/Layout/{card.ASSET}.Nin_NX_NVN.zs').read_bytes()))
modified = resources(zstd.decompress(next(iter(card.build().values()))))
key = next(k for k in original if k.endswith('.bntx'))
before,after = textures(original[key]),textures(modified[key])
# Walk the resource block chain independently of texture-table pointers.
container = modified[key]
block = read(container,22,'H')
assert container[block:block+4] == b'_STR'
block += read(container,block+4)
for _ in range(9):
    assert container[block:block+4] == b'BRTI'
    assert read(container,block+4) == read(container,block+8,'Q')
    block += read(container,block+4)
assert block == read(container,48,'Q') and container[block:block+4] == b'BRTD'
assert block + read(container,block+8,'Q') == read(container,24)
assert len(before) == 7 and len(after) == 9
for a,b in zip(before,after):
    assert a.name == b.name and a.data == b.data and a.info[16:96] == b.info[16:96]
for texture,backing in zip(after[7:],(True,False)):
    assert read(texture.info,0x1c) == 0x0b01
    assert struct.unpack_from('<2I',texture.info,0x24) == (art.WIDTH,art.HEIGHT)
    assert read(texture.info,0x34) == 0 and texture.info[0x58:0x5c] == bytes((2,3,4,5))
    decoded = decode_rgba(texture.data,art.WIDTH,art.HEIGHT)
    assert decoded == art.pixels(backing)
    assert any(0 < a < 255 for a in decoded[3::4]) # Smooth coverage at corners.
    def pixel(x,y):
        column = int((x+64)*2); row = int((48-y)*2)
        return tuple(decoded[4*(row*art.WIDTH+column):4*(row*art.WIDTH+column)+4])
    assert pixel(-63,47)[3] == 0 and pixel(0,0)[3] == 255
    if not backing:
        assert pixel(0,0) == art.FACE
        assert pixel(40,0) == art.INK
        assert pixel(0,10) == art.CREAM and pixel(-23,-7) == art.CHIP
        assert pixel(16,-4.5) == art.CREAM and pixel(9,-11.5) == art.CREAM
print('PASS: original seven textures preserved; two RGBA8 images, independent GPU-byte-order decode, complete colored card and antialiased corners')

from nso_reader import image
SDK = image(card.ROOT.parent/'Extracted/Animal Crossing_ New Horizons 3.0.3 [01006F8002326800][v2228224][UPD]/Program #0/0/sdk',
            'a226048605d3597f90cb1067d141cd36647b90cd000000000000000000000000')
RAW = (card.ROOT.parent/'Grab 5/working/main.raw').read_bytes()
# Main's inline texture dictionary traversal in ArcResourceAccessor. Fingerprint
# both noncontiguous blocks; refuses a different game implementation.
assert hashlib.sha256(RAW[0x263368:0x2633c0]+RAW[0x263410:0x263464]).hexdigest() == '912a64b6fe7ea0f0996de1e2b172de983a88e7deb747f87392f27d34423e1207'
for base in (0,0x7100000000):
    machine = Uc(UC_ARCH_ARM64,UC_MODE_ARM)
    machine.mem_map(base+0x263000,0x1000)
    machine.mem_write(base+0x263000,RAW[0x263000:0x264000])
    machine.mem_map(base+0x3f03000,0x1000)
    ram = base+0x6000000
    machine.mem_map(ram,0x200000)
    sdk_base = base+0x1000000
    machine.mem_map(sdk_base,(len(SDK)+4095)&~4095)
    machine.mem_write(sdk_base,SDK)
    result = []
    def service(u,pc,size,data):
        if pc == base+0x3f03380: # strcmp import
            def cstring(address):
                value = bytearray()
                while (byte := u.mem_read(address,1)[0]):
                    value.append(byte); address += 1
                return bytes(value)
            a,b = (cstring(u.reg_read(r)) for r in (UC_ARM64_REG_X0,UC_ARM64_REG_X1))
            u.reg_write(UC_ARM64_REG_X0,0 if a == b else 1)
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))
        elif pc in (base+0x26346c,base+0x2633c0):
            result.append(u.reg_read(UC_ARM64_REG_X21) if pc == base+0x26346c else None)
            u.emu_stop()
    machine.hook_add(UC_HOOK_CODE,service)
    for resource in (original[key],modified[key]):
        # Execute the real SDK leaf routine. No imported functions or GPU
        # services are needed; compare every relocated byte with the model.
        machine.mem_write(ram,resource)
        machine.reg_write(UC_ARM64_REG_X0,ram+read(resource,24))
        stop = ram+0x1ff000
        machine.reg_write(UC_ARM64_REG_LR,stop)
        machine.emu_start(sdk_base+0x81a90,stop,count=100000)
        assert machine.reg_read(UC_ARM64_REG_PC) == stop
        expected = bytearray(relocate(resource,ram))
        expected[20] |= 1 # SDK marks the file relocated.
        assert bytes(machine.mem_read(ram,len(resource))) == expected

        dic = ram+read(resource,56,'Q')
        names = [t.name for t in textures(resource)]
        for name in names+['MissingShoppingCard^w','QoLShoppingCardIcon','']:
            string = ram+0x100000
            machine.mem_write(string,name.encode()+b'\0')
            machine.reg_write(UC_ARM64_REG_X20,dic)
            machine.reg_write(UC_ARM64_REG_X19,string)
            machine.reg_write(UC_ARM64_REG_X2,len(name))
            result.clear()
            machine.emu_start(base+0x263368,base+0x263480,count=10000)
            assert len(result) == 1
            index = None if result[0] is None else (result[0]-dic-8)//16-1
            # The root sentinel represents FindIndex's -1 result for an empty key.
            if index == -1: index = None
            assert index == (names.index(name) if name in names else None), (name,index)
    print(f'PASS: native SDK relocation and 3.0.3 texture-name lookup, original/expanded containers, absent keys, base {base:#x}')
