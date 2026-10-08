"""Compiled badge callbacks with native pane lookup and native catalog queries.

UI objects are synthetic verified-ABI fixtures; original HHP/Hotel row renderers
are stubbed. Tests do not run Eden or prove the live adapter chain. ItemParam,
ItemRemake, catalog/variant queries and recursive pane search are real game data
and code. No save file is accessed.
"""
import struct
import sys
from pathlib import Path
from compression import zstd
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import build_catalog_badge_layout as layouts
from catalog_variants_arm_test import Variants, furniture
from catalog_stars_arm_test import symbol, ids
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import *

NAMES = ['T_CatalogStar_00'] + [f'T_StarOutline_{i:02}' for i in range(8)]

class Badges(Variants):
    def __init__(self, base):
        super().__init__(base)
        self.ui = base + 0x23000000
        self.u.mem_map(self.ui, 0x30000)
        self.row = self.ui
        self.root = self.ui + 0x2000
        self.name_pane = self.ui + 0x2200
        self.badges = [self.ui + 0x2400 + i * 0x200 for i in range(9)]
        self.write(self.row + 0x2e0, self.ui + 0x500)
        self.write(self.ui + 0x500, self.ui + 0x600)
        self.write(self.ui + 0x648, self.ui + 0x700)
        self.direct_methods = self.ui + 0x9000
        self.group_methods = self.ui + 0xa000
        self.group = self.ui + 0xb000
        self.group_member = self.ui + 0xb100
        self.write(self.direct_methods + 0x5e0, base + 0x3c1b588)
        self.write(self.group_methods + 0x5e0, base + 0x3c47b90)
        self.write(self.group, self.group_member + 0x28)
        self.write(self.group + 8, self.group_member + 0x28)
        self.write(self.group + 0x10, 1, 4)
        self.write(self.group + 0x14, 0x28, 4)
        self.write(self.group_member + 0x10, self.name_pane)
        self.write(self.group_member + 0x18, self.direct_methods)
        self.write(self.group_member + 0x28, self.group)
        self.write(self.group_member + 0x30, self.group)
        self.use_group(True)
        # Match the real Pane/TextBox vtables: +c0 is the two-argument
        # recursive search, not the +60 three-argument dispatch wrapper.
        self.write(self.ui + 0x80c0, base + 0x27e6c0)
        self.write(self.ui + 0x8060, base + 0x27d1d0)
        self.load_layout('Nook')
        self.controller = self.ui + 0x4000
        self.write(self.controller + 0x1f8, 0, 1)
        for offset in (0x210, 0x218, 0x220): self.write(self.controller + offset, self.ui + 0x5000)
        self.write(self.ui + 0x5578, self.ui + 0x6000)
        self.write(self.ui + 0x5588, self.ui + 0x6000)
        self.write(self.ui + 0x6208, 1, 4)
        self.write(self.ui + 0x6210, self.ui + 0x7000)
        self.write(self.ui + 0x7000, self.row)
        self.orig = self.ui + 0xf000
        self.u.mem_write(self.orig, bytes.fromhex('c0035fd6')) # ret
        for shop in ('Hhp', 'Hotel', 'Miles'):
            self.write(self.mod + symbol(shop+'CatalogBadge', 's_FnPtr'), self.orig)
        self.orig_calls = 0
        self.forwarded_text_panes = []
        self.u.hook_add(UC_HOOK_CODE, self.count_original)

    def load_layout(self, shop):
        """Use each packaged BFLYT's actual hierarchy, including hidden badges.

        Pane allocation is still simulated; this does not exercise the native
        resource loader or prove what Eden loaded.
        """
        asset = {'Nook': 'LCatalogBtn', 'Hhp': 'LNatteeCatalogBtn',
                 'Hotel': 'LHotelCatalogBtn', 'Miles': 'LBtnListShopSonkatsuBtn'}[shop]
        packed = layouts.PROJECT.parent / 'vanilla-3.0.3/Layout' / (asset + '.Nin_NX_NVN.zs')
        entry = f'blyt/{asset}.bflyt'
        modified, _ = layouts.patch_archive(zstd.decompress(packed.read_bytes()), entry, visible=False)
        resources = layouts.resources(modified)
        data = next(data for key, data in resources.items() if key.endswith(entry))
        known = dict(zip(NAMES, self.badges))
        known.update(RootPane=self.root, T_Name_00=self.name_pane)
        stack, children, last = [], {}, None
        allocated = self.ui + 0x10000
        for section in layouts.layout_sections(data):
            if section[:4] in layouts.PANE_TYPES:
                name = layouts.pane_name(section)
                pane = known.get(name, allocated)
                allocated += 0x200
                self.u.mem_write(pane, bytes(0x200))
                self.write(pane, self.ui + 0x8000)
                self.u.mem_write(pane + 0xb0, name.encode() + b'\0')
                self.write(pane + 0x58, 0xb0 | (section[8] & 1), 1)
                children[pane] = []
                if stack:
                    children[stack[-1]].append(pane)
                    self.write(pane + 0x18, stack[-1])
                last = pane
            elif section[:4] == b'pas1': stack.append(last)
            elif section[:4] == b'pae1': stack.pop()
        assert not stack
        for parent, members in children.items():
            sentinel = parent + 0x20
            self.write(sentinel, members[-1] + 8 if members else sentinel)
            self.write(sentinel + 8, members[0] + 8 if members else sentinel)
            for index, pane in enumerate(members):
                self.write(pane + 8, members[index-1] + 8 if index else sentinel)
                self.write(pane + 0x10, members[index+1] + 8 if index+1 < len(members) else sentinel)
        assert self.root != int.from_bytes(self.u.mem_read(self.badges[0] + 0x18, 8), 'little')
        self.embed_row()

    def embed_row(self):
        # Live logs show that RootPane becomes L_Btn_nn when embedded. A
        # separate screen RootPane contains multiple rows with identical badge
        # names. Put a decoy row first to catch screen-wide searches.
        screen = self.ui + 0x28000
        neighbor = self.ui + 0x28200
        self.neighbor_badges = [self.ui + 0x28400 + index * 0x200 for index in range(9)]
        for pane, name in [(screen, 'RootPane'), (neighbor, 'L_Btn_00')]:
            self.u.mem_write(pane, bytes(0x200))
            self.write(pane, self.ui + 0x8000)
            self.u.mem_write(pane + 0xb0, name.encode() + b'\0')
        self.u.mem_write(self.root + 0xb0, b'L_Btn_01\0')
        for pane, name in zip(self.neighbor_badges, NAMES):
            self.u.mem_write(pane, bytes(0x200))
            self.write(pane, self.ui + 0x8000)
            self.u.mem_write(pane + 0xb0, name.encode() + b'\0')
            self.write(pane + 0x58, 0xa1, 1)
        links = {screen: [neighbor, self.root], neighbor: self.neighbor_badges}
        links.update({pane: [] for pane in self.neighbor_badges})
        for parent, members in links.items():
            sentinel = parent + 0x20
            self.write(sentinel, members[-1] + 8 if members else sentinel)
            self.write(sentinel + 8, members[0] + 8 if members else sentinel)
            for index, pane in enumerate(members):
                self.write(pane + 0x18, parent)
                self.write(pane + 8, members[index-1] + 8 if index else sentinel)
                self.write(pane + 0x10, members[index+1] + 8 if index+1 < len(members) else sentinel)


    def call(self, address):
        # Full row trees plus the one-time diagnostic format more text than
        # the shared balloon fixture's 50,000-instruction allowance.
        self.u.reg_write(UC_ARM64_REG_SP, self.sp)
        self.u.reg_write(UC_ARM64_REG_LR, self.stop)
        self.u.emu_start(address, self.stop, count=500000)
        assert self.u.reg_read(UC_ARM64_REG_PC) == self.stop
        assert self.u.reg_read(UC_ARM64_REG_SP) == self.sp

    def use_group(self, enabled):
        self.write(self.ui + 0x708, self.group if enabled else self.name_pane)
        self.write(self.ui + 0x718, self.group_methods if enabled else self.direct_methods)

    def count_original(self, u, pc, size, _):
        if pc == self.orig: self.orig_calls += 1
        if pc == self.base + 0x3c1b588:
            self.forwarded_text_panes.append(self.reg(0))
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))

    def check(self, shop, item, owned, expected, player=0, index=0, payload=0):
        bits = bytearray(0x754)
        if owned: bits[item//8] |= 1 << (item%8)
        self.u.mem_write(self.item, struct.pack('<HBBI', item, 0, 0, payload))
        self.u.mem_write(self.bits, bytes(bits))
        self.write(self.controller + 0x1f8, player, 1)
        if shop == 'Nook':
            self.write(self.ctx + 28*8, self.controller)
            self.write(self.ctx + 23*8, self.row)
            self.write(self.ctx + 11*8, self.item)
            self.u.reg_write(UC_ARM64_REG_X0, self.ctx)
        else:
            for register, value in ((UC_ARM64_REG_X0, self.controller),
                                    (UC_ARM64_REG_X1, self.item), (UC_ARM64_REG_X2, index)):
                self.u.reg_write(register, value)
        before = self.orig_calls
        self.call(self.mod + symbol(shop+'CatalogBadge', 'Callback'))
        assert self.orig_calls == before + (shop != 'Nook')
        flags = [bytes(self.u.mem_read(p+0x58, 1))[0] for p in self.badges]
        assert flags == [0xb0 | expected]*9, (shop,item,flags,self.logs[-1:])
        assert bytes(self.u.mem_read(self.bits, 0x754)) == bytes(bits)
        assert all(bytes(self.u.mem_read(pane + 0x58, 1)) == b'\xa1'
                   for pane in self.neighbor_badges), 'modified another catalog row'


def main():
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        q = Badges(base)
        for shop in ('Nook', 'Hhp', 'Hotel'):
            q.load_layout(shop)
            for owned in (False, True, False, True):
                q.check(shop, ids[0][0], owned, not owned); cases += 1
            q.check(shop, ids[0][0], False, False, player=8); cases += 1
            item, remake, _ = furniture[0]
            q.history(remake, None)
            q.check(shop, item, False, True); cases += 1
            # Another variant cataloged: no filled family badge.
            q.history(remake, 1)
            q.check(shop, item, True, False); cases += 1
            q.history(remake, 0)
            q.check(shop, item, True, False); cases += 1
            q.check(shop, 0xfffe, False, False); cases += 1
            # Incomplete resource hides remaining layers, never a bare outline.
            q.u.mem_write(q.badges[0]+0xb0, b'X')
            q.check(shop, ids[0][0], False, False); cases += 1
            q.u.mem_write(q.badges[0]+0xb0, b'T')
            label = {'Nook': 'Nook', 'Hhp': 'HHP', 'Hotel': 'Hotel'}[shop]
            assert any(f'CATALOG_TREE_END shop={label}' in log for log in q.logs)
            assert 'status=2 panes=8 visible=0' in q.logs[-1]
            traces = sum('CATALOG_TREE_END' in log for log in q.logs)
            for pane in q.badges: q.u.mem_write(pane + 0xb0, b'X')
            q.check(shop, ids[0][0], True, False); cases += 1
            assert 'status=1 panes=0 visible=0' in q.logs[-1]
            assert sum('CATALOG_TREE_END' in log for log in q.logs) == traces
            for pane in q.badges: q.u.mem_write(pane + 0xb0, b'T')
            # Missing row structure must not cause a search in the next row.
            icon = int.from_bytes(q.u.mem_read(q.badges[0] + 0x18, 8), 'little')
            q.u.mem_write(icon + 0xb0, b'X')
            q.check(shop, ids[0][0], False, False); cases += 1
            assert 'lookup=no-row-icon' in q.logs[-1]
            q.u.mem_write(icon + 0xb0, b'N')
            # Standalone roots and differently named embedded roots use the
            # same structural anchor, without depending on a particular slot.
            for root_name in (b'RootPane\0', b'L_Btn_07\0'):
                q.u.mem_write(q.root + 0xb0, root_name)
                q.check(shop, ids[0][0], False, True); cases += 1
                q.check(shop, ids[0][0], True, False); cases += 1

        # Exercise the real native group dispatcher against the same intrusive
        # list fixture; only final font/text work is stubbed. This independently
        # verifies the group member offsets used by the compiled lookup.
        q.u.reg_write(UC_ARM64_REG_X0, q.group)
        q.u.reg_write(UC_ARM64_REG_X1, q.item)
        q.u.reg_write(UC_ARM64_REG_X2, 1)
        q.call(base + 0x3c47b90)
        assert q.forwarded_text_panes == [q.name_pane]
        cases += 1
        # Two members with a nonzero embedded-node offset: the native renderer
        # visits both. Lookup must also skip an unsupported first member.
        second = q.ui + 0xb200
        q.write(q.group + 0x10, 2, 4)
        q.write(q.group, second + 0x28)
        q.write(q.group_member + 0x30, second + 0x28)
        q.write(second + 0x10, q.name_pane)
        q.write(second + 0x18, q.direct_methods)
        q.write(second + 0x28, q.group_member + 0x28)
        q.write(second + 0x30, q.group)
        q.u.reg_write(UC_ARM64_REG_X0, q.group)
        q.call(base + 0x3c47b90)
        assert q.forwarded_text_panes == [q.name_pane] * 3
        q.write(q.group_member + 0x18, q.ui + 0xc000)
        q.check('Nook', ids[0][0], False, True)
        q.check('Nook', ids[0][0], True, False)
        q.write(q.group_member + 0x18, q.direct_methods)
        cases += 2
        for grouped in (False, True):
            q.use_group(grouped)
            q.u.mem_write(q.name_pane + 0xb0, b'T_Name_01\0')
            for shop in ('Nook', 'Hhp', 'Hotel'):
                q.check(shop, ids[0][0], False, True); cases += 1
                q.check(shop, ids[0][0], True, False); cases += 1
        q.use_group(True)
        # Empty/unsupported groups must never be interpreted as raw panes.
        q.write(q.group + 0x10, 0, 4)
        q.check('Nook', ids[0][0], False, False); cases += 1
        q.write(q.group + 0x10, 1, 4)
        q.write(q.group_methods + 0x5e0, 0)
        q.check('Nook', ids[0][0], False, False); cases += 1
        q.write(q.group_methods + 0x5e0, base + 0x3c47b90)
        # Missing base name binding and invalid row indices are harmless.
        q.write(q.ui+0x708, 0)
        q.check('Nook', ids[0][0], False, False); cases += 1
        q.check('Hhp', ids[0][0], False, False, index=1); cases += 1
        q.check('Hotel', ids[0][0], False, False, index=0xffffffff); cases += 1
    print(f'PASS: {cases} badge cases at two bases; three callbacks, row reuse, native group dispatch, pane lookup and catalog queries')

if __name__ == '__main__': main()
