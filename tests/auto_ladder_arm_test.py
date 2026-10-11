"""Compiled ladder extension, native hop guards/timer and shared pole regression.

Ladder terrain and reservation are modeled; the user confirmed prototype 2
working uphill and downhill. The valid-landing observer is exercised with real inline contexts.
"""
import struct
from auto_vault_arm_test import VaultMachine, DIAGNOSTIC, EXTRA
from core_arm_test import Machine, DEFAULT, RAW
from unicorn.arm64_const import *

LADDER = 0x2396154
LANDING = 0x2396bfc
CONFIG = DEFAULT + b'hold_to_hoover=false\nhold_to_advance_dialogue=false\nauto_vault=true\nauto_ladder=true\n'

class LadderMachine(VaultMachine):
    def __init__(self, base, config=CONFIG, expected_hooks=None):
        super().__init__(base, config=config, expected_hooks=7 + EXTRA if expected_hooks is None else expected_hooks)
        self.contact_original = self.ram + 0xd0a0
        self.write(self.module.find('AutoLadderContact', 'OrigRef'), self.contact_original)
        self.u.mem_write(base + 0x15f3040, RAW[0x15f3040:0x15f3040+20])
        self.contact_classes = []
        self.wall_contact = self.real_contacts = self.run_contact_body = False
        self.tool = 0x11
        self.ladder_calls = []
        self.ladder_valid = self.ladder_result = self.ladder_commit = True
        self.inline_ctx = self.ram + 0x14000
        self.ladder_return = self.ram + 0xd080
        self.u.mem_write(self.inline_ctx, bytes([0x5a])*0x100)
        self.write(self.inline_ctx + 19*8, self.player)

    def service(self, u, pc, size, context):
        if hasattr(self, 'contact_original'):
            if pc == self.base + 0x15f3040:
                if self.run_contact_body:
                    self.run_contact_body = False
                    return  # Execute real native contact classification.
                u.reg_write(UC_ARM64_REG_PC, self.module.find('AutoLadderContact', 'Callback'))
                return
            if pc == self.contact_original:
                classes = self.read(self.reg(1) + 0x20, 4)
                self.contact_classes.append(classes)
                if self.real_contacts:
                    self.run_contact_body = True
                    u.reg_write(UC_ARM64_REG_PC, self.base + 0x15f3040)
                    return
                u.mem_write(self.reg(1), struct.pack('<fff', 0., 0., -1.))
                u.reg_write(UC_ARM64_REG_X0, int(self.contact and (not self.wall_contact or bool(classes & 1))))
                u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
                return
        if getattr(self, 'native_landing_test', False):
            if pc == self.base + LANDING:
                self.reached_landing = True
                return  # Run the original probe-return branch.
            if pc in (self.base + 0x15e0e30, self.base + 0x15ea82c):
                u.reg_write(UC_ARM64_REG_X0, int(self.terrain_allowed))
                u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
                return
        if hasattr(self, 'ladder_calls') and pc == self.base + LADDER:
            assert self.reg(0) == self.player and self.reg(1) in (0, 1)
            probe = self.reg(1)
            self.ladder_calls.append(probe)
            if probe and self.ladder_valid:
                self.ladder_outer_return = u.reg_read(UC_ARM64_REG_LR)
                u.reg_write(UC_ARM64_REG_X0, self.inline_ctx)
                u.reg_write(UC_ARM64_REG_LR, self.ladder_return)
                u.reg_write(UC_ARM64_REG_PC, self.module.find('AutoLadderLanding', 'Callback'))
                return
            u.reg_write(UC_ARM64_REG_X0, int(self.ladder_result if probe else self.ladder_commit))
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
            return
        if hasattr(self, 'ladder_calls') and pc == self.ladder_return:
            u.reg_write(UC_ARM64_REG_X0, int(self.ladder_result))
            u.reg_write(UC_ARM64_REG_PC, self.ladder_outer_return)
            return
        return super().service(u, pc, size, context)


def main():
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        m = LadderMachine(base)
        assert base + LANDING in m.hooks and 'auto ladder: READY; prototype 2;' in m.text
        # Full frame sequence: real automatic hop guards and native timer tail.
        # A height-changing tentative position must use the one-use stable proof.
        bank = struct.pack('<fff', 105., 15., 205.)
        m.u.mem_write(m.actor + 0x278, bank)
        m.u.mem_write(m.actor + 0x284, bank)
        before = bytes(m.u.mem_read(m.inline_ctx, 0x100))
        for count in range(32):
            m.ladder_calls.clear()
            assert m.automatic(0, prime=False) == (2 if count <= 30 else 1)
            assert m.ladder_calls == ([1] if count <= 30 else [1, 0])
            if count == 31: break
            m.u.mem_write(m.actor + 0x278, struct.pack('<fff',105.,12.9,204.8))
            assert m.automatic(1, prime=False) == 2
            assert m.ladder_calls == [1]
            m.tick(2)
            assert m.read(m.player + 0x6a0, 2) == count + 1
            assert m.automatic(1, prime=False) == 0  # Proof consumed once.
            m.u.mem_write(m.actor + 0x278, bank)
            cases += 1
        assert bytes(m.u.mem_read(m.inline_ctx, 0x100)) == before
        assert m.read(m.module.find('CurrentLadderProbe')) == 0
        m.u.reg_write(UC_ARM64_REG_X0, m.inline_ctx)
        m.call(m.module.find('AutoLadderLanding', 'Callback'))
        assert bytes(m.u.mem_read(m.inline_ctx, 0x100)) == before
        assert m.vault_calls == []
        # A native "success" that only permits DisableLadder dialogue cannot arm.
        for valid, native in ((False, True), (True, False), (False, False)):
            m.ladder_valid, m.ladder_result = valid, native
            m.write(m.player + 0x6a0, 31, 2)
            m.ladder_calls.clear()
            assert m.automatic(0, prime=False) == 0 and m.ladder_calls == [1]
            assert m.automatic(1, prime=False) == 0
            cases += 1
        m.ladder_valid = m.ladder_result = True
        m.ladder_commit = False
        m.ladder_calls.clear()
        assert m.automatic(0, prime=False) == 0 and m.ladder_calls == [1, 0]
        m.ladder_commit = True
        # Release, strong-input/speed/direction/contact guards, ordinary hop priority.
        for change in ('release','weak','speed','contact','facing','input','native','tool','bank','handle'):
            n = LadderMachine(base)
            assert n.automatic(0, prime=False) == 2
            n.ladder_calls.clear()
            if change == 'release': n.stick(0.,0.)
            if change == 'weak': n.stick(0.,.5)
            if change == 'speed': n.u.mem_write(n.actor+0x2e0, struct.pack('<f',6.))
            if change == 'contact': n.contact = False
            if change == 'facing': n.direction = 0x40000000
            if change == 'input': n.input_direction = 0x40000000
            if change == 'native': n.hop_result = 1
            if change == 'tool': n.tool = 0x28
            if change == 'bank': n.u.mem_write(n.actor+0x284, struct.pack('<fff',.01,0.,0.))
            if change == 'handle': n.write(n.player+0x230,7,4)
            assert n.automatic(1, prime=False) == (1 if change == 'native' else 0)
            assert n.ladder_calls == [] and n.vault_calls == []
            cases += 1
        # Exact context scope: another player cannot validate this probe.
        n = LadderMachine(base)
        n.write(n.inline_ctx + 19*8, n.player+0x3000)
        assert n.automatic(0, prime=False) == 0
        cases += 1
        # Execute the actual final native ladder terrain/probe branches. High
        # forbidden terrain returns 1 too, but bypasses our valid-landing site.
        for height, allowed in ((15., False), (30., False), (45., False),
                                (45., True), (60., False), (60., True)):
            n = LadderMachine(base)
            n.native_landing_test = True
            n.terrain_allowed = allowed
            n.reached_landing = False
            n.u.mem_write(base + LANDING, RAW[LANDING:LANDING+20])
            n.u.reg_write(UC_ARM64_REG_X19, n.player)
            n.u.reg_write(UC_ARM64_REG_X20, n.request)
            n.u.reg_write(UC_ARM64_REG_W21, 1)  # Check only, no reservation/dialogue.
            n.u.reg_write(UC_ARM64_REG_X29, n.ram + 0x18000)
            n.u.reg_write(UC_ARM64_REG_S0, struct.unpack('<I', struct.pack('<f',height))[0])
            n.u.emu_start(base + 0x2396b00, base + 0x2396bcc, count=1000)
            assert n.u.reg_read(UC_ARM64_REG_PC) == base + 0x2396bcc
            assert n.reached_landing == (height <= 44.5 or allowed)
            assert n.u.reg_read(UC_ARM64_REG_W8) == 1
            cases += 1
        # Reproduce uphill failure with REAL native contact records, not a
        # model saying "wall=true": masks 4/6 exclude 0x04 and 0x80 contacts;
        # scoped retry 7 admits them, retaining the native facing filter.
        for flags in (0x04, 0x80, 0x44, 0x100, 0):
            n = LadderMachine(base)
            n.real_contacts = True
            collision = n.ram + 0x20000
            n.write(n.player + 0xa0, collision)
            n.write(collision + 0x2ef4, 1, 4)
            n.u.mem_write(collision + 0x2ef8 + 0x38, struct.pack('<fff',0.,0.,-1.))
            n.write(collision + 0x2ef8 + 0x4c, flags, 4)
            n.u.mem_write(n.request, bytes([0x5a])*0x30)
            n.write(n.request+0x20, 6, 4)
            n.write(n.request+0x28, n.actor+0x2b4)
            # Outside a synchronous retry, even a ladder keeps mask 6.
            n.u.reg_write(UC_ARM64_REG_X0, n.player)
            n.u.reg_write(UC_ARM64_REG_X1, n.request)
            n.call(n.module.find('AutoLadderContact', 'Callback'))
            assert n.reg(0) == int(flags == 0x100)
            assert n.read(n.request+0x20,4) == 6
            assert n.read(n.request+0x28) == n.actor+0x2b4
            n.contact_classes.clear()
            expected = 2 if flags in (4, 0x80, 0x100) else 0
            assert n.automatic(0, prime=False) == expected, (hex(flags), n.contact_classes)
            assert n.contact_classes == ([4,6] if flags == 0x100 else [4,6,4,7])
            if expected:
                assert n.automatic(1, prime=False) == 2
                n.tick(2)
                assert n.read(n.player+0x6a0,2) == 1
            assert n.read(n.module.find('CurrentAttempt')) == 0
            cases += 1
        # A climbable wall still requires strong input, low speed, native
        # contact/facing and input alignment, and a valid native ladder landing.
        for change in ('release','weak','speed','facing','input','blocked','other_tool','manual'):
            n = LadderMachine(base)
            n.wall_contact = True
            if change == 'release': n.stick(0.,0.)
            if change == 'weak': n.stick(0.,.5)
            if change == 'speed': n.u.mem_write(n.actor+0x2e0, struct.pack('<f',6.))
            if change == 'facing': n.direction = 0x40000000
            if change == 'input': n.input_direction = 0x40000000
            if change == 'blocked': n.ladder_valid = False
            if change == 'other_tool': n.tool = 0x28
            if change == 'manual':
                assert n.geometry(automatic=0) == 0
                assert n.contact_classes == []
            else:
                assert n.automatic(0, prime=False) == 0
            if change in ('other_tool','manual','release','weak','speed'):
                assert 7 not in n.contact_classes
            assert n.vault_calls == []
            cases += 1
        # Independent settings and compatibility failures preserve the other tool.
        for setting, expected in ((b'auto_vault=false\nauto_ladder=true\n',6+int(DIAGNOSTIC)),
                                  (b'auto_vault=true\nauto_ladder=false\n',5+EXTRA),
                                  (b'auto_vault=false\nauto_ladder=false\n',2)):
            machine = Machine(base, {'controls': DEFAULT+b'hold_to_hoover=false\nhold_to_advance_dialogue=false\n'+setting})
            machine.boot(); machine.activate()
            assert len(machine.hooks) == expected, machine.text
            cases += 1
        for offset in (LADDER, 0x239630c, LANDING, 0x15f3040):
            conflict = Machine(base, {'controls': CONFIG})
            conflict.write(base + offset, 0, 4)
            conflict.boot(); conflict.activate()
            assert len(conflict.hooks) == 5 + EXTRA and base + LANDING not in conflict.hooks
            assert 'auto ladder: DISABLED' in conflict.text
            cases += 1
        conflict = Machine(base, {'controls': CONFIG})
        conflict.write(base + 0x239a340, 0, 4)
        conflict.boot(); conflict.activate()
        assert len(conflict.hooks) == 6 + int(DIAGNOSTIC) and base + LANDING in conflict.hooks
        cases += 1
    print(f'PASS: {cases} auto ladder cases at two bases; compiled policy, native hop guards/timer; live cliff contact pending')

if __name__ == '__main__': main()
