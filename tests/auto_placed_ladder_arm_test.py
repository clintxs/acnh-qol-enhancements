"""Compiled world-ladder policy with native hop guards and timer.

World objects/terrain are modeled; a separate fixture executes the native
placed-object selection branch and inspects its request. Live variants and
collision contacts still need confirmation in game.
"""
import struct
from auto_ladder_arm_test import LadderMachine, CONFIG, PLACED_LADDER
from auto_traversal_swap_arm_test import SwapMachine, EMPTY, NET, POLE, LADDER, KINDS
from auto_vault_arm_test import EXTRA
from core_arm_test import Machine, DEFAULT
from unicorn.arm64_const import *


class NativePlacedMachine(LadderMachine):
    def __init__(self, base):
        super().__init__(base)
        self.write(self.parameters + 0x2d0, self.vtable)
        self.world_tile = self.ram + 0x19000
        self.object_kind = 0x21
        self.object_direction = 1
        self.native_requests = []
        self.native_world = True

    def service(self, u, pc, size, context):
        if not getattr(self, 'native_world', False):
            return super().service(u, pc, size, context)
        result = 0
        if pc == self.base + PLACED_LADDER:
            return  # Real native object selection, not the higher-level stub.
        if pc == self.getter and self.reg(0) == self.parameters + 0x2d0:
            result = self.values
        elif pc == self.base + 0x2395be4:
            u.mem_write(self.reg(1), struct.pack('<fffI', 105., 15., 205., 0x20000000))
            result = 1
        elif pc == self.base + 0x15e1014:
            result = self.world_tile
        elif pc == self.base + 0x15e4d80:
            result = self.object_kind == 0x22
        elif pc == self.base + 0x15e4e3c:
            result = self.object_kind == 0x21
        elif pc == self.base + 0x114c030:
            u.mem_write(self.reg(0), bytes(24))
            self.write(self.reg(0) + 10, self.object_direction << 2, 1)
        elif pc == self.base + 0x113db50:
            result = self.object_kind
        elif pc == self.base + 0x15e49e4:
            u.mem_write(self.reg(0), b'\x11\x22\x33\x44\x55\x66\x77\x88')
        elif pc == self.base + 0x239630c:
            self.native_requests.append((bytes(u.mem_read(self.reg(1), 0x1c)), self.reg(2)))
            result = 1
        else:
            return super().service(u, pc, size, context)
        u.reg_write(UC_ARM64_REG_X0, result)
        u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))


def ready(m):
    m.placed_valid = m.placed_result = True
    m.tool = 0
    return m


def main():
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        m = ready(LadderMachine(base))
        assert 'auto placed ladder: READY; prototype 1;' in m.text
        for count in range(32):
            m.placed_calls.clear()
            assert m.automatic(0, prime=False) == (2 if count <= 30 else 1)
            assert m.placed_calls == ([1] if count <= 30 else [1, 0])
            assert not m.ladder_calls and not m.vault_calls
            if count == 31: break
            assert m.automatic(1, prime=False) == 2
            assert m.placed_calls == [1]  # No search at tentative movement position.
            m.tick(2)
            assert m.read(m.player + 0x6a0, 2) == count + 1
            assert m.automatic(1, prime=False) == 0  # Proof consumed once.
            cases += 1
        # All facing directions, held tools and auto-equipping disabled.
        for direction in (0, 0x40000000, 0x80000000, 0xc0000000):
            for tool in (0, 1, 0x11, 0x28):
                n = ready(LadderMachine(base))
                n.tool = tool
                n.direction = n.input_direction = direction
                n.write(n.actor + 0x2b4, direction, 4)
                n.write(n.player + 0x6a0, 31, 2)
                assert n.automatic(0, prime=False) == 1
                assert n.placed_calls == [1, 0] and not n.ladder_calls and not n.vault_calls
                cases += 1
        # World ladder takes priority over borrowing, with no equipment action.
        for item in (EMPTY, NET, POLE, LADDER):
            n = SwapMachine(base)
            n.held, n.tool = item, KINDS[item]
            n.inventory = []
            n.placed_valid = n.placed_result = True
            n.write(n.player + 0x6a0, 31, 2)
            assert n.automatic(0, prime=False) == 1
            assert n.placed_calls == [1, 0] and n.action_requests == []
            assert n.dispatch() == 0 and n.action_requests == []
            cases += 1
        # Original native guards and ordinary hop priority still apply.
        for change in ('release', 'weak', 'speed', 'contact', 'facing', 'input', 'native', 'bank', 'handle', 'tool'):
            n = ready(LadderMachine(base))
            assert n.automatic(0, prime=False) == 2
            n.placed_calls.clear()
            if change == 'release': n.stick(0., 0.)
            if change == 'weak': n.stick(0., .5)
            if change == 'speed': n.u.mem_write(n.actor+0x2e0, struct.pack('<f', 6.))
            if change == 'contact': n.contact = False
            if change == 'facing': n.direction = 0x40000000
            if change == 'input': n.input_direction = 0x40000000
            if change == 'native': n.hop_result = 1
            if change == 'bank': n.u.mem_write(n.actor+0x284, struct.pack('<fff', .01, 0., 0.))
            if change == 'handle': n.write(n.player+0x230, 7, 4)
            if change == 'tool': n.tool = 1
            assert n.automatic(1, prime=False) == (1 if change == 'native' else 0)
            assert n.placed_calls == []
            cases += 1
        # Validation witness and native result are both required; no dialogue-
        # only "success" can arm a climb. A refused commit stays refused.
        for valid, native in ((False, True), (True, False), (False, False)):
            n = ready(LadderMachine(base))
            n.placed_valid, n.placed_result = valid, native
            n.write(n.player+0x6a0, 31, 2)
            assert n.automatic(0, prime=False) == 0 and n.placed_calls == [1]
            cases += 1
        n = ready(LadderMachine(base))
        n.placed_commit = False
        n.write(n.player+0x6a0, 31, 2)
        assert n.automatic(0, prime=False) == 0 and n.placed_calls == [1, 0]
        cases += 1
        # Real wall-contact classification must also admit empty-handed
        # placed climbing, without weakening the native flag exclusions.
        for flags in (0x04, 0x80, 0x44, 0x100, 0):
            n = ready(LadderMachine(base))
            n.real_contacts = True
            collision = n.ram + 0x20000
            n.write(n.player + 0xa0, collision)
            n.write(collision + 0x2ef4, 1, 4)
            n.u.mem_write(collision + 0x2ef8 + 0x38, struct.pack('<fff', 0., 0., -1.))
            n.write(collision + 0x2ef8 + 0x4c, flags, 4)
            n.write(n.player + 0x6a0, 31, 2)
            assert n.automatic(0, prime=False) == (1 if flags in (0x04, 0x80, 0x100) else 0)
            assert n.placed_calls == ([1, 0] if flags in (0x04, 0x80, 0x100) else [])
            cases += 1
        # Real native object-selection branch: both recognized categories,
        # placement orientation, non-handheld byte and check-only propagation.
        for kind in (0x21, 0x22):
            for direction in (1, 3, 5, 7):
                for probe in (0, 1):
                    n = NativePlacedMachine(base)
                    n.object_kind, n.object_direction = kind, direction
                    n.u.reg_write(UC_ARM64_REG_X0, n.player)
                    n.u.reg_write(UC_ARM64_REG_X1, probe)
                    n.call(base + PLACED_LADDER)
                    assert n.reg(0) == 1 and len(n.native_requests) == 1
                    request, flags = n.native_requests[0]
                    assert flags == probe and request[0x10] == 1 and request[0x11] == 0
                    assert request[0x14:0x1c] == b'\x11\x22\x33\x44\x55\x66\x77\x88'
                    cases += 1
        # Independent fingerprint failure preserves already confirmed paths.
        for address in (PLACED_LADDER, 0x113db50, 0x15e49e4):
            n = Machine(base, {'controls': CONFIG})
            n.write(base + address, 0, 4)
            n.boot(); n.activate()
            assert 'auto placed ladder: DISABLED;' in n.text
            assert 'auto placed ladder: READY;' not in n.text
            assert 'auto ladder: READY;' in n.text and len(n.hooks) == 7 + EXTRA
            cases += 1
        n = Machine(base, {'controls': DEFAULT+b'hold_to_hoover=false\nhold_to_advance_dialogue=false\nauto_vault=true\nauto_ladder=false\nauto_equip_traversal=false\n'})
        n.boot(); n.activate()
        assert 'auto placed ladder: READY;' not in n.text
        cases += 1
    print(f'PASS: {cases} placed ladder policy, native object selection, timer and compatibility cases at two ASLR bases')


if __name__ == '__main__': main()
