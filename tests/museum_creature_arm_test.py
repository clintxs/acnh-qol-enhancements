"""Compiled Catalog hooks + unmodified 3.0.3 museum lookup; synthetic saves only.

Real ItemParam supplies all 80 fish, 80 bugs and 40 sea creatures. Engine
provider/lock callbacks are mocked; native item resolution/comparison and
museum donor lookup execute ARM. Museum data is protected read-only.
"""
import struct
from catalog_stars_arm_test import Stars, symbol, nonwall, bcsv, field_offsets
from catalog_query_arm_test import RAW
from unicorn import UC_PROT_READ
from unicorn.arm64_const import *

KINDS = (0x9f46eb0d, 0x91b9b8d3, 0xe47038b8)
SPECIES = {kind: [i for i, row in nonwall
                 if struct.unpack_from('<I', bcsv, row + field_offsets[0x5270eb75])[0] == kind]
           for kind in KINDS}
assert [len(SPECIES[k]) for k in KINDS] == [80, 80, 40]
CATCH_SITES = ((0xc57ad8, 0xc57ae8), (0xc57b2c, 0xc57b3c), (0xc587ec, 0xc587fc))

class Museum(Stars):
    def __init__(self, base):
        super().__init__(base)
        # Model loader relocations for native BCSV accessors' global caches.
        for offset in range(0x4ab0000, 0x4ad1000, 8):
            target = struct.unpack_from('<Q', RAW, offset)[0]
            if 0x3f07000 <= target < 0x5800000 and bytes(self.u.mem_read(base+offset, 8)) == RAW[offset:offset+8]:
                self.write(base+offset, base+target)
        cache = struct.unpack_from('<Q', RAW, 0x4abfe90)[0]
        guard = struct.unpack_from('<Q', RAW, 0x4abfe88)[0]
        self.write(base+guard, 1, 1)
        self.write(base+cache+0x120, field_offsets[0xcb5eb33f], 4)
        self.m = base + 0x30000000
        self.u.mem_map(self.m, 0x400000)
        self.u.mem_map((self.owner + 0xaa9d8) & ~4095, 0x1000)
        self.island = self.m + 0x1000
        self.museum = self.m + 0x200000
        self.view = self.island + 0x70 + 0x1bf198
        self.action, self.event = self.m + 0x300000, self.m + 0x301000
        self.write(base + 0x5474040, self.m)
        self.write(self.m + 0x10, self.m + 0x500)
        self.write(self.m + 0x500, self.m + 0x800)
        self.write(self.m + 0x630, self.m + 0x800)  # alternate provider
        self.write(self.m + 0x800, self.m + 0x900)
        self.write(self.m + 0x938, base + 0x208)
        self.write(self.view, self.museum)
        self.write(self.island + 0x1bf198, self.museum)  # alternate island view
        # Native ItemKind cache: non-fossil kinds require no item normalization.
        # ItemKind wrapping metadata is irrelevant for these unwrapped catches;
        # an empty loaded table makes the native wrap predicate return false.
        self.write(base + 0x522a778, self.m + 0x320000)
        self.write(base + 0x5226ec0, 65536, 4)
        self.write(base + 0x5226ec8, self.m + 0x310000)
        self.u.mem_write(self.museum + 0x1004, struct.pack('<HBBI', 0xfffe, 0, 0, 0) * 1024)
        self.u.mem_write(self.museum + 0x3004, bytes([255]) * 1024)
        self.u.mem_protect(self.museum, 0x4000, UC_PROT_READ)
        self.write(self.action + 8, self.event)
        self.write(self.ctx + 20*8, self.action)
        self.write(self.ctx + 21*8, self.item)
        self.native_calls = 0
        self.getter_ready = True

    def hook(self, u, pc, size, data):
        if pc == self.base + 0x208:
            u.reg_write(UC_ARM64_REG_X0, self.island if self.getter_ready else 0)
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
        else:
            if pc == self.base + 0x26bf130: self.native_calls += 1
            super().hook(u, pc, size, data)

    def call(self, addr):
        self.u.reg_write(UC_ARM64_REG_SP, self.sp)
        self.u.reg_write(UC_ARM64_REG_LR, self.stop)
        self.u.emu_start(addr, self.stop, count=250000)
        assert self.u.reg_read(UC_ARM64_REG_PC) == self.stop
        assert self.u.reg_read(UC_ARM64_REG_SP) == self.sp

    def prepare(self, item, donor=255, slot=0, flags=0, player=0):
        self.u.mem_write(self.item, struct.pack('<HBBI', item, 0, flags, 0))
        self.write(self.museum + 0x1004 + slot*8, item, 2)
        self.write(self.museum + 0x3004 + slot, donor, 1)
        self.write(self.event + 0x2a8, player, 1)
        self.write(self.owner + 0x1f8, player, 1)
        self.name = self.frame - 0x440
        self.text = self.name + 0xf0
        self.reset_name()

    def reset_name(self):
        self.write(self.name + 8, 0, 4)
        self.write(self.name + 0x18, self.text)
        self.write(self.name + 0x20, 128, 4)
        self.u.mem_write(self.text, 'sea bass\0'.encode('utf-16-le'))

    def run_hook(self, hook, expected):
        before = bytes(self.u.mem_read(self.museum, 0x3404))
        context = bytes(self.u.mem_read(self.ctx, 31*8))
        item = bytes(self.u.mem_read(self.item, 8))
        self.u.reg_write(UC_ARM64_REG_X0, self.ctx)
        self.call(self.mod + symbol(hook, 'Callback'))
        actual = bytes(self.u.mem_read(self.text, len(expected)*2 + 2))
        assert actual == (expected+'\0').encode('utf-16-le'), (hook, expected, actual, self.logs[-1:])
        assert bytes(self.u.mem_read(self.museum, 0x3404)) == before
        assert bytes(self.u.mem_read(self.ctx, 31*8)) == context
        assert bytes(self.u.mem_read(self.item, 8)) == item

    def pocket(self):
        self.name, self.text = self.frame - 0x440, self.frame - 0x440 + 0xf0
        self.write(self.owner, self.base + 0x4a06e28)
        self.write(self.owner + 0x258, self.window)
        self.write(self.frame + 8, self.base + 0x30883d4)

    def catch(self, site):
        # Run original native constructor argument setup to establish name/item.
        self.u.reg_write(UC_ARM64_REG_SP, self.frame - 0x1b50)
        self.u.reg_write(UC_ARM64_REG_X21, self.item)
        self.u.emu_start(self.base + site[0], self.base + site[1])
        self.name, source = self.reg(0), self.reg(1)
        assert source == self.item
        assert self.name == self.frame - 0x1b50 + 0x400
        self.text = self.name + 0xf0
        self.reset_name()


def main():
    count = 0
    for base in (0x7100000000, 0x7200000000):
        q = Museum(base)
        q.call(q.mod + symbol('ValidateCatalogBindings'))
        assert q.reg(0) == 1
        for kind, site in zip(KINDS, CATCH_SITES):
            for item in SPECIES[kind]:
                for donor in (255, 0, 7):
                    q.prepare(item, donor)
                    q.pocket()
                    expected = '★ Sea bass' if donor == 255 else 'sea bass'
                    q.run_hook('CabinetCatalogStar', expected)
                    q.catch(site)
                    q.run_hook('DialogueCatchCreatureStar', '★ sea bass' if donor == 255 else 'sea bass')
                    count += 2
        item = SPECIES[KINDS[0]][0]
        # Storage's existing mode/controller gate also applies to creatures.
        for mode, expected in ((0, '★ Sea bass'), (1, 'sea bass')):
            q.prepare(item)
            q.write(q.owner, base + 0x4a16028)
            q.write(q.owner + 0xaa9d8, q.window)
            q.write(q.owner + 0x220, mode, 4)
            q.write(q.frame + 8, base + 0x316a5a0)
            q.run_hook('CabinetCatalogStar', expected); count += 1
        # Ordinary event item substitutions also use museum ownership.
        for donor in (255, 7):
            q.prepare(item, donor)
            q.name = q.frame - 0x1b50 + 0x400
            q.text = q.name + 0xf0
            q.reset_name()
            q.u.mem_write(q.frame - 0x1b50 + 0x28, struct.pack('<HBBI', item, 0, 0, 0))
            q.run_hook('DialogueValueItemStar', '★ sea bass' if donor == 255 else 'sea bass'); count += 1

        # Catch/catalog bits never influence donations; live donation change has
        # no cache, and every active resident sees another resident's donation.
        for player in range(8):
            for donor in (255, (player+1)%8):
                q.prepare(item, donor, player=player); q.pocket()
                q.u.mem_write(q.bits, bytes([255]) * 0x754)
                q.run_hook('CabinetCatalogStar', '★ Sea bass' if donor == 255 else 'sea bass')
                count += 1
        # Native lock paired once; failed acquisition is unavailable.
        q.prepare(item); q.pocket(); q.write(q.view + 0x20, q.lock)
        for result, expected, calls in ((1, '★ Sea bass', [0x200, 0x204]), (0, 'sea bass', [0x200])):
            q.reset_name()
            q.lock_result, q.lock_calls = result, []
            q.run_hook('CabinetCatalogStar', expected)
            assert q.lock_calls == calls
            count += 1
        q.write(q.view + 0x20, 0)
        for address, good in ((base + 0x5474040, q.m), (q.m + 0x10, q.m + 0x500),
                              (q.m + 0x500, q.m + 0x800), (q.view, q.museum),
                              (base + 0x5226ec8, q.m + 0x310000),
                              (q.rows + item*8, q.data + dict(nonwall)[item])):
            q.prepare(item); q.pocket(); q.write(address, 0)
            q.run_hook('CabinetCatalogStar', 'sea bass'); q.write(address, good); count += 1
        q.getter_ready = False; q.prepare(item); q.pocket()
        q.run_hook('CabinetCatalogStar', 'sea bass'); q.getter_ready = True; count += 1
        for flags in (1, 2, 3):
            q.prepare(item, flags=flags); q.pocket(); q.run_hook('CabinetCatalogStar', 'sea bass'); count += 1
        for other in (117, 5794, 0xfffe):
            q.prepare(other); q.pocket(); q.run_hook('CabinetCatalogStar', 'sea bass'); count += 1
        # Alternate island uses its own view (no +70 adjustment).
        q.write(q.m + 0x453, 1, 1)
        q.prepare(item); q.pocket(); q.run_hook('CabinetCatalogStar', '★ Sea bass'); count += 1
        q.write(q.m + 0x453, 0, 1)
        # Last museum slot and entirely absent species both use native lookup.
        q.write(q.museum + 0x1004, 0xfffe, 2)
        q.prepare(item, donor=7, slot=1023); q.pocket()
        q.run_hook('CabinetCatalogStar', 'sea bass'); count += 1
        q.write(q.museum + 0x1004 + 1023*8, 0xfffe, 2)
        q.reset_name(); q.run_hook('CabinetCatalogStar', '★ Sea bass'); count += 1
    print(f'PASS: {count} museum creature cases at two ASLR bases; all 200 species, native donation lookup, '
          'catch/pockets, residents, guards, locks and unchanged museum data')

if __name__ == '__main__': main()
