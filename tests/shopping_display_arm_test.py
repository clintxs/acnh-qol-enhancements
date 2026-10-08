"""Cash refresh regression and savings presentation using the compiled UI hook."""
from shopping_card_arm_test import CardMachine, SHOPS
from unicorn.arm64_const import *
from unicorn import UC_HOOK_CODE

class DisplayMachine(CardMachine):
    def __init__(self, base):
        super().__init__(base)
        self.owner = self.ram + 0x4000
        self.counter = self.ram + 0x5000
        self.binding = self.ram + 0x6000
        self.adapter = self.ram + 0x6100
        self.methods = self.ram + 0x6200
        self.display_updates = 0
        self.texts = []
        self.panes = {}
        self.parents = {}
        for i, name in enumerate(('RootPane', 'N_Pos_01', 'T_Num_00', 'N_ShoppingCard_00',
                                  'T_SavingsNum_00', 'P_IconBase_00', 'W_BaseL_00',
                                  'W_BaseStripe_00', 'N_RegionType_00', 'P_Icon_00')):
            pane = self.ram + 0x80000 + i * 0x200
            self.panes[name] = pane
            self.u.mem_write(pane + 0xb0, name.encode() + b'\0')
            self.write(pane + 0x58, 2 if name == 'N_ShoppingCard_00' else 3, 1)
        for name, pane in self.panes.items():
            parent = 0 if name == 'RootPane' else self.panes[
                'RootPane' if name == 'N_Pos_01' else
                'N_ShoppingCard_00' if name == 'T_SavingsNum_00' else 'N_Pos_01']
            self.parents[pane] = parent
            self.write(pane + 0x18, parent)
        self.write(self.owner + 0x210, self.counter)
        self.write(self.counter + 0x250, self.binding)
        self.write(self.binding + 0x48, self.adapter)
        self.write(self.adapter + 8, self.panes['T_Num_00'])
        self.write(self.adapter + 0x18, self.methods)
        self.write(self.methods + 0x5e0, base + 0x3c1b588)
    def service(self, u, pc, size, data):
        off = pc - self.base
        if off == 0x27d1d0:
            root = self.reg(0); name = self.string(self.reg(1))
            pane = self.panes.get(name, 0); node = pane
            while node and node != root: node = self.parents.get(node, 0)
            self.finish(pane if node else 0); return
        if off == 0x3c1b588:
            assert self.reg(0) == self.panes['T_SavingsNum_00']
            text = bytes(u.mem_read(self.reg(1), self.reg(2) * 2)).decode('utf-16-le')
            assert self.read(self.reg(1) + self.reg(2) * 2, 2) == 0
            self.texts.append(text); self.finish(); return
        super().service(u, pc, size, data)
    def update(self): self.invoke(self.callbacks[0x301b394], self.owner)
    def shown(self, name): return bool(self.read(self.panes[name] + 0x58, 1) & 1)
    def native_refresh(self, entry, end, player_offset, saved_reg, cached=None):
        # Execute the original "current funds changed?" branch. Its actual BL
        # runs the compiled ShopWallet hook; a mistaken savings response would
        # queue the native amount animation (counter+290) and ka-ching sound.
        current = self.wallet if cached is None else cached
        self.write(self.owner + player_offset, 0, 1)
        self.write(self.counter + 0x260, current, 4)
        self.write(self.counter + 0x264, current, 4)
        self.write(self.counter + 0x290, 0, 1)
        self.u.reg_write(UC_ARM64_REG_X19, self.owner)
        self.u.reg_write(UC_ARM64_REG_X20, self.counter)
        self.u.reg_write(saved_reg, current)
        self.u.reg_write(UC_ARM64_REG_SP, self.ram + 0x1f0000)
        guard = self.u.hook_add(UC_HOOK_CODE, lambda u,pc,n,d: u.emu_stop() if pc == self.base + end else None)
        self.u.emu_start(self.base + entry, self.base + end, count=500000)
        self.u.hook_del(guard)
        assert self.u.reg_read(UC_ARM64_REG_PC) == self.base + end
        assert self.read(self.counter + 0x290, 1) == (current != self.wallet)
        assert self.read(self.counter + 0x260, 4) == current
        if current != self.wallet:
            assert self.read(self.counter + 0x294, 4) == current
            assert self.read(self.counter + 0x298, 4) == self.wallet

for base in (0x7100000000, 0x7200000000):
    m = DisplayMachine(base)
    for player in range(8): m.deposit(123456789 + player, player)
    for shop in SHOPS:
        m.write(m.info + 0x38, shop, 4)
        for player in range(8):
            m.write(m.owner + 0x230, player, 1)
            m.update()
            assert m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
            assert m.texts[-1] == f'{123456789 + player:,}'
        m.native_refresh(0x2b13f10, 0x2b13ff4, 0x224, UC_ARM64_REG_X22)
        m.write(m.owner + 0x228, 0, 4)
        m.native_refresh(0x301b674, 0x301b7a0, 0x230, UC_ARM64_REG_X21)
    # A real cash change still queues the native animation; sound is not muted.
    m.native_refresh(0x2b13f10, 0x2b13ff4, 0x224, UC_ARM64_REG_X22, cached=m.wallet - 100)
    # Both cap and zero are visible in full; no stale amount after withdrawal.
    m.deposit(999999999)
    m.update(); assert m.texts[-1] == '999,999,999'
    m.route(0x257b340, 0xb168b4, m.player, 999999999)
    m.update(); assert m.texts[-1] == '0'
    for stage in (0, 0xde45e2c6):
        m.write(m.info + 0x38, stage, 4)
        m.update()
        assert not m.shown('N_ShoppingCard_00')
        assert all(m.shown(name) for name in ('P_IconBase_00', 'W_BaseL_00',
            'W_BaseStripe_00', 'N_RegionType_00', 'P_Icon_00', 'T_Num_00'))
    m.write(m.info + 0x38, SHOPS[0], 4)
    m.write(m.owner + 0x230, 255, 1); m.update()
    assert not m.shown('N_ShoppingCard_00')
    m.write(m.owner + 0x230, 0, 1); m.update()
    # Typed group adapter is resolved before interpreting a pane's parent.
    group = m.ram + 0x7000; node = m.ram + 0x7100
    m.write(m.methods + 0x5e0, base + 0x3c47b90)
    m.write(m.adapter + 8, group)
    m.write(group + 8, node); m.write(group + 0x10, 1, 4); m.write(group + 0x14, 8, 4)
    m.write(node, group)
    m.write(node + 8, m.panes['T_Num_00']); m.write(node + 0x10, m.methods + 0x600)
    m.write(m.methods + 0x600 + 0x5e0, base + 0x3c1b588)
    m.update(); assert m.shown('N_ShoppingCard_00')
    # Absent layout / unresolved adapter are harmless, and native update still runs.
    m.panes.pop('N_ShoppingCard_00')
    before = len(m.texts); calls = m.display_updates
    m.update(); assert len(m.texts) == before and m.display_updates == calls + 1
    m.write(m.adapter + 0x18, 0); m.update()
    m.write(m.owner + 0x210, 0); m.update()
print('PASS: original cash refresh branches queue no false animation; green savings display, all residents, cap/zero, room changes, typed adapters and missing resources at two bases')
