"""Execute native crafting/cooking updates through compiled C++/SIMD hooks.

Only the recipe/animation readiness query is modeled. The native animation-rate
setter runs against two track objects; execution stops on its return to the
state update. No game/save writes or live Eden execution.
"""
import itertools
import math
import struct
from core_arm_test import Machine, RAW, DEFAULT
from unicorn.arm64_const import *

HOOKS = (0x22fc6bc, 0x2279a14)
GUARDS = (0x22fc63c, 0x2279998, 0x22758a8, 0x22a6bec, 0x22b1830)
RAMP_FRAMES = 60
GP = [globals()[f'UC_ARM64_REG_X{i}'] for i in range(31)]
Q = [globals()[f'UC_ARM64_REG_Q{i}'] for i in range(32)]

def ramp(frame, maximum=3):
    return 1. + (maximum - 1.) * math.log1p(9. * min(frame, RAMP_FRAMES) / RAMP_FRAMES) / math.log(10.)

def rate_tolerance(maximum):
    # The scaled table adds float32 operations at peaks above 3. Allow a few
    # ULPs there while keeping the confirmed default's original tight bound.
    return 2e-7 if maximum <= 3 else maximum * 2e-7

class CraftingMachine(Machine):
    def __init__(self, base, files, maximum=6):
        self.maximum = maximum
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
        self.setter_return = None
        self.tracks = (self.ram + 0x31000, self.ram + 0x32000)
        self.native = True

    def service(self, u, pc, size, user):
        if not self.native:
            return super().service(u, pc, size, user)
        if pc == self.return_adapter:
            native_pc, sp, before_gp, before_q = self.pending
            assert u.reg_read(UC_ARM64_REG_SP) == sp
            u.reg_write(UC_ARM64_REG_LR, self.read(sp - 0x10))
            after_q = [u.reg_read(reg) for reg in Q]
            # Only S0's low 32 bits change; every other SIMD lane and all GP
            # registers (including the native mash counter in W8) survive.
            assert after_q[1:] == before_q[1:]
            assert after_q[0] >> 32 == before_q[0] >> 32
            rate = struct.unpack('<f', (after_q[0] & 0xffffffff).to_bytes(4, 'little'))[0]
            assert abs(rate - ramp(self.elapsed, self.maximum)) < rate_tolerance(self.maximum)
            after_gp = [u.reg_read(reg) for reg in GP]
            assert after_gp == before_gp
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
            self.setter_return = u.reg_read(UC_ARM64_REG_LR)
        elif pc == self.setter_return:
            # Execute the real setter, not a mocked boundary. No extra clamp
            # may appear between our callback and the animation tracks.
            encoded = struct.pack('<f', self.rate)
            assert bytes(u.mem_read(self.animation + 0xa8, 4)) == encoded
            if self.read(self.animation + 0xae, 1):
                assert all(bytes(u.mem_read(track + 0xa0, 4)) == encoded for track in self.tracks)
            u.emu_stop()
        elif pc == self.return_stop:
            u.emu_stop()

    def run(self, cooking=False, count=0, pressed=False, blocked=False, remote=False,
            ready=True, suspended=False, busy=False, playing=True, elapsed=RAMP_FRAMES):
        self.u.mem_write(self.obj, bytes(0x2000))
        self.u.mem_write(self.player, bytes(0x1800))
        self.u.mem_write(self.animation, bytes(0x100))
        track_list, track_slots = self.ram + 0x33000, self.ram + 0x34000
        self.write(self.animation + 0x78, track_list)
        self.write(track_list + 0x18, len(self.tracks), 4)
        self.write(track_list + 0x20, track_slots)
        for i, track in enumerate(self.tracks):
            self.write(track_slots + i * 16, track)
            self.u.mem_write(track + 0xa0, struct.pack('<f', -1))
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
        self.write(self.obj + 0x1c04, elapsed, 4)
        self.elapsed = elapsed
        initial_rate = struct.pack('<f', 1.25)
        self.u.mem_write(self.obj + speed, initial_rate)
        self.return_stop = self.ram + 0x100
        self.hits = self.recipe_checks = 0
        self.ready, self.rate = ready, None
        self.setter_return = None
        self.u.reg_write(UC_ARM64_REG_SP, self.stack)
        for i, reg in enumerate(GP): self.u.reg_write(reg, 0x12340000 + i)
        for i, reg in enumerate(Q): self.u.reg_write(reg, (0xabcdef01 << 96) + 0xabcdef0012345678 + i)
        self.u.reg_write(UC_ARM64_REG_X0, self.obj)
        self.u.reg_write(UC_ARM64_REG_LR, self.return_stop)
        # Enter the registered state's real update wrapper to include native
        # suspension/busy gates, then its real mash-A/rate calculation.
        start = 0x22b17ac if cooking else 0x22a6b68
        self.u.emu_start(self.base + start, 0, count=10000)
        assert self.pending is None and self.bypass is None
        assert self.read(self.obj + 0xed0, 4) == int(pressed)
        # Stop after the setter, before the wrapper's native increment.
        assert self.read(self.obj + 0x1c04, 4) == elapsed
        if suspended or busy or not playing:
            assert self.rate is None and not self.hits
            assert self.read(self.obj + counter, 4) == count
            assert bytes(self.u.mem_read(self.obj + speed, 4)) == initial_rate
            return
        native_count = count + int(pressed and not blocked and not remote)
        assert self.read(self.obj + counter, 4) == native_count
        assert self.hits == int(bool(self.callbacks) and not remote), (cooking, count, pressed, blocked, remote, ready, self.hits, self.callbacks)
        assert self.recipe_checks == int(not cooking and not remote)
        expected = 1.25 if remote else (ramp(elapsed, self.maximum) if self.callbacks else min(3., max(1., native_count)))
        if not cooking and not remote and not ready: expected = 1.
        assert abs(self.rate - expected) < rate_tolerance(self.maximum), (cooking, count, pressed, blocked, remote, ready, elapsed, self.rate, expected)
        assert bytes(self.u.mem_read(self.obj + speed, 4)) == struct.pack('<f', self.rate)

    def native_frame_step(self, offset, instructions):
        self.u.reg_write(UC_ARM64_REG_X19, self.obj)
        self.u.emu_start(self.base + offset, 0, count=instructions)
        return self.read(self.obj + 0x1c04, 4)

def check():
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        # Default true, regular false, custom false, custom re-enable, absent.
        configs = (
            ({'crafting': DEFAULT}, True),
            ({'crafting': DEFAULT.replace(b'true', b'false')}, False),
            ({'crafting': DEFAULT, 'crafting.custom': b'enabled=false'}, False),
            ({'crafting': DEFAULT.replace(b'true', b'false'), 'crafting.custom': b'enabled=true'}, True),
            ({'crafting.custom': b'enabled=true'}, False),
        )
        for files, enabled in configs:
            m = CraftingMachine(base, files)
            assert len(m.callbacks) == (2 if enabled else 0)
            for values in itertools.product((False, True), (0, 1, 2, 3, 8)):
                cooking, count = values
                for pressed, blocked, remote, ready, suspended, busy, playing in itertools.product((False, True), repeat=7):
                    m.run(cooking, count, pressed, blocked, remote, ready, suspended, busy, playing)
                    cases += 1
            # Every ramp sample, including saturation and repeated actions.
            # Native entry resets elapsed time; each wrapper increments it.
            for cooking in (False, True, False):
                m.write(m.obj + 0x1c04, 500, 4)
                assert m.native_frame_step(0x22758a8, 1) == 0
                previous, previous_gain = None, None
                for frame in range(RAMP_FRAMES + 6):
                    for pressed, ready in itertools.product((False, True), repeat=2):
                        m.run(cooking=cooking, pressed=pressed, ready=ready, elapsed=frame)
                        cases += 1
                    # Check increasing speed with diminishing gains, independently
                    # of controller input. The disabled module keeps manual A.
                    m.run(cooking=cooking, elapsed=frame)
                    if enabled:
                        if previous is not None:
                            gain = m.rate - previous
                            assert gain >= 0
                            if previous_gain is not None: assert gain <= previous_gain + rate_tolerance(m.maximum)
                            previous_gain = gain
                        previous = m.rate
                    offset = 0x22b1830 if cooking else 0x22a6bec
                    assert m.native_frame_step(offset, 3) == frame + 1
                for frame in (1000, 0xffffffff):
                    m.run(cooking=cooking, elapsed=frame)
                    cases += 1
            # Native state teardown still explicitly restores animation rate 1.
            for start in (0x22fcde0, 0x227a0e0):
                m.rate = None
                m.u.reg_write(UC_ARM64_REG_SP, m.stack)
                m.u.reg_write(UC_ARM64_REG_X0, m.obj)
                m.u.reg_write(UC_ARM64_REG_LR, m.return_stop)
                m.u.emu_start(base + start, 0, count=10000)
                assert m.rate == 1.
        # Actual regular/custom configuration reaches both native rate hooks.
        # Every supported peak reaches both native paths and the real setter.
        speed_configs = tuple(({'crafting': DEFAULT + f'crafting_speed={speed}'.encode()}, speed)
                              for speed in range(1, 31)) + (
            ({'crafting': DEFAULT, 'crafting.custom': b'crafting_speed=2'}, 2),
            ({'crafting': DEFAULT + b'crafting_speed=2', 'crafting.custom': b'crafting_speed=3'}, 3),
            ({'crafting': DEFAULT + b'crafting_speed=31'}, 30),
            ({'crafting': DEFAULT + b'crafting_speed=2', 'crafting.custom': b'crafting_speed=6'}, 6),
            ({'crafting': DEFAULT + b'crafting_speed=2', 'crafting.custom': b'crafting_speed=30'}, 30),
            ({'crafting': DEFAULT + b'crafting_speed=2', 'crafting.custom': b'crafting_speed=' + b'9' * 4000}, 30),
            ({'crafting': DEFAULT + b'crafting_speed=2', 'crafting.custom': b'crafting_speed=999999999999999999x'}, 2),
            ({'crafting': DEFAULT + b'crafting_speed=2', 'crafting.custom': b'crafting_speed=99\ncrafting_speed=1'}, 2),
            ({'crafting': DEFAULT + b'crafting_speed=2', 'crafting.custom': b'crafting_speed=1\ncrafting_speed=3'}, 2),
            ({'crafting': DEFAULT + b'crafting_speed=2', 'crafting.custom': b'crafting_speed=3\nversion=bad'}, 2),
        )
        for files, maximum in speed_configs:
            m=CraftingMachine(base,files,maximum)
            assert f'crafting_speed={maximum}' in m.text and 'BOOT_OK' in m.text
            for cooking in (False, True):
                for frame in range(RAMP_FRAMES+2):
                    m.run(cooking=cooking,elapsed=frame)
                    cases+=1
                m.run(cooking=cooking,elapsed=60,ready=False)
                m.run(cooking=cooking,elapsed=60,remote=True)
        # The retired marker cannot activate the renamed package.
        m=CraftingMachine(base,{'faster-crafting':DEFAULT,'faster-crafting.custom':b'enabled=true'})
        assert not m.callbacks and 'Crafting: not installed' in m.text
        assert not any('/faster-crafting' in name for name in m.opened)
        for offset in (*HOOKS, *GUARDS):
            m = Machine(base, {'crafting': DEFAULT, 'amiibo': DEFAULT})
            m.write(base + offset, 0xdeadbeef, 4)
            m.boot(); m.activate()
            assert 'Crafting] DISABLED' in m.text and 'Amiibo timeout: READY' in m.text
            assert not any(base + site in m.hooks for site in HOOKS)
    print(f'PASS: {cases} native crafting/cooking cases at two bases; logarithmic 60-frame 1..30x ramp, maximum clamping, real animation-track writes without downstream clamping, saturation/restarts, native frame reset/increments, readiness/suspension/local-player gates, native exit resets, manual A when disabled, isolated layered config, unchanged input/counters, SIMD/GP preservation, atomic fingerprints')

if __name__ == '__main__': check()
