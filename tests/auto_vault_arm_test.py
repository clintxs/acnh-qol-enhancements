"""Run compiled auto vault plus native stick/contact/direction/timer scheduling.

Terrain geometry, equipment lookup, parameter resources and traversal reservation
are modeled. This does not prove live collision, animation or multiplayer timing.
"""
import struct
from core_arm_test import Machine, DEFAULT, RAW
from unicorn.arm64_const import *

GEOMETRY = 0x230c470
DEPENDENCIES = (GEOMETRY, 0x230b8d0, 0x230bf18, 0x239a340, 0x239aff4,
                0x15f5c40, 0x241e98c, 0x221a094, 0x2223454)
CONFIG = DEFAULT + b'hold_to_hoover=false\nhold_to_advance_dialogue=false\nauto_vault=true\n'


class VaultMachine(Machine):
    def __init__(self, base):
        super().__init__(base, {'controls': CONFIG})
        self.boot(); self.activate()
        assert len(self.hooks) == 4 and all(base + a in self.hooks for a in (GEOMETRY, 0x230b8d0)), self.text
        self.player = self.ram + 0x1000
        self.actor = self.ram + 0x6000
        self.request = self.ram + 0x8000
        self.parameters = self.ram + 0x9000
        self.vtable = self.ram + 0xb000
        self.values = self.ram + 0xc000
        self.original = self.ram + 0xd000
        self.original_entry = self.ram + 0xd030
        self.getter = self.ram + 0xd010
        self.nested_return = self.ram + 0xd020
        self.manual_request = self.ram + 0xe000
        self.u.mem_write(self.manual_request, struct.pack('<B3xIB3xfB', 1, 0, 0, 0., 0))
        self.nested = False
        self.nested_results = []
        self.write(self.module.find('AutoVaultGeometry', 'OrigRef'), self.original)
        # Installation is modeled, so restore the entry bytes to execute its
        # real body as the trampoline original in native scheduling cases.
        self.u.mem_write(base + 0x230b8d0, RAW[0x230b8d0:0x230b8d0 + 20])
        self.write(self.player + 0x68, self.actor)
        self.write(self.player + 0x60, self.ram + 0x10000)
        self.write(self.player + 0x12f8, 1, 1)
        for offset in (0x120, 0xea0, 0xed0):
            self.write(self.parameters + offset, self.vtable)
        self.write(self.vtable + 0xf0, self.getter)
        self.u.mem_write(self.values, struct.pack('<ffI', .2, 5., 30))
        self.hop_result = 0
        self.tool = 0x28
        self.vaultable = self.commit_ok = self.contact = True
        self.direction = self.input_direction = 0
        self.vault_calls = []
        self.equipment_calls = 0
        self.parameter_calls = 0
        self.geometry_calls = []
        self.output = 0
        self.final_hop_result = None
        self.stick(0., 1.)

    def stick(self, x, y):
        self.u.mem_write(self.player + 0xeec, struct.pack('<ff', x, y))

    def service(self, u, pc, size, context):
        if not hasattr(self, 'original'):
            return super().service(u, pc, size, context)
        if pc == self.base + GEOMETRY:
            u.reg_write(UC_ARM64_REG_PC, self.module.find('AutoVaultGeometry', 'Callback'))
            return
        if pc == self.original:
            self.geometry_calls.append(bytes(u.mem_read(self.reg(1), 17)))
            result = self.hop_result
        elif pc == self.original_entry:
            u.reg_write(UC_ARM64_REG_X1, self.request)
            u.reg_write(UC_ARM64_REG_X2, self.output)
            if self.final_hop_result is not None:
                # Simulate HopEntry finding an adjusted native hop AFTER its
                # first failed straight geometry call observes eligible intent.
                self.geometry_return = u.reg_read(UC_ARM64_REG_LR)
                u.reg_write(UC_ARM64_REG_LR, self.ram + 0xd040)
            u.reg_write(UC_ARM64_REG_PC, self.module.find('AutoVaultGeometry', 'Callback'))
            return
        elif pc == self.ram + 0xd040:
            u.reg_write(UC_ARM64_REG_X0, self.final_hop_result)
            u.reg_write(UC_ARM64_REG_PC, self.geometry_return)
            return
        elif pc == self.base + 0x15f5c40:
            assert self.reg(0) == self.player
            self.equipment_calls += 1
            result = self.tool
        elif pc == self.base + 0x239a340:
            assert self.reg(0) == self.player and self.reg(1) in (0, 1)
            self.vault_calls.append(self.reg(1))
            result = int(self.vaultable if self.reg(1) else self.commit_ok)
            if self.nested:
                # The real vault validator calls the hop geometry in manual
                # mode first. Exercise that re-entry into our compiled hook.
                self.nested_result = result
                self.outer_return = u.reg_read(UC_ARM64_REG_LR)
                u.reg_write(UC_ARM64_REG_X0, self.player)
                u.reg_write(UC_ARM64_REG_X1, self.manual_request)
                u.reg_write(UC_ARM64_REG_X2, 0)
                u.reg_write(UC_ARM64_REG_LR, self.nested_return)
                u.reg_write(UC_ARM64_REG_PC, self.module.find('AutoVaultGeometry', 'Callback'))
                return
        elif pc == self.nested_return:
            self.nested_results.append(self.reg(0))
            u.reg_write(UC_ARM64_REG_X0, self.nested_result)
            u.reg_write(UC_ARM64_REG_PC, self.outer_return)
            return
        elif pc == self.base + 0x241e98c:
            assert self.reg(0) == 1
            self.parameter_calls += 1
            result = self.parameters
        elif pc == self.getter:
            offset = self.reg(0) - self.parameters
            result = self.values + {0x120: 0, 0xea0: 4, 0xed0: 8}[offset]
        elif pc == self.base + 0x161ee68:
            result = 0  # No stage-specific prohibition in the modeled resource.
        elif pc == self.base + 0x15f3040:
            u.mem_write(self.reg(1), struct.pack('<fff', 0., 0., -1.))
            result = int(self.contact)
        elif pc == self.base + 0x5f0400:
            result = self.direction ^ 0x80000000  # Modeled contact-normal angle.
        elif pc == self.base + 0x2469464:
            # Native angle helper returns its small wrapper through X8.
            self.write(self.reg(8), self.input_direction, 4)
            result = 0
        elif pc == self.base + 0x230b654:
            result = 0  # No stage/position-specific hop prohibition.
        else:
            return super().service(u, pc, size, context)
        u.reg_write(UC_ARM64_REG_X0, result)
        u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))

    def geometry(self, probe=0, automatic=1, skew=0., output=0):
        self.u.mem_write(self.request, struct.pack('<B3xIB3xfB', probe, 0, automatic, skew, 0))
        self.output = output
        self.write(self.module.find('AutoVaultEntry', 'OrigRef'), self.original_entry)
        for index, value in enumerate((self.player, probe, automatic)):
            self.u.reg_write(UC_ARM64_REG_X0 + index, value)
        self.call(self.module.find('AutoVaultEntry', 'Callback'))
        return self.reg(0)

    def automatic(self, probe):
        # Execute native HopEntry and HopDirection, not a Python copy of their
        # strength, motion, facing, direction and alternate-candidate guards.
        for index, value in enumerate((self.player, probe, 1)):
            self.u.reg_write(UC_ARM64_REG_X0 + index, value)
        self.write(self.module.find('AutoVaultEntry', 'OrigRef'), self.base + 0x230b8d0)
        self.call(self.module.find('AutoVaultEntry', 'Callback'))
        return self.reg(0)

    def tick(self, result):
        # Actual movement-update tail: 2 increments, 0 clears, 1 leaves alone.
        sp, stop = self.ram + 0x1f0000 - 0x70, self.ram + 0x100
        self.u.mem_write(sp, bytes(0x70))
        self.write(sp, result, 4)
        self.write(sp + 0x38, stop)
        self.u.reg_write(UC_ARM64_REG_SP, sp)
        self.u.reg_write(UC_ARM64_REG_X19, self.player)
        self.u.emu_start(self.base + 0x221a0a8, stop, count=1000)
        assert self.u.reg_read(UC_ARM64_REG_PC) == stop
        assert self.u.reg_read(UC_ARM64_REG_SP) == sp + 0x70


def main():
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        m = VaultMachine(base)
        for hop in (1, 2):
            m.hop_result = hop
            assert m.geometry() == hop and not m.vault_calls and not m.equipment_calls
            cases += 1
        m.hop_result = 0
        for adjusted_hop in (1, 2):
            m.final_hop_result = adjusted_hop
            assert m.geometry() == adjusted_hop and not m.vault_calls and not m.equipment_calls
            cases += 1
        m.final_hop_result = None
        for arguments in ({'automatic': 0}, {'skew': 27.}, {'skew': -27.}, {'output': m.ram + 0xe000}):
            assert m.geometry(**arguments) == 0 and not m.vault_calls and not m.equipment_calls
            cases += 1
        for tool in (0, 8, 9, 0x11, 0x12, 0x19, 0x71):
            m.tool = tool
            assert m.geometry() == 0 and not m.vault_calls
            cases += 1
        m.tool = 0x28
        m.vaultable = False
        assert m.geometry() == 0 and m.vault_calls == [1] and not m.parameter_calls
        m.vaultable = True
        for count in (0, 1, 29, 30, 31, 32, 65535):
            for probe in (0, 1):
                m.vault_calls.clear()
                m.write(m.player + 0x6a0, count, 2)
                assert m.geometry(probe=probe) == (2 if count <= 30 else 1)
                assert m.vault_calls == ([1, 0] if count > 30 and not probe else [1])
                assert m.read(m.player + 0x6a0, 2) == count  # Hook doesn't own timer writes.
                cases += 1
        # Read the resource's threshold instead of assuming 30 forever.
        m.write(m.values + 8, 60, 4)
        m.write(m.player + 0x6a0, 31, 2)
        assert m.geometry() == 2
        m.write(m.values + 8, 30, 4)
        m.commit_ok = False
        m.vault_calls.clear()
        assert m.geometry() == 0 and m.vault_calls == [1, 0]
        m.commit_ok = True
        m.nested = True
        m.vault_calls.clear()
        assert m.geometry() == 1 and m.vault_calls == [1, 0] and m.nested_results == [0, 0]
        m.nested = False
        # Execute the real final validator in check-only mode: no reservation,
        # player write or state transition is permitted by this early return.
        before = bytes(m.u.mem_read(m.player, 0x3000))
        for index, value in enumerate((m.player, m.ram + 0xf000, 1)):
            m.u.reg_write(UC_ARM64_REG_X0 + index, value)
        m.call(base + 0x239aff4)
        assert m.reg(0) == 1 and before == bytes(m.u.mem_read(m.player, 0x3000))

        # Real native automatic-entry guards must reject before our extension.
        for x, y in ((0., 0.), (0., .5), (0., .919)):
            m.vault_calls.clear(); m.stick(x, y)
            assert m.automatic(1) == 0 and not m.vault_calls
            cases += 1
        m.stick(0., 1.)
        m.contact = False
        m.vault_calls.clear()
        assert m.automatic(1) == 0 and not m.vault_calls
        m.contact = True
        m.u.mem_write(m.actor + 0x2e0, struct.pack('<f', 6.))
        assert m.automatic(1) == 0 and not m.vault_calls
        m.u.mem_write(m.actor + 0x2e0, struct.pack('<f', 0.))
        for attribute in ('direction', 'input_direction'):
            setattr(m, attribute, 0x40000000)  # 90 degrees from facing/contact.
            assert m.automatic(1) == 0 and not m.vault_calls
            setattr(m, attribute, 0)
            cases += 1

        # Native probe + native timer accumulation, then the commit call used
        # by the dispatcher. No synthetic A, direct state edits or tool writes.
        m.write(m.player + 0x6a0, 0, 2)
        for count in range(32):
            m.vault_calls.clear()
            result = m.automatic(1)
            assert result == (2 if count <= 30 else 1), (count, result)
            assert m.vault_calls == [1]
            m.tick(result)
            assert m.read(m.player + 0x6a0, 2) == min(count + 1, 31)
            cases += 1
        m.vault_calls.clear()
        assert m.automatic(0) == 1 and m.vault_calls == [1, 0]
        m.stick(0., 0.)
        m.tick(m.automatic(1))
        assert m.read(m.player + 0x6a0, 2) == 0
        m.stick(0., 1.)
        assert m.automatic(1) == 2  # Releasing starts the hold over.
        m.vaultable = False
        m.tick(m.automatic(1))
        assert m.read(m.player + 0x6a0, 2) == 0

        for offset in DEPENDENCIES:
            conflict = Machine(base, {'controls': CONFIG})
            conflict.write(base + offset, 0, 4)
            conflict.boot(); conflict.activate()
            assert len(conflict.hooks) == 2 and 'auto vault: DISABLED' in conflict.text
            cases += 1
        for setting in (b'', b'auto_vault=false\n'):
            disabled = Machine(base, {'controls': DEFAULT + b'hold_to_hoover=false\nhold_to_advance_dialogue=false\n' + setting})
            disabled.boot(); disabled.activate()
            assert len(disabled.hooks) == 2
        overlay = Machine(base, {'controls': CONFIG, 'controls.custom': b'auto_vault=false'})
        overlay.boot(); overlay.activate()
        assert len(overlay.hooks) == 2
        enabled = Machine(base, {'controls': DEFAULT, 'controls.custom': b'auto_vault=true'})
        enabled.boot(); enabled.activate()
        assert base + GEOMETRY in enabled.hooks and len(enabled.hooks) == 15
        absent = Machine(base, {'controls.custom': b'auto_vault=true'})
        absent.boot(); absent.activate()
        assert len(absent.hooks) == 2
    print(f'PASS: {cases} auto vault cases at two bases; compiled policy, native automatic hop guards and timer; live terrain/animation pending')


if __name__ == '__main__': main()
