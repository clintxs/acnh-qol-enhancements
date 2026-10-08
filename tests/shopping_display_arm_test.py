"""Cash refresh regression and savings presentation using the compiled UI hook."""
from shopping_card_arm_test import CardMachine, SHOPS
import struct
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
        self.hud = self.ram + 0x90000
        self.hud_page = self.ram + 0x91000
        self.hud_holder = self.ram + 0x92000
        self.hud_layout = self.ram + 0x93000
        self.parameter_calls = []
        self.write(self.hud + 0x210, self.hud_page)
        self.write(self.hud_page + 0x3f8, self.hud_holder)
        self.write(self.hud_holder, self.hud_layout)
        self.write(base + 0x4ab2020, self.ram + 0x94000)
        # Execute the ORIGINAL BellPage update as the trampoline target. This
        # catches wrong owner layouts that a stubbed auxiliary update missed.
        self.write(self.module.find('BellPageBalanceDisplay', 'OrigRef'), base + 0x2b13bf0)
        self.write(self.hud + 0x218, self.wallet, 4)
        self.write(self.hud + 0x21c, self.wallet, 4)
        self.write(self.counter + 0x260, self.wallet, 4)
        self.write(self.counter + 0x264, self.wallet, 4)
        self.display_updates = 0
        self.texts = []
        self.feedback = []; self.formatted = []
        # Execute the real native feedback callback through animation dispatch
        # and positional sound setup. Only engine formatting/playback is modeled.
        animation = self.ram + 0x95000
        self.write(self.counter + 0x1d0, animation)
        self.write(animation, animation + 0x100)
        self.write(animation + 8, animation + 0x300)
        self.write(animation + 0x18, animation + 0x200)
        self.position_query = animation + 0x400
        self.position_get = animation + 0x410
        self.write(animation + 0x120, self.position_query)
        self.write(animation + 0x4b0, self.position_get)
        self.write(base + 0x5639190, self.ram + 0x96000)
        self.write(base + 0x4ab3508, self.ram + 0x97000)
        for i in range(4):
            self.write(self.counter + 0x2a8 + i*8, i + 1)
            raw = self.read(base + 0x4a022a8 + i*8)
            self.write(base + 0x4a022a8 + i*8, base + raw)
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
        if self.services.get(pc) == ('catalog_engine', None):
            self.catalog_engine_calls.append((off, self.reg(0), self.reg(1))); self.finish(); return
        if off == 0x3025a4c:
            assert self.reg(0) == self.counter
            self.formatted.append(self.reg(1)); self.finish(); return
        if off == 0x3c66950:
            assert self.reg(0) == self.counter
            name = self.string(self.read(self.reg(1) + 8))
            self.feedback.append((name, self.reg(2))); self.finish(); return
        if pc == getattr(self, 'position_query', 0): self.finish(); return
        if pc == getattr(self, 'position_get', 0):
            u.mem_write(self.reg(0), struct.pack('<2f', 932, 416)); self.finish(); return
        if off in (0x3f033a0, 0x3f033b0): self.finish(); return
        if off == 0x3f04470:
            u.mem_write(self.reg(0), bytes(u.mem_read(self.reg(1), self.reg(2))))
            self.finish(self.reg(0)); return
        if off == 0x2b12c24:
            assert self.reg(0) == self.hud_layout + 0x38
            assert self.read(self.reg(1)) == self.ram + 0x94010
            assert self.string(self.read(self.reg(1) + 8)) == '/L_Bell_00'
            self.parameter_calls.append(self.u.reg_read(UC_ARM64_REG_LR))
            self.finish(self.counter); return
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
    def update_hud(self): self.invoke(self.callbacks[0x2b13bf0], self.hud)
    def settle_hud(self, expected):
        for _ in range(30): self.update_hud()
        assert self.texts[-1] == f'{expected:,}'
        assert not self.feedback or self.feedback[-1][0].endswith('Finish')
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

# This separate controller owns a Page at +210, not a counter. Exercise its
# actual native update from entry through return, with only engine lookup/text
# boundaries modeled. The auxiliary-only test above cannot establish HUD wiring.
for base in (0x7100000000, 0x7200000000):
    m = DisplayMachine(base)
    for player in range(8): m.deposit(123456 + player, player)
    for shop in SHOPS:
        m.write(m.info + 0x38, shop, 4)
        for player in range(8):
            m.write(m.hud + 0x224, player, 1)
            m.write(m.hud + 0x230, 7 - player, 1) # poison auxiliary player field
            m.update_hud()
            assert m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
            assert m.texts[-1] == f'{123456 + player:,}'
            assert m.read(m.counter + 0x290, 1) == 0
            assert m.read(m.hud + 0x218, 4) == m.wallet
        # Resident switches initialize silently; a subsequent purchase/sale
        # uses the native number step and Down/Up/Finish feedback callbacks.
        assert not m.feedback
        m.write(m.hud + 0x224, 0, 1)
        m.update_hud()
        assert not m.feedback
        # Route uses the explicit player context; select resident 0 for updates.
        m.write(m.player, 0, 1)
        m.route(0x257b340, 0xb168b4, m.player, 100)
        expected = m.balance()
        m.update_hud()
        assert int(m.texts[-1].replace(',', '')) > expected
        assert m.feedback[-1] == ('Down', 2)
        m.settle_hud(expected)
        assert m.feedback[-1] == ('DownFinish', 4)
        assert m.formatted[-1] == m.wallet # hidden cash text restored
        assert m.read(m.counter + 0x260, 4) == m.wallet
        assert m.read(m.counter + 0x290, 1) == 0
        m.deposit(200); expected = m.balance()
        m.update_hud()
        assert int(m.texts[-1].replace(',', '')) < expected
        assert m.feedback[-1] == ('Up', 1)
        m.settle_hud(expected)
        assert m.feedback[-1] == ('UpFinish', 3)
        # Pockets/repeated updates cannot replay a settled transaction.
        feedback = list(m.feedback)
        for _ in range(5): m.update_hud()
        m.update() # auxiliary cannot duplicate audio
        assert m.feedback == feedback
        m.feedback.clear()
        assert m.read(m.counter + 0x290, 1) == 0
        # Reset deposits before the next room's distinct-resident assertions.
        for player in range(8):
            m.write(m.player, player, 1)
            m.invoke(base + 0x257fb30, m.player, 999999999)
            m.deposit(123456 + player, player)
    assert base + 0x2b13c70 in m.parameter_calls
    assert 'SAVINGS_DISPLAY_READY source=HUD' in m.text
    m.write(m.info + 0x38, 0, 4); m.update_hud()
    assert not m.shown('N_ShoppingCard_00') and m.shown('T_Num_00')
    # Genuine pocket-money changes still use the original HUD amount animation.
    m.wallet += 100
    m.update_hud()
    assert m.read(m.counter + 0x290, 1) == 1
    assert m.read(m.counter + 0x298, 4) == m.wallet
    # A missing/unmounted HUD page is a no-op for native and custom paths.
    m.write(m.hud + 0x210, 0); m.update_hud()
print('PASS: actual BellPage HUD and native number stepping/Up/Down/Finish feedback at two bases, all shops/residents, silent initialization and unchanged balances, no auxiliary duplicate feedback, untouched native cash counter and restored cash text')

for base in (0x7100000000, 0x7200000000):
    m = DisplayMachine(base)
    m.deposit(999999999)
    m.update_hud()
    assert m.texts[-1] == '999,999,999' and not m.feedback
    # Full-range purchases remain bounded, including retargeting mid-animation.
    m.route(0x257b340, 0xb168b4, m.player, 999999999)
    m.update_hud()
    intermediate = int(m.texts[-1].replace(',', ''))
    assert 0 < intermediate < 999999999
    m.deposit(999999999)
    m.update_hud()
    assert intermediate < int(m.texts[-1].replace(',', '')) < 999999999
    m.settle_hud(999999999)
    m.route(0x257b340, 0xb168b4, m.player, 999999999)
    m.settle_hud(0)
    m.deposit(1); m.update_hud()
    assert m.texts[-1] == '1' and m.feedback[-1] == ('UpFinish', 3)
    before = list(m.feedback)
    # Reopening pockets/replacing the UI with the same resident cannot restart
    # a settled savings transition; no raw UI pointers are retained.
    m.update_hud(); m.update_hud()
    assert m.feedback == before
    m.write(m.info + 0x38, 0, 4); m.update_hud()
    m.deposit(100)
    m.write(m.info + 0x38, SHOPS[0], 4); m.update_hud()
    assert m.texts[-1] == '101' and m.feedback == before
print('PASS: full-cap to zero, mid-flight reversal, one-Bell completion, silent pocket refresh and silent shop re-entry at two bases')

# Catalog and BellPage can be created in DIFFERENT contexts at Nook Stop/on the
# phone. The 0.3.7 shared-context fixture passed while the live display failed.
# Execute the actual Catalog open/close routines and BellPage update; verify
# that only native lifecycle events control the resident-scoped presentation.
for base in (0x7100000000, 0x7200000000):
    m = DisplayMachine(base)
    catalog = m.ram + 0x161000; page = m.ram + 0x164000
    m.write(m.hud + 0x98, m.ram + 0x160000)
    m.write(catalog + 0x98, m.ram + 0x163000) # deliberately unrelated root list
    m.write(catalog + 0x210, page)
    m.write(catalog + 0x218, m.ram + 0x165000)
    m.write(catalog + 0x228, m.ram + 0x166000)
    m.write(catalog + 0x1f0, 0xffffffff, 4)
    m.write(catalog + 0x1f4, 0xffffffff, 4)
    m.write(m.module.find('CatalogOpened', 'OrigRef'), base + 0x2b70ffc)
    m.write(m.module.find('CatalogClosed', 'OrigRef'), base + 0x2b711b8)
    m.catalog_engine_calls = []
    m.services[base + 0x3769260] = ('catalog_engine', None)
    m.services[base + 0x3c5c660] = ('catalog_engine', None)
    for stage in (0, 0xde45e2c6):
        m.write(m.info + 0x38, stage, 4)
        for player in range(8):
            m.deposit(1234567 + player, player)
            m.write(m.hud + 0x224, player, 1)
            m.write(catalog + 0x1f8, player, 1)
            m.update_hud(); assert not m.shown('N_ShoppingCard_00')
            m.invoke(m.callbacks[0x2b70ffc], catalog)
            before = list(m.feedback)
            m.update_hud()
            assert m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
            assert m.texts[-1] == f'{m.balance(player):,}' and m.feedback == before
            m.route(0x257b340, 0x2b7f588, m.player, 1000)
            m.settle_hud(m.balance(player))
            assert m.feedback[-1] == ('DownFinish', 4)
            before = list(m.feedback)
            m.update_hud(); assert m.feedback == before
            assert m.read(m.counter + 0x260, 4) == m.wallet
            m.invoke(m.callbacks[0x2b711b8], catalog)
            m.update_hud()
            assert not m.shown('N_ShoppingCard_00') and m.shown('T_Num_00')
            # Cached-controller reopen is silent; direct/destructive teardown
            # also clears it even without the ordinary close notification.
            m.deposit(20, player)
            m.invoke(m.callbacks[0x2b70ffc], catalog); m.update_hud()
            assert m.texts[-1] == f'{m.balance(player):,}' and m.feedback == before
            if player % 2:
                m.invoke(base + 0x2b67074, catalog, caller=base + 0x2b67078)
            else:
                m.invoke(m.callbacks[0x2b66cf0], catalog)
            m.update_hud(); assert not m.shown('N_ShoppingCard_00')
            m.feedback.clear()
    assert len(m.lifecycle_events) == 16
    assert len(m.catalog_engine_calls) == 16 * 2 * 3 # two opens, three native calls
    # Closing one resident's catalog must not clear another resident's bit.
    for player in (0, 7):
        m.write(catalog + 0x1f8, player, 1); m.invoke(m.callbacks[0x2b70ffc], catalog)
    m.invoke(m.callbacks[0x2b711b8], catalog) # close resident 7
    m.write(m.hud + 0x224, 0, 1); m.update_hud()
    assert m.shown('N_ShoppingCard_00')
    m.write(m.hud + 0x224, 7, 1); m.update_hud()
    assert not m.shown('N_ShoppingCard_00')
    m.write(catalog + 0x1f8, 255, 1); m.invoke(m.callbacks[0x2b70ffc], catalog)
    m.update_hud(); assert not m.shown('N_ShoppingCard_00')
    # HUD cash refresh still compares pocket funds, preventing false ka-ching.
    m.native_refresh(0x2b13f10, 0x2b13ff4, 0x224, UC_ARM64_REG_X22)
print('PASS: native catalog lifecycle and BellPage with independent UI contexts, eight residents, order feedback, silent reopen, close/direct/deleting teardown, resident isolation and unchanged cash refresh at two bases')
