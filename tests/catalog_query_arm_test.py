"""Execute the 3.0.3 native catalog query, not a reimplementation.

Run from the workspace with PYTHONPATH='Grab 5/working/python'. Uses the
local main.raw (never packaged). Player resolution, lock callbacks, random
seed and memcpy are mocked; query, ID resolution and bit test execute ARM.
No mod hooks are installed and no save file is read or written.
"""
from pathlib import Path
import hashlib
import struct
import sys

from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_PROT_READ, UC_PROT_EXEC
from unicorn.arm64_const import *

ROOT = Path(__file__).resolve().parents[2]
RAW = (ROOT / 'Grab 5/working/main.raw').read_bytes()
assert hashlib.sha256(RAW).hexdigest() == 'b1ae8e2a0d82f9f38f711c132d1751b221baf9d6411b2be06ebc4bb502ac912d'


class NativeQuery:
    def __init__(self, base):
        self.base, self.ram = base, base + 0x10000000
        self.u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        self.u.mem_map(base, 0x5800000)
        self.u.mem_write(base, RAW)
        self.u.mem_map(self.ram, 0x200000)
        self.player = self.ram
        self.item = self.ram + 0x1000
        self.bits = self.ram + 0x2000
        self.manager = self.ram + 0x10000
        self.table = self.ram + 0x70000
        self.rows = self.ram + 0x80000
        self.save = self.ram + 0x110000
        self.other = self.save + 0x194b0
        self.status = self.ram + 0x140000
        self.lock = self.ram + 0x160000
        self.sp = self.ram + 0x1f0000
        self.stop = base + 0x100
        self.mapped_player, self.have_save = 0, True
        self.lock_result, self.lock_calls = 1, []
        # Resolve imported global pointer slots explicitly at either load base.
        self.write(base + 0x4ab43c0, self.ram + 0x3000)
        self.write(self.ram + 0x3000, self.manager)
        self.write(self.manager + 0x57e58, self.table)
        self.write(self.table + 0x1d8, 65536, 4)
        self.write(self.table + 0x1e0, self.rows)
        self.write(self.table + 0x1f8, 0, 2)
        self.u.mem_write(self.rows, struct.pack('<Q', self.ram + 0x4000) * 65536)
        self.write(base + 0x4ab4fb8, self.ram + 0x3010)
        self.write(self.ram + 0x3010, self.status)
        self.write(self.other + 8, 1)
        self.write(self.other + 0x1070, self.bits)
        self.write(self.other + 0x1078, 0)
        self.write(base + 0x5474040, self.ram + 0x3020)
        self.write(self.ram + 0x3040, self.other)
        self.write(self.lock, self.lock + 0x20)
        self.write(self.lock + 8, self.lock + 0x30)
        self.write(self.lock + 0x20, self.lock + 0x40)
        self.write(self.lock + 0x30, self.lock + 0x50)
        self.write(self.lock + 0x40, base + 0x200)
        self.write(self.lock + 0x50, base + 0x204)
        for slot in range(8):
            self.write(self.status + slot * 0x13f0 + 0x13d4, 1, 1)
            self.write(self.status + slot * 0x13f0 + 0x13d6, 1, 1)
        self.u.hook_add(UC_HOOK_CODE, self.hook)
        self.u.mem_protect(base, 0x3f07000, UC_PROT_READ | UC_PROT_EXEC)

    def write(self, address, value, size=8):
        self.u.mem_write(address, value.to_bytes(size, 'little'))

    def reg(self, i):
        return self.u.reg_read(UC_ARM64_REG_X0 + i)

    def hook(self, u, pc, size, _):
        off = pc - self.base
        if off == 0x2482d8c:
            assert self.reg(1) == 1
            result = self.mapped_player
        elif off == 0x2483354:
            assert self.reg(2) == 1
            assert bytes(u.mem_read(self.reg(1), 1)) == bytes(u.mem_read(self.player, 1))
            result = self.save if self.have_save else 0
        elif off == 0x3f030c0:
            result = 0x12345678  # Temporary bitset object's random seed only.
        elif off == 0x3f04470:
            assert self.reg(2) == 0x754
            u.mem_write(self.reg(0), bytes(u.mem_read(self.reg(1), self.reg(2))))
            result = self.reg(0)
        elif off in (0x200, 0x204):
            self.lock_calls.append(off)
            result = self.lock_result if off == 0x200 else 0
        else:
            return
        u.reg_write(UC_ARM64_REG_X0, result)
        u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))

    def query(self, item_id, owned, player=0, locked=False, payload=0):
        item = struct.pack('<HBBI', item_id, 0, 0, payload)
        bits = bytearray(0x754)
        if owned and item_id < 15000:
            bits[item_id // 8] |= 1 << (item_id % 8)
        self.u.mem_write(self.player, bytes([player]))
        self.u.mem_write(self.item, item)
        self.u.mem_write(self.bits, bytes(bits))
        self.write(self.other + 0x1090, self.lock if locked else 0)
        self.lock_calls.clear()
        self.u.reg_write(UC_ARM64_REG_X0, self.player)
        self.u.reg_write(UC_ARM64_REG_X1, self.item)
        self.u.reg_write(UC_ARM64_REG_SP, self.sp)
        self.u.reg_write(UC_ARM64_REG_LR, self.stop)
        # Catch any attempted mutation of the source item/history in native code.
        self.u.mem_protect(self.item, 0x2000, UC_PROT_READ)
        self.u.emu_start(self.base + 0x254a1e0, self.stop, count=10000)
        assert self.u.reg_read(UC_ARM64_REG_PC) == self.stop
        assert self.u.reg_read(UC_ARM64_REG_SP) == self.sp
        assert bytes(self.u.mem_read(self.item, 8)) == item
        assert bytes(self.u.mem_read(self.bits, 0x754)) == bytes(bits)
        return self.reg(0)


def main():
    # Real current wallpaper/floor IDs; the engine table presence is mocked.
    sys.path.insert(0, str(ROOT / 'CylindricalEarth'))
    import bcsv
    import specs_200
    table = bcsv.File(specs_200.ItemParam)
    table.load((ROOT / 'vanilla-3.0.3/Bcsv/ItemParam.bcsv').read_bytes())
    ids = [r.UniqueID for r in table.rows if r.ItemKind[0] in ('RoomWall', 'RoomFloor')]
    assert len(ids) == 527 and all(0 <= i < 15000 for i in ids)
    count = 0
    for base in (0x7100000000, 0x7200000000):
        q = NativeQuery(base)
        for item_id in ids + [0, 7, 8, 31, 32, 14999]:
            for owned in (False, True):
                assert q.query(item_id, owned) == int(owned), (item_id, owned)
                count += 1
        for player in (8, 255):
            assert q.query(ids[0], True, player=player) == 0
            count += 1
        q.mapped_player = 255
        assert q.query(ids[0], True) == 0
        q.mapped_player = 0
        q.have_save = False
        assert q.query(ids[0], True) == 0
        q.have_save = True
        count += 2
        for off in (0x13d4, 0x13d6):
            q.write(q.status + off, 0, 1)
            assert q.query(ids[0], True) == 0
            q.write(q.status + off, 1, 1)
            count += 1
        for result in (0, 1):
            q.lock_result = result
            assert q.query(ids[0], True, locked=True) == 1
            assert q.lock_calls == ([0x200, 0x204] if result else [0x200])
            count += 1
        q.write(q.other + 8, 0)  # Native fallback save view.
        assert q.query(ids[0], True) == 1
        q.write(q.other + 8, 1)
        count += 1
        for item_id in (15000, 65533, 65534, 65535):
            assert q.query(item_id, False) == 0
            count += 1
        q.write(q.rows + ids[0] * 8, 0)
        assert q.query(ids[0], True) == 0
        q.write(q.rows + ids[0] * 8, q.ram + 0x4000)
        q.write(q.ram + 0x3000, 0)
        assert q.query(ids[0], True) == 0
        q.write(q.ram + 0x3000, q.manager)
        count += 2
        assert q.query(ids[0], True, payload=0x12345678) == 1
        count += 1
    print(f'{count} native catalog query cases passed at two load bases; engine services mocked.')


if __name__ == '__main__':
    main()
