"""Execute native crafting/cooking updates through compiled C++/SIMD hooks.

Only the recipe/animation readiness query is modeled; execution stops at the
native animation-rate setter. No game/save writes or live Eden execution.
"""
import itertools
import struct
from core_arm_test import Machine, RAW, DEFAULT
from unicorn.arm64_const import *

HOOKS = (0x22fc694, 0x22799fc)
GP = [globals()[f'UC_ARM64_REG_X{i}'] for i in range(31)]
Q = [globals()[f'UC_ARM64_REG_Q{i}'] for i in range(32)]

class CraftingMachine(Machine):
    def __init__(self, base, files):
        self.native = False
        super().__init__(base, files)
        self.boot(); self.activate()
        self.callbacks = {base + offset: self.hooks[base + offset]
                          for offset in HOOKS if base + offset in self.hooks}
        for offset in HOOKS:
            self.u.mem_write(base + offset, RAW[offset:offset + 20])
        self.adapter = self.module.find('exl_inline_float_hook_impl')
        self.obj, self.player, self.animation = self.ram + 0x10000, self.ram + 0x20000, self.ram + 0x30000
        self.stack, self.return_adapter = self.ram + 0x80000, self.ram + 0x400
        self.pending, self.bypass = None, None
        self.native = True

    def service(self, u, pc, size, user):
        if not self.native:
            return super().service(u, pc, size, user)
        if pc == self.return_adapter:
            native_pc, sp, before_gp, before_q = self.pending
            assert u.reg_read(UC_ARM64_REG_SP) == sp
            u.reg_write(UC_ARM64_REG_LR, self.read(sp - 0x10))
            assert [u.reg_read(reg) for reg in Q] == before_q
            after_gp = [u.reg_read(reg) for reg in GP]
            assert after_gp[8] == 3
            assert all(a == b for i, (a, b) in enumerate(zip(before_gp, after_gp)) if i != 8)
            self.pending, self.bypass = None, native_pc
            u.reg_write(UC_ARM64_REG_PC, native_pc)
            return
        if pc in self.callbacks:
            if self.bypass == pc:
                self.bypass = None
            else:
                assert self.pending is None
                sp = u.reg_read(UC_ARM64_REG_SP)
                self.pending = (pc, sp, [u.reg_read(reg) for reg in GP], [u.reg_read(reg) for reg in Q])
                self.hits += 1
                # Match real inline_impl.cpp's saved LR / callback ABI, then
                # execute pristine displaced instructions after the adapter.
                self.write(sp - 0x10, u.reg_read(UC_ARM64_REG_LR))
                self.write(self.return_adapter + 8, self.callbacks[pc])
                u.reg_write(UC_ARM64_REG_LR, self.return_adapter)
                u.reg_write(UC_ARM64_REG_PC, self.adapter)
                return
        relative = pc - self.base
        if relative == 0x264bce0:
            self.recipe_checks += 1
            u.reg_write(UC_ARM64_REG_X0, int(self.ready))
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
        elif relative == 0x3750760:
            assert self.reg(0) == self.animation
            self.rate = struct.unpack('<f', u.reg_read(UC_ARM64_REG_S0).to_bytes(4, 'little'))[0]
            u.emu_stop()
        elif pc == self.return_stop:
            u.emu_stop()

    def run(self, cooking=False, count=0, pressed=False, blocked=False, remote=False,
            ready=True, suspended=False, busy=False, playing=True):
        self.u.mem_write(self.obj, bytes(0x2000))
        self.u.mem_write(self.player, bytes(0x1800))
        self.u.mem_write(self.animation, bytes(0x100))
        counter, speed = (0xdb8, 0x1920) if cooking else (0xc08, 0x1928)
        self.write(self.obj + 0x60, self.player)
        self.write(self.player + 0x1138, self.animation)
        self.write(self.obj + 0x178, int(remote), 4)
        self.write(self.obj + 0x17ac, int(blocked), 1)
        self.write(self.obj + 0x17cf, int(suspended), 1)
        self.write(self.animation + 0xae, int(playing), 1)
        self.write(self.animation + 0xb4, int(busy), 1)
        self.write(self.obj + 0xed0, int(pressed), 4)
        self.write(self.obj + counter, count, 4)
        initial_rate = struct.pack('<f', 1.25)
        self.u.mem_write(self.obj + speed, initial_rate)
        self.return_stop = self.ram + 0x100
        self.hits = self.recipe_checks = 0
        self.ready, self.rate = ready, None
        self.u.reg_write(UC_ARM64_REG_SP, self.stack)
        for i, reg in enumerate(GP): self.u.reg_write(reg, 0x12340000 + i)
        for i, reg in enumerate(Q): self.u.reg_write(reg, 0xabcdef0012345678 + i)
        self.u.reg_write(UC_ARM64_REG_X0, self.obj)
        self.u.reg_write(UC_ARM64_REG_LR, self.return_stop)
        # Enter the registered state's real update wrapper to include native
        # suspension/busy gates, then its real mash-A/rate calculation.
        start = 0x22b17ac if cooking else 0x22a6b68
        self.u.emu_start(self.base + start, 0, count=10000)
        assert self.pending is None and self.bypass is None
        assert self.read(self.obj + 0xed0, 4) == int(pressed)
        if suspended or busy or not playing:
            assert self.rate is None and not self.hits
            assert self.read(self.obj + counter, 4) == count
            assert bytes(self.u.mem_read(self.obj + speed, 4)) == initial_rate
            return
        native_count = count + int(pressed and not blocked and not remote)
        assert self.read(self.obj + counter, 4) == native_count
        assert self.hits == int(bool(self.callbacks) and not remote), (cooking, count, pressed, blocked, remote, ready, self.hits, self.callbacks)
        assert self.recipe_checks == int(not cooking and not remote)
        expected = 1.25 if remote else (3. if self.callbacks else min(3., max(1., native_count)))
        if not cooking and not remote and not ready: expected = 1.
        assert self.rate == expected, (cooking, count, pressed, blocked, remote, ready, self.rate, expected)
        assert bytes(self.u.mem_read(self.obj + speed, 4)) == struct.pack('<f', expected)

def check():
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        # Default true, regular false, custom false, custom re-enable, absent.
        configs = (
            ({'faster-crafting': DEFAULT}, True),
            ({'faster-crafting': DEFAULT.replace(b'true', b'false')}, False),
            ({'faster-crafting': DEFAULT, 'faster-crafting.custom': b'enabled=false'}, False),
            ({'faster-crafting': DEFAULT.replace(b'true', b'false'), 'faster-crafting.custom': b'enabled=true'}, True),
            ({'faster-crafting.custom': b'enabled=true'}, False),
        )
        for files, enabled in configs:
            m = CraftingMachine(base, files)
            assert len(m.callbacks) == (2 if enabled else 0)
            for values in itertools.product((False, True), (0, 1, 2, 3, 8)):
                cooking, count = values
                for pressed, blocked, remote, ready, suspended, busy, playing in itertools.product((False, True), repeat=7):
                    m.run(cooking, count, pressed, blocked, remote, ready, suspended, busy, playing)
                    cases += 1
            # Native state teardown still explicitly restores animation rate 1.
            for start in (0x22fcde0, 0x227a0e0):
                m.rate = None
                m.u.reg_write(UC_ARM64_REG_SP, m.stack)
                m.u.reg_write(UC_ARM64_REG_X0, m.obj)
                m.u.reg_write(UC_ARM64_REG_LR, m.return_stop)
                m.u.emu_start(base + start, 0, count=10000)
                assert m.rate == 1.
        for offset in (*HOOKS, 0x22fc63c, 0x2279998):
            m = Machine(base, {'faster-crafting': DEFAULT, 'amiibo': DEFAULT})
            m.write(base + offset, 0xdeadbeef, 4)
            m.boot(); m.activate()
            assert 'Faster Crafting] DISABLED' in m.text and 'Amiibo timeout: READY' in m.text
            assert not any(base + site in m.hooks for site in HOOKS)
    print(f'PASS: {cases} native crafting/cooking cases at two bases; native 3x cap, readiness/suspension/local-player gates, native exit resets, manual A when disabled, isolated layered config, unchanged input/counters, SIMD/GP preservation, atomic fingerprints')

if __name__ == '__main__': check()
