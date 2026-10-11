"""Run compiled Controls callbacks at two ASLR bases.

Native item-ID/field-kind decoding and the original physical-Y mapping fragment
execute. Picker scheduling/reservation, loaded parameter tables and animations
are modeled; this does not establish Eden's live scheduler or save behavior.
"""
import struct
from core_arm_test import Machine, DEFAULT, RAW
from unicorn.arm64_const import *

Y = 1 << 38

class HooverMachine(Machine):
    def __init__(self, base):
        self.callbacks = {}
        super().__init__(base, {'controls': DEFAULT + b'hold_to_advance_dialogue=false\n'})
        self.boot(); self.activate()
        assert 'Controls: ENABLED' in self.text and len(self.hooks) == 7, self.text
        self.next_stub = self.ram + 0x180000
        self.callbacks = {}
        self.events = []
        self.target = self.actual = 100
        self.reserve_ok = True
        self.player = self.ram + 0x1000
        self.other_player = self.ram + 0x4000
        self.source = self.ram + 0x8000
        self.context = self.ram + 0x9000
        for p, resident in ((self.player, 0), (self.other_player, 1)):
            self.write(p + 0x1ee8, resident, 1)
            self.write(p + 0x230, resident + 10, 4)
            self.write(p + 0xec0 + 0x48, p)
        self.original('HooverInput', lambda: 77)
        self.original('HooverPickup', self.pick)
        self.original('HooverReservation', self.reserve)
        self.original('HooverFurniture', lambda: self.events.append('furniture') or 1)
        # Real native FieldKind computes the row index using this BCSV header,
        # then consults the runtime enum array, as the game does after loading.
        header = self.ram + 0x10000
        self.write(header, 47, 4); self.write(header + 4, 32, 4)
        self.write(header + 8, 1, 2)
        enums = self.ram + 0x12000
        self.write(base + 0x50503b8, 47, 4); self.write(base + 0x50503c0, enums)
        rows = self.ram + 0x20000
        for kind in range(47):
            row = header + 20 + kind * 32
            self.write(row, row - header, 4)
            self.write(enums + kind * 2, kind, 2)
            self.write(rows + (1000 + kind) * 8, row)
        for got, table, pointers in ((0x4abbc68, self.ram + 0x13000, rows),
                                     (0x4ac0428, self.ram + 0x14000, self.ram + 0x30000)):
            slot = table + 0x300
            self.write(base + got, slot); self.write(slot, table)
            self.write(table + 0x1d8, 1100, 4); self.write(table + 0x1e0, pointers)
        self.write(self.ram + 0x30000 + 100 * 8, self.ram + 0x15000)
        self.write(self.ram + 0x30000 + 101 * 8, self.ram + 0x15020)
        # Restoring native helpers is unnecessary: installation doesn't hook them.
    def original(self, name, callback):
        stub = self.next_stub; self.next_stub += 16
        self.callbacks[stub] = callback
        self.write(self.module.find(name, 'OrigRef'), stub)
    def service(self, u, pc, size, context):
        if pc not in self.callbacks:
            return super().service(u, pc, size, context)
        result = self.callbacks[pc]()
        if result is None: return  # Callback redirected execution to reservation.
        u.reg_write(UC_ARM64_REG_X0, result)
        u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
    def pick(self):
        if self.reg(1):
            assert self.reg(1) == 1 and self.reg(4) == 1
            self.events.append('probe')
            if self.target is None: return 0
            self.write(self.reg(2), self.target)
            return 1
        self.events.append('pick')
        if self.actual is None: return 0
        self.write(self.source, self.actual)
        for i, value in enumerate((self.ram + 0xa000, self.ram + 0xb000, self.source,
                                   self.source, self.ram + 0xc000, self.ram + 0xd000)):
            self.u.reg_write(UC_ARM64_REG_X0 + i, value)
        self.u.reg_write(UC_ARM64_REG_PC, self.module.find('HooverReservation', 'Callback'))
        return None
    def reserve(self):
        self.events.append(('reserve', self.read(self.reg(2))))
        return int(self.reserve_ok)
    def invoke(self, name, *args):
        for i, value in enumerate(args): self.u.reg_write(UC_ARM64_REG_X0 + i, value)
        self.call(self.module.find(name, 'Callback'))
        return self.reg(0)
    def input(self, held=True, pressed=False, player=None):
        p = player or self.player
        # Physical masks are prepared before native context-action mapping.
        self.write(p + 0xec8, Y if held else 0)
        self.write(p + 0xed0, Y if pressed else 0)
        assert self.invoke('HooverInput', p + 0xec0, 0) == 77
        self.write(p + 0xed0, (Y | 8) if pressed else 0)
    def gate(self, fresh=False, player=None):
        self.u.mem_write(self.context, b'\0' * 0x100)
        self.write(self.context + 19 * 8, player or self.player)
        self.write(self.context + 8 * 8, int(fresh))
        self.invoke('HooverPressGate', self.context)
        return self.read(self.context + 8 * 8, 4)
    def frame(self, target, fresh=False, held=True, actual=None, player=None):
        self.events.clear(); self.target = target
        self.actual = target if actual is None else actual
        p = player or self.player
        self.input(held, fresh, p)
        gate = self.gate(fresh, p)
        if gate & 1: self.invoke('HooverPickup', p, 0, 0, 0, 0)
        return list(self.events)

cases = 0
for base in (0x7100000000, 0x7200000000):
    m = HooverMachine(base)
    # Execute original physical input mapping (controller bit 4 -> action 38)
    # with its actual register/stack ABI, independently of our fixture masks.
    controller = m.ram + 0x16000; held_mask = m.ram + 0x17000
    pressed_mask = held_mask + 8
    for held, pressed in ((False, False), (True, True), (True, False)):
        m.write(controller + 8, 0x10 if held else 0, 4)
        m.write(controller + 0x8c, 0x10 if pressed else 0, 4)
        m.write(held_mask, 0); m.write(pressed_mask, 0)
        m.u.reg_write(UC_ARM64_REG_SP, m.ram + 0x1f0000)
        for reg, value in ((9, controller), (10, 38), (11, 0), (25, pressed_mask), (28, held_mask)):
            m.u.reg_write(UC_ARM64_REG_X0 + reg, value)
        m.u.emu_start(base + 0x2462860, base + 0x24629d0, count=500)
        assert m.u.reg_read(UC_ARM64_REG_PC) == base + 0x24629d0
        assert m.read(held_mask) == (Y if held else 0)
        assert m.read(pressed_mask) == (Y if pressed else 0)
        cases += 1
    # All nine category transitions, all six crop species, weed, normal items.
    for initial in (100, 1018, 1031):
        assert m.frame(initial, fresh=True) == ['pick', ('reserve', initial)]
        for target in (100, 101, *range(1018, 1024), 1031, 1011, 1033, 1034, 999, None):
            events = m.frame(target)
            same = ((initial == 100 and target in (100, 101)) or
                    (initial == 1018 and target is not None and 1018 <= target <= 1023) or
                    (initial == 1031 and target == 1031))
            assert events == (['probe', 'pick', ('reserve', target)] if same else ['probe']), (initial, target, events)
            cases += 1
        assert m.frame(initial, held=False) == []
        assert m.frame(initial) == []  # Holding without a fresh press cannot arm.
        cases += 2
    # A fresh empty/unsupported/failed action must not latch on a later target.
    for initial in (None, 1011, 1033, 1034, 999):
        m.frame(initial, fresh=True)
        assert m.frame(100) == []
        cases += 1
    m.reserve_ok = False
    m.frame(100, fresh=True)
    assert m.frame(100) == []
    m.reserve_ok = True
    # Final guard rejects a different source even after a matching preflight.
    m.frame(100, fresh=True)
    assert m.frame(100, actual=1031) == ['probe', 'pick']
    assert m.frame(100) == ['probe', 'pick', ('reserve', 100)]
    # A blocked repeat delegates refusal and keeps its original category.
    m.reserve_ok = False
    assert m.frame(100) == ['probe', 'pick', ('reserve', 100)]
    assert m.frame(1018) == ['probe']
    m.reserve_ok = True
    assert m.frame(100) == ['probe', 'pick', ('reserve', 100)]
    cases += 3
    # Release/repress during animation: each input update clears the old action.
    m.input(False); m.input(True, True)
    m.input(True, False)
    assert m.gate() == 0
    assert m.frame(1018, fresh=True) == ['pick', ('reserve', 1018)]
    assert m.frame(100) == ['probe']
    # Residents and reused player addresses cannot inherit another hold.
    assert m.frame(100, player=m.other_player) == []
    m.frame(100, fresh=True, player=m.other_player)
    assert m.frame(1018) == ['probe', 'pick', ('reserve', 1018)]
    m.write(m.player + 0x230, 999, 4)
    assert m.frame(1018) == []
    # Unscoped reservations/furniture calls remain native.
    m.events.clear(); m.write(m.source, 1033)
    assert m.invoke('HooverReservation', 0, 0, m.source, m.source, 0, 0) == 1
    assert m.invoke('HooverFurniture', m.player, 0, 0, 0, 0) == 1
    assert m.events == [('reserve', 1033), 'furniture']
    cases += 12
# Installation is atomic: every fingerprint is checked before the first hook.
for offset in (0x2461f30, 0x2222a34, 0x2326e2c, 0x21baf28, 0x23282d0, 0x267d6c4, 0x113db50):
    m = Machine(0x7100000000, {'controls': DEFAULT + b'hold_to_advance_dialogue=false\n'})
    m.write(m.base + offset, 0xdeadbeef, 4)
    m.boot(); m.activate()
    assert len(m.hooks) == 2 and 'Controls] hold Y: DISABLED' in m.text
    cases += 1
print(f'PASS: {cases} Controls cases; compiled lock/release/probe/commit guards, native Y and item classification, two ASLR bases; live Eden pending')
