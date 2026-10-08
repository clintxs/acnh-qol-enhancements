"""Compiled routing hooks plus original 3.0.3 savings read/write instructions.

Resident lookup, current-field type lookup, RNG, and original wallet services
are modeled. The native encrypted savings representation, cap, withdrawal,
shop sale branch and hash implementation execute directly from local main.
"""
import struct
from core_arm_test import Machine, RAW, DEFAULT
from unicorn.arm64_const import *
from unicorn import UC_HOOK_MEM_INVALID, UC_HOOK_CODE

WALLET_CALLS = (0xe7bf84, 0x2ad2fcc, 0x2ad41e4, 0x2ad5a54, 0x2ad5fe8,
                0x31192d8)
DEBIT_CALLS = (0xace110, 0xacf388, 0xadc1d0, 0xb168b4, 0x311d400)
SHOPS = (0xbe3cad72, 0x8a17f484, 0x91e12aa0)
HOOKS = (0x257abf0, 0x257b340, 0x257b6f0, 0xf86e68, 0x2579fe0, 0x301b394)

class CardMachine(Machine):
    def __init__(self, base, enabled=True):
        self.enabled = enabled; self.wallet = 77777; self.wallet_debits = []; self.lookups = []
        super().__init__(base, {'shopping-card': DEFAULT} if enabled else {})
        self.boot(); self.activate()
        self.u.hook_add(UC_HOOK_MEM_INVALID, lambda u,a,p,n,v,d: print("invalid", hex(u.reg_read(UC_ARM64_REG_PC)), hex(p)) or False)
        for offset in HOOKS: self.u.mem_write(base + offset, RAW[offset:offset + 20])
        self.callbacks = {offset: self.module.find(name, 'Callback') for offset, name in
                          zip(HOOKS, ('ShopWallet', 'ShopDebit', 'ShopCapacity', 'SaleMessage', 'ShopPurchaseSpace', 'ShopBalanceDisplay'))}
        for n, name in enumerate(('ShopWallet', 'ShopDebit', 'ShopCapacity', 'SaleMessage', 'ShopPurchaseSpace', 'ShopBalanceDisplay')):
            stub = self.ram + 0x9000 + n * 0x10
            self.services[stub] = ('card_original', name)
            self.write(self.module.find(name, 'OrigRef'), stub)
        self.player = self.ram + 0xa000
        self.manager = self.ram + 0xb000; self.types = self.ram + 0xc000
        self.info = self.ram + 0xd000; self.getter = base + 0x1000
        self.write(base + 0x4ab2720, self.manager - 8); self.write(self.manager - 8, self.manager)
        self.write(self.manager + 0x330, 1); self.write(self.manager + 0x338, 42, 4)
        self.write(base + 0x4ab2640, self.types - 8); self.write(self.types - 8, self.types)
        self.write(self.types, self.types + 0x100); self.write(self.types + 0x118, self.getter)
        self.write(base + 0x4ac3a80, self.info)
        self.write(self.info + 0x38, SHOPS[0], 4)
        # The native resolver hashes the type name; return its independently
        # checked indoor discriminator. Also execute the actual hash below.
        self.services[self.getter] = ('field', None)
        self.services[base + 0x5bd550] = ('type_hash', None)
        self.query = self.ram + 0xe000; self.event = self.ram + 0xf000
        self.write(self.query + 8, self.event)
        self.write(self.query + 0x100, self.query + 0x200)
        self.u.mem_write(self.query + 0x200, b'cShop\0')
        for player in range(8):
            resident = self.resident(player); view = resident + 0x194b0
            self.write(view + 8, 1)
            self.write(view + 0x8620, 0); self.write(view + 0x8628, self.money(player))
        self.write(base + 0x54021a0, self.ram + 0x8000)
    def resident(self, player): return self.ram + 0x10000 + player * 0x28000
    def money(self, player): return self.ram + 0x180000 + player * 0x100
    def finish(self, result=0):
        self.u.reg_write(UC_ARM64_REG_X0, result)
        self.u.reg_write(UC_ARM64_REG_PC, self.u.reg_read(UC_ARM64_REG_LR))
    def service(self, u, pc, size, data):
        off = pc - self.base; service = self.services.get(pc)
        if off == 0x257abf0:
            if self.enabled: u.reg_write(UC_ARM64_REG_PC, self.callbacks[off])
            else: self.finish(self.wallet)
            return
        if off in (0x24830c0, 0x2483354):
            player = self.read(self.reg(1), 1); self.lookups.append(player)
            self.finish(self.resident(player) if player < 8 else 0); return
        if off == 0x3f030c0: self.finish(0x12345678); return # native money encryption seed
        if service == ('field', None):
            assert self.reg(0) == self.types and self.reg(1) == 42
            self.write(self.types + 0x200, self.types + 0x300)
            self.finish(self.types + 0x200); return
        if service == ('type_hash', None): self.finish(0x758f7eaf); return
        if service and service[0] == 'card_original':
            name = service[1]
            if name == 'ShopWallet': self.finish(self.wallet)
            elif name == 'ShopDebit': self.wallet_debits.append((self.read(self.reg(0), 1), self.reg(1))); self.finish()
            elif name == 'ShopBalanceDisplay': self.display_updates += 1; self.finish()
            elif name == 'ShopCapacity': self.finish(1234567)
            elif name == 'ShopPurchaseSpace': self.space_args = (self.reg(1), self.reg(2)); self.finish(3)
            else: self.finish(1)
            return
        super().service(u, pc, size, data)
    def invoke(self, address, *arguments, caller=None):
        for i, value in enumerate(arguments): self.u.reg_write(UC_ARM64_REG_X0 + i, value)
        sp = self.ram + 0x1f0000
        stop = caller if caller is not None else self.ram + 0x9100
        self.u.reg_write(UC_ARM64_REG_SP, sp); self.u.reg_write(UC_ARM64_REG_LR, stop)
        self.stop = stop
        guard = self.u.hook_add(UC_HOOK_CODE, lambda u,pc,n,d: u.emu_stop() if pc == self.stop else None)
        self.u.emu_start(address, stop, count=500000)
        self.u.hook_del(guard)
        assert self.u.reg_read(UC_ARM64_REG_PC) == stop
        assert self.u.reg_read(UC_ARM64_REG_SP) == sp
        return self.reg(0)
    def balance(self, player=0):
        self.write(self.player, player, 1)
        return self.invoke(self.base + 0x257f8b0, self.player)
    def deposit(self, amount, player=0):
        self.write(self.player, player, 1)
        self.invoke(self.base + 0x257f9f0, self.player, amount)
    def route(self, offset, callsite, *args):
        return self.invoke(self.callbacks[offset], *args, caller=self.base + callsite + 4)

for base in (0x7100000000, 0x7200000000):
    m = CardMachine(base)
    for player in range(8):
        m.deposit(500000 + player, player)
        assert m.balance(player) == 500000 + player
    for shop in SHOPS:
        m.write(m.info + 0x38, shop, 4)
        for player in range(8):
            m.write(m.player, player, 1)
            before = m.balance(player)
            for site in WALLET_CALLS:
                assert m.route(0x257abf0, site, m.player) == before
            for site in DEBIT_CALLS:
                m.route(0x257b340, site, m.player, 100)
                before -= 100
                assert m.balance(player) == before
            assert not m.wallet_debits and m.wallet == 77777
            for site in (0xf9255c, 0x31143fc):
                assert m.route(0x2579fe0, site, m.player, 99000, 2) == 3
                assert m.space_args == (0, 2)
            m.write(m.event + 0x2a8, player, 1)
            if shop != SHOPS[2]:
                assert m.route(0x257b6f0, 0xb1d4d4, m.player, 0) == 0
                assert m.invoke(m.callbacks[0xf86e68], m.query) == 0
                # Execute the ACTUAL ShopSellDone capacity branch and native
                # deposit with the compiled callback's result. Stop before records.
                before = m.balance(player)
                m.u.reg_write(UC_ARM64_REG_X19, m.player)
                m.u.reg_write(UC_ARM64_REG_X22, 23456)
                m.u.reg_write(UC_ARM64_REG_X0, 0)
                m.u.reg_write(UC_ARM64_REG_SP, m.ram + 0x1f0000)
                m.u.emu_start(base + 0xb1d4d8, base + 0xb1d4fc, count=500000)
                assert m.balance(player) == before + 23456
            else:
                assert m.route(0x257b6f0, 0xb1d4d4, m.player, 0) == 1234567
                assert m.invoke(m.callbacks[0xf86e68], m.query) == 1
    # ABD deposit/withdraw, pocket Bells, other callers IN shops, and unrelated
    # rooms never borrow the card path.
    for site in (0x2e72a1c, 0x2e72ef4, 0x307d91c, 0xc33b74, 0x2581728,
                 0x2b13f18, 0x2b13f28, 0x301af0c, 0x301b67c, 0x301b6c0):
        assert m.route(0x257abf0, site, m.player) == m.wallet
    m.route(0x257b340, 0x2e72a28, m.player, 100)
    assert m.wallet_debits == [(7, 100)]
    for stage in (0, 0x6622bd46, 0xde45e2c6, 0x3ee08254):
        m.write(m.info + 0x38, stage, 4)
        assert m.route(0x257abf0, WALLET_CALLS[0], m.player) == m.wallet
        m.route(0x2579fe0, 0x31143fc, m.player, 99000, 2)
        assert m.space_args == (99000, 2)
        m.route(0x257b340, DEBIT_CALLS[0], m.player, 10)
        assert m.route(0x257b6f0, 0xb1d4d4, m.player, 0) == 1234567
        assert m.invoke(m.callbacks[0xf86e68], m.query) == 1
    # Room lookup failure, invalid player and alternate capacity modes fail closed.
    m.write(m.info + 0x38, SHOPS[0], 4)
    m.u.mem_write(m.query + 0x200, b'cMoneyBox\0')
    assert m.invoke(m.callbacks[0xf86e68], m.query) == 1
    m.write(m.player, 255, 1)
    assert m.route(0x257abf0, WALLET_CALLS[0], m.player) == m.wallet
    m.write(m.player, 0, 1); m.write(m.manager + 0x330, 0)
    assert m.route(0x257abf0, WALLET_CALLS[0], m.player) == m.wallet
    m.write(m.manager + 0x330, 1)
    assert m.invoke(m.module.find('shopping', 'AvailableBells'), m.player) == m.balance()
    # Native savings cap and empty account, without pocket-money fallback.
    m.deposit(999999999)
    assert m.balance() == 999999999
    m.deposit(100); assert m.balance() == 999999999
    m.route(0x257b340, DEBIT_CALLS[0], m.player, 999999999)
    assert m.balance() == 0 and m.route(0x257abf0, WALLET_CALLS[0], m.player) == 0
    # Prove room constants with the native hash body, not the source comments.
    del m.services[base + 0x5bd550]
    for name, expected in zip(('IdrMarket01', 'IdrMarket02', 'IdrTailor'), SHOPS):
        m.u.mem_write(m.query + 0x200, name.encode() + b'\0')
        assert m.invoke(base + 0x5bd550, m.query + 0x200, 0) == expected
    # Feature absent: Fitting Room's shared accessor uses the original wallet.
    off = CardMachine(base, False)
    assert off.invoke(off.module.find('shopping', 'AvailableBells'), off.player) == off.wallet
    # Every fingerprint must fail before any feature hook is installed.
    import re
    from pathlib import Path
    checks = Path(__file__).resolve().parents[1] / 'source/program/shopping/fingerprints.inc'
    for offset in re.findall(r'^\{(0x[0-9a-f]+),', checks.read_text(), re.M):
        bad = Machine(base, {'shopping-card': DEFAULT})
        bad.write(base + int(offset, 16), 0xdeadbeef, 4)
        bad.boot(); bad.activate()
        assert len(bad.hooks) == 1 and 'Shopping Card] DISABLED' in bad.text
print('PASS: Shopping Card native savings, encrypted writes/cap, all 8 players, exact caller/room scope, native sale branch with ordinary success dialogue, fitting accessor, removal and all fingerprints at two ASLR bases')
