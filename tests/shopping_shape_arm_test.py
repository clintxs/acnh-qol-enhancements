"""Execute 3.0.3's shape-table reader, PIC shape selection and mesh generation.

Only the heap and imported sinf/cosf services are modeled. This verifies native
resource decoding and actual triangles, not the GPU shader or in-game rendering.
Run with the local main dump and tools/check.py's Unicorn Python environment.
"""
from pathlib import Path
import math
import struct
import sys
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import *
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import build_shopping_card_layout as card
from compression import zstd
from layout_io import resources, layout_sections, pane_name

RAW = (card.ROOT.parent / 'Grab 5/working/main.raw').read_bytes()
# Fingerprints make this test refuse a different engine implementation.
assert RAW[0x268774:0x268788].hex() == '690e8e52292da6721f01096b81020054750e00f9'
# The resource bytes under test come from the actual packaged layout builder.
archive = resources(zstd.decompress(next(iter(card.build().values()))))
sections = layout_sections(next(v for k,v in archive.items() if k.endswith('.bflyt')))
shape = next(s for s in sections if s[:4] == b'spi1')
panes = {pane_name(s): s for s in sections if s[:4] == b'pic1'}

class ShapeMachine:
    SECTION_READER = 0x268550
    PIC_SHAPE_SELECTION = 0x282d9c
    PIC_MATERIAL_SETUP = 0x282e3c
    ALLOCATE = 0x267b30
    SINF = 0x3f03310
    COSF = 0x3f03320

    def __init__(self, base):
        self.base = base
        self.ram = base + 0x6000000
        self.u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        self.u.mem_map(base, 0x6000000)
        self.u.mem_write(base, RAW)
        self.u.mem_map(self.ram, 0x100000)
        self.heap = self.ram + 0x20000
        self.allocations = []
        self.u.hook_add(UC_HOOK_CODE, self.service)
        # Dynamic-loader relocations used by RoundRectShape and its pi constant.
        self.write(base + 0x4ad0920, base + 0x474da30)
        self.write(base + 0x474da50, base + 0x28e6a4)
        self.write(base + 0x4acee68, self.ram + 0x800)
        self.u.mem_write(self.ram + 0x800, struct.pack('<f', math.pi))

    def write(self, address, value, size=8):
        self.u.mem_write(address, int(value).to_bytes(size, 'little'))

    def read(self, address, size=8):
        return int.from_bytes(self.u.mem_read(address, size), 'little')

    def service(self, u, pc, size, data):
        offset = pc - self.base
        if offset == self.ALLOCATE:
            size = u.reg_read(UC_ARM64_REG_X0)
            pointer = self.heap
            self.heap += (size + 31) & ~31
            self.allocations.append((pointer, size))
            # Guard each allocation so overrun by mesh generation is observable.
            u.mem_write(pointer, bytes([0xcd]) * ((size + 31) & ~31))
            u.reg_write(UC_ARM64_REG_X0, pointer)
        elif offset in (self.SINF, self.COSF):
            value = struct.unpack('<f', struct.pack('<I', u.reg_read(UC_ARM64_REG_S0)))[0]
            value = math.sin(value) if offset == self.SINF else math.cos(value)
            u.reg_write(UC_ARM64_REG_S0, struct.unpack('<I', struct.pack('<f', value))[0])
        else:
            return
        u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))

    def run(self, offset, stop=None):
        stop = self.ram + 0xff000 if stop is None else self.base + stop
        self.u.reg_write(UC_ARM64_REG_SP, self.ram + 0xfe000)
        self.u.reg_write(UC_ARM64_REG_LR, stop)
        self.u.emu_start(self.base + offset, stop, count=200000)
        assert self.u.reg_read(UC_ARM64_REG_PC) == stop
        assert self.u.reg_read(UC_ARM64_REG_SP) == self.ram + 0xfe000

    def build(self, pane):
        context, header, table = (self.ram + o for o in (0x1000, 0x2000, 0x3000))
        self.u.mem_write(table, shape)
        self.write(context + 0x128, table)
        self.write(context + 0x120, 0, 4)
        self.write(header + 16, 1, 2)
        self.u.reg_write(UC_ARM64_REG_X0, context)
        self.u.reg_write(UC_ARM64_REG_X1, header)
        self.run(self.SECTION_READER)
        assert self.read(context + 0x18) == table
        assert self.read(context + 0x140) == 0
        assert self.read(context + 0x13c, 4) == 1
        pic, resource, build_set = (self.ram + o for o in (0x4000, 0x5000, 0x6000))
        self.u.mem_write(pic, bytes(0x200))
        self.u.mem_write(resource, pane)
        self.write(build_set + 0x30, context)
        for register, value in ((UC_ARM64_REG_X19, 0), (UC_ARM64_REG_X20, 0),
                                (UC_ARM64_REG_X21, build_set), (UC_ARM64_REG_X22, pic),
                                (UC_ARM64_REG_X25, resource), (UC_ARM64_REG_X26, pic + 0xf0),
                                (UC_ARM64_REG_X9, 3)):
            self.u.reg_write(register, value)
        self.run(self.PIC_SHAPE_SELECTION, self.PIC_MATERIAL_SETUP)
        assert self.read(pic + 0xf0, 1) == 4
        wrapper = self.read(pic + 0xf8)
        assert self.read(wrapper, 2) == 1
        obj = self.read(wrapper + 0x18)
        width, height = struct.unpack_from('<2f', pane, 76)
        index = struct.unpack_from('<I', pane, 104)[0]
        radius = card.CARD_RADII[index]
        actual = struct.unpack('<2f', self.u.mem_read(obj + 0x40, 8))
        assert math.isclose(actual[0], 2 * radius / width, rel_tol=1e-6)
        assert math.isclose(actual[1], 2 * radius / height, rel_tol=1e-6)
        divisions = self.read(obj + 0x48, 4)
        vertices, indices = self.read(obj + 0x24, 4), self.read(obj + 0x28, 4)
        assert divisions == card.CORNER_DIVISIONS
        assert (vertices, indices) == (4 * divisions + 20, 12 * divisions + 18)
        vertex_buffer, index_buffer = self.read(obj + 0x10), self.read(obj + 8)
        coords = [struct.unpack('<5f', self.u.mem_read(vertex_buffer + i * 20, 20)) for i in range(vertices)]
        assert all(math.isfinite(v) and abs(v) <= 1.00001 for c in coords for v in c)
        mesh = struct.unpack(f'<{indices}H', self.u.mem_read(index_buffer, indices * 2))
        assert max(mesh) < vertices
        area = 0
        for i in range(0, indices, 3):
            a,b,c = [coords[j] for j in mesh[i:i+3]]
            area += abs((b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0])) * width * height / 8
        expected_area = width * height - 4 * radius**2 + 2 * divisions * radius**2 * math.sin(math.pi / (2 * divisions))
        assert math.isclose(area, expected_area, rel_tol=1e-5)
        assert math.isclose(max(c[0] for c in coords), 1, abs_tol=1e-6)
        assert math.isclose(min(c[0] for c in coords), -1, abs_tol=1e-6)
        assert math.isclose(max(c[1] for c in coords), 1, abs_tol=1e-6)
        assert math.isclose(min(c[1] for c in coords), -1, abs_tol=1e-6)
        assert len(set((round(x, 5), round(y, 5)) for x,y,*_ in coords)) > 4 * divisions
        # Actual curve points must lie on all four requested corner circles.
        corners = [0, 0, 0, 0]
        for x,y,*_ in coords:
            px, py = x * width / 2, y * height / 2
            cx = math.copysign(width / 2 - radius, px)
            cy = math.copysign(height / 2 - radius, py)
            if abs(px) >= width / 2 - radius - .001 and abs(py) >= height / 2 - radius - .001:
                distance = math.hypot(px-cx, py-cy)
                # Each corner fan also has a center vertex inside the shape.
                assert distance < .001 or abs(distance - radius) < .001
                if distance > .001:
                    corners[(px < 0) + 2 * (py < 0)] += 1
        assert min(corners) >= divisions
        for pointer, size in self.allocations:
            padding = ((size + 31) & ~31) - size
            assert bytes(self.u.mem_read(pointer + size, padding)) == bytes([0xcd]) * padding
        return vertices, indices

for base in (0x7100000000, 0x7302000000):
    machine = ShapeMachine(base)
    for name in ('P_CardRim_00', 'P_CardEdge_00', 'P_CardFace_00'):
        counts = machine.build(panes[name])
        print(f'PASS: native spi1 reader/PIC selection/RoundRect mesh {name}: {counts}, base {base:#x}')
print('PASS: three compact curved silhouettes, bounded mesh buffers, actual native corner circles; GPU/live display still requires playtest')
