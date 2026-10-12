"""Run compiled auto vault plus native stick/contact/direction/timer scheduling.

Terrain geometry, equipment lookup, parameter resources and traversal reservation
are modeled. This does not prove live collision, animation or multiplayer timing.
"""
import struct
from pathlib import Path
from core_arm_test import Machine, DEFAULT, RAW
from unicorn.arm64_const import *

GEOMETRY = 0x230c470
DEPENDENCIES = (GEOMETRY, 0x230b8d0, 0x230bf18, 0x239a340, 0x239aff4,
                0x15f5c40, 0x241e98c, 0x239a398, 0x221a094, 0x2223454)
DIAGNOSTIC = b"DIAGNOSTIC: auto-vault-v4" in (Path(__file__).resolve().parents[1] / "project.elf").read_bytes()
EXTRA = 3 * int(DIAGNOSTIC)
CONFIG = DEFAULT + b'hold_to_hoover=false\nhold_to_advance_dialogue=false\nauto_vault=true\nauto_ladder=false\nauto_equip_traversal=false\n'


class VaultMachine(Machine):
    def __init__(self, base, verbose=True, config=CONFIG, expected_hooks=None):
        super().__init__(base, {'controls': config}, verbose=verbose)
        self.boot(); self.activate()
        assert len(self.hooks) == (5 + EXTRA if expected_hooks is None else expected_hooks) and all(base + a in self.hooks for a in (GEOMETRY, 0x230b8d0)), self.text
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
        self.prepare_return = self.ram + 0xd050
        self.prepare_original = self.ram + 0xd060
        self.manual_original = self.ram + 0xd070
        self.prepare_calls = []
        self.manual_original_calls = 0
        self.commit_calls = 0
        self.predicted_vaultable = False
        self.reject_tentative = False
        self.write(self.module.find('AutoVaultPrepare', 'OrigRef'), self.prepare_original)
        self.u.mem_write(base + 0x239aff4, RAW[0x239aff4:0x239aff4 + 20])
        self.manual_request = self.ram + 0xe000
        self.u.mem_write(self.manual_request, struct.pack('<B3xIB3xfB', 1, 0, 0, 0., 0))
        self.nested = False
        self.manual_hop_result = 0
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

    def prepare_vault(self):
        self.u.reg_write(UC_ARM64_REG_X0, self.player)
        self.u.reg_write(UC_ARM64_REG_X1, self.ram + 0xf000)
        self.u.reg_write(UC_ARM64_REG_X2, self.vault_calls[-1])
        self.u.reg_write(UC_ARM64_REG_LR, self.prepare_return)
        self.u.reg_write(UC_ARM64_REG_PC, self.module.find('AutoVaultPrepare', 'Callback'))

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
            argument = self.reg(1)
            self.vault_calls.append(argument)
            # Model the observed failure: the native argument=1 position can
            # fail even when the current-position crossing is valid.
            self.vault_result = int(self.predicted_vaultable if argument else self.vaultable)
            if self.reject_tentative and struct.unpack('<f', bytes(u.mem_read(self.actor + 0x27c, 4)))[0] < -.5:
                self.vault_result = 0
            self.outer_return = u.reg_read(UC_ARM64_REG_LR)
            if self.nested:
                self.saved_entry = self.read(self.module.find('AutoVaultEntry', 'OrigRef'))
                self.write(self.module.find('AutoVaultEntry', 'OrigRef'), self.manual_original)
                u.reg_write(UC_ARM64_REG_X0, self.player)
                u.reg_write(UC_ARM64_REG_X1, argument)
                u.reg_write(UC_ARM64_REG_X2, 0)
                u.reg_write(UC_ARM64_REG_LR, self.nested_return)
                u.reg_write(UC_ARM64_REG_PC, self.module.find('AutoVaultEntry', 'Callback'))
                return
            if self.vault_result:
                self.prepare_vault()
                return
            result = 0
        elif pc == self.manual_original:
            self.manual_original_calls += 1
            result = self.manual_hop_result
        elif pc == self.nested_return:
            nested = self.reg(0)
            self.nested_results.append(nested)
            self.write(self.module.find('AutoVaultEntry', 'OrigRef'), self.saved_entry)
            if nested == 1:
                u.reg_write(UC_ARM64_REG_X0, 1)
                u.reg_write(UC_ARM64_REG_PC, self.outer_return)
                return
            if self.vault_result:
                self.prepare_vault()
                return
            u.reg_write(UC_ARM64_REG_X0, 0)
            u.reg_write(UC_ARM64_REG_PC, self.outer_return)
            return
        elif pc == self.prepare_original:
            self.prepare_calls.append(self.reg(2))
            if self.reg(2) & 1:
                # Execute the native check-only early return, not its model.
                u.reg_write(UC_ARM64_REG_PC, self.base + 0x239aff4)
                return
            self.commit_calls += 1
            result = int(self.commit_ok)
        elif pc == self.prepare_return:
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

    def geometry(self, probe=0, automatic=1, skew=0., output=0, prime=True):
        if prime and probe and automatic:
            count = self.read(self.player + 0x6a0, 2)
            self.write(self.player + 0x6a0, 0, 2)
            self.geometry(0, automatic, skew, output, prime=False)
            self.write(self.player + 0x6a0, count, 2)
        self.u.mem_write(self.request, struct.pack('<B3xIB3xfB', probe, 0, automatic, skew, 0))
        self.output = output
        self.write(self.module.find('AutoVaultEntry', 'OrigRef'), self.original_entry)
        for index, value in enumerate((self.player, probe, automatic)):
            self.u.reg_write(UC_ARM64_REG_X0 + index, value)
        self.call(self.module.find('AutoVaultEntry', 'Callback'))
        return self.reg(0)

    def automatic(self, probe, prime=True):
        # Independent probe cases first create a real compiled action proof
        # below threshold. Full frame regressions pass prime=False explicitly.
        if prime and probe:
            count = self.read(self.player + 0x6a0, 2)
            self.write(self.player + 0x6a0, 0, 2)
            self.automatic(0, prime=False)
            self.write(self.player + 0x6a0, count, 2)
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
        if DIAGNOSTIC:
            trace = VaultMachine(base, verbose=False)
            assert 'DIAGNOSTIC: auto-vault-v4' in trace.text
            ctx = trace.ram + 0xf000
            trace.u.mem_write(ctx, bytes(0x100))
            trace.write(ctx + 19*8, trace.player)
            trace.write(trace.player + 0x1810, 1, 4)
            before_ctx = bytes(trace.u.mem_read(ctx, 0x100))
            before_player = bytes(trace.u.mem_read(trace.player, 0x2000))
            for _ in range(400):
                trace.u.reg_write(UC_ARM64_REG_X0, ctx)
                trace.call(trace.module.find('AutoVaultMovementTrace', 'Callback'))
            assert bytes(trace.u.mem_read(ctx, 0x100)) == before_ctx
            assert bytes(trace.u.mem_read(trace.player, 0x2000)) == before_player
            assert 1 <= trace.text.count('VAULT movement:') <= 4
            assert 'state=1 action=4294967295 tool=0x28 input=1' in trace.text
            trace.nested = True
            for count in (0, 15, 30, 31):
                trace.write(trace.player + 0x6a0, count, 2)
                assert trace.automatic(1) == (2 if count <= 30 else 1)
            assert 'geometry=0x1f tool=0x28 timer=31 vault=1 result=1' in trace.text
            assert trace.text.count('VAULT entry:') <= 8
            assert 'VAULT search: phase=1 native=0 prepared=0 transfer=1' in trace.text
            # Float-inline context: SIMD at +0, GP at +0x200, saved LR at +0x2f0.
            # Exercise each rejection category; callback must preserve every
            # GP/SIMD byte and player memory while changing only its scope.
            float_ctx = trace.ram + 0x11000
            scope = trace.ram + 0x12000
            scope_slot = trace.module.find('CurrentVaultProbe')
            trace.u.mem_write(scope, bytes(64)); trace.write(scope, trace.player)
            trace.write(scope_slot, scope)
            trace.u.mem_write(float_ctx, bytes([0x5a])*0x300)
            trace.write(float_ctx + 0x200 + 20*8, trace.player)
            trace.write(float_ctx + 0x200, 7)
            trace.u.mem_write(float_ctx, struct.pack('<f', 1.25))
            for site in (0x239a8b0, 0x239a98c, 0x239aabc, 0x239aad4,
                         0x239ab08, 0x239af80, 0x239a6b4, 0x239a574):
                trace.write(float_ctx + 0x2f0, base + site)
                snapshot = bytes(trace.u.mem_read(float_ctx, 0x300))
                player_snapshot = bytes(trace.u.mem_read(trace.player, 0x3000))
                trace.u.reg_write(UC_ARM64_REG_X0, float_ctx)
                trace.call(trace.module.find('AutoVaultRejectedTrace', 'Callback'))
                assert bytes(trace.u.mem_read(float_ctx, 0x300)) == snapshot
                assert bytes(trace.u.mem_read(trace.player, 0x3000)) == player_snapshot
                assert trace.read(scope + 24, 4) == site
            assert trace.read(scope + 16, 4) == 8 and trace.read(scope + 20, 4) == 255
            assert trace.read(scope + 28, 4) == 7 and trace.read(scope + 32, 4) == 0x3fa00000
            scope_snapshot = bytes(trace.u.mem_read(scope, 64))
            trace.write(float_ctx + 0x2f0, base + 0x239a5a0)
            trace.u.reg_write(UC_ARM64_REG_X0, float_ctx)
            trace.call(trace.module.find('AutoVaultExitTrace', 'Callback'))
            assert bytes(trace.u.mem_read(scope, 64)) == scope_snapshot
            trace.write(scope + 16, 0, 4)
            trace.u.reg_write(UC_ARM64_REG_X0, float_ctx)
            trace.call(trace.module.find('AutoVaultExitTrace', 'Callback'))
            assert trace.read(scope + 24, 4) == 0x239a5a0
            scope_snapshot = bytes(trace.u.mem_read(scope, 64))
            trace.write(float_ctx + 0x200 + 20*8, trace.player + 0x4000)
            trace.u.reg_write(UC_ARM64_REG_X0, float_ctx)
            trace.call(trace.module.find('AutoVaultRejectedTrace', 'Callback'))
            assert bytes(trace.u.mem_read(scope, 64)) == scope_snapshot
            trace.write(scope_slot, 0)
            trace.u.reg_write(UC_ARM64_REG_X0, float_ctx)
            trace.call(trace.module.find('AutoVaultRejectedTrace', 'Callback'))
            assert bytes(trace.u.mem_read(scope, 64)) == scope_snapshot
            cases += 8
        m = VaultMachine(base)
        # Execute the native selector instead of inferring its flag semantics.
        for argument, offset in ((0, 0x278), (1, 0x284)):
            m.u.reg_write(UC_ARM64_REG_X20, m.player)
            m.u.reg_write(UC_ARM64_REG_W19, argument)
            m.u.reg_write(UC_ARM64_REG_X0, m.values)
            m.u.emu_start(base + 0x239a394, base + 0x239a3b8, count=10)
            assert m.u.reg_read(UC_ARM64_REG_X21) == m.actor + offset
            assert m.u.reg_read(UC_ARM64_REG_X24) == offset
            cases += 1
        for hop in (1, 2):
            m.hop_result = hop
            assert m.geometry() == hop and not m.vault_calls and (DIAGNOSTIC or not m.equipment_calls)
            cases += 1
        m.hop_result = 0
        for adjusted_hop in (1, 2):
            m.final_hop_result = adjusted_hop
            assert m.geometry() == adjusted_hop and not m.vault_calls and (DIAGNOSTIC or not m.equipment_calls)
            cases += 1
        m.final_hop_result = None
        for arguments in ({'automatic': 0}, {'skew': 27.}, {'skew': -27.}, {'output': m.ram + 0xe000}):
            assert m.geometry(**arguments) == 0 and not m.vault_calls and (DIAGNOSTIC or not m.equipment_calls)
            cases += 1
        for tool in (0, 8, 9, 0x11, 0x12, 0x19, 0x71):
            m.tool = tool
            assert m.geometry() == 0 and not m.vault_calls
            cases += 1
        m.tool = 0x28
        m.vaultable = False
        assert m.geometry() == 0 and m.vault_calls == [0] and not m.parameter_calls
        m.vaultable = True
        for count in (0, 1, 29, 30, 31, 32, 65535):
            for probe in (0, 1):
                m.vault_calls.clear()
                m.write(m.player + 0x6a0, count, 2)
                assert m.geometry(probe=probe) == (2 if count <= 30 else 1)
                assert m.vault_calls == ([0, 0] if count > 30 and not probe else [0])
                assert m.read(m.player + 0x6a0, 2) == count  # Hook doesn't own timer writes.
                cases += 1
        # Read the resource's threshold instead of assuming 30 forever.
        m.write(m.values + 8, 60, 4)
        m.write(m.player + 0x6a0, 31, 2)
        assert m.geometry() == 2
        m.write(m.values + 8, 30, 4)
        m.commit_ok = False
        m.vault_calls.clear()
        assert m.geometry() == 0 and m.vault_calls == [0, 0]
        m.commit_ok = True
        m.nested = True
        m.vault_calls.clear()
        assert m.geometry() == 1 and m.vault_calls == [0, 0] and m.nested_results == [0, 0]
        m.nested = False
        # Execute the real final validator in check-only mode: no reservation,
        # player write or state transition is permitted by this early return.
        before = bytes(m.u.mem_read(m.player, 0x3000))
        for index, value in enumerate((m.player, m.ram + 0xf000, 1)):
            m.u.reg_write(UC_ARM64_REG_X0 + index, value)
        m.call(base + 0x239aff4)
        assert m.reg(0) == 1 and before == bytes(m.u.mem_read(m.player, 0x3000))

        # Real frame sequence: stable bank action -> tentative river movement.
        # The terrain model rejects tentative y=-2.09, as in the live log;
        # only the stable action phase may search for a vault.
        regression = VaultMachine(base)
        regression.nested = True
        regression.reject_tentative = True
        bank = struct.pack('<fff', 386.38, 0., 614.94)
        tentative = struct.pack('<fff', 386.58, -2.09, 614.19)
        for count in range(31):
            regression.u.mem_write(regression.actor + 0x278, bank)
            regression.u.mem_write(regression.actor + 0x284, bank)
            regression.vault_calls.clear()
            assert regression.automatic(0, prime=False) == 2
            assert regression.vault_calls == [0]
            assert regression.commit_calls == 0 and regression.manual_original_calls == 0
            regression.u.mem_write(regression.actor + 0x278, tentative)
            before_actor = bytes(regression.u.mem_read(regression.actor, 0x400))
            regression.vault_calls.clear()
            result = regression.automatic(1, prime=False)
            assert result == 2 and regression.vault_calls == []
            assert bytes(regression.u.mem_read(regression.actor, 0x400)) == before_actor
            regression.tick(result)
            assert regression.read(regression.player + 0x6a0, 2) == count + 1
            # One proof cannot advance two movement checks.
            assert regression.automatic(1, prime=False) == 0
            cases += 1
        regression.u.mem_write(regression.actor + 0x278, bank)
        regression.vault_calls.clear()
        assert regression.automatic(0, prime=False) == 1
        assert regression.vault_calls == [0, 0] and regression.commit_calls == 1
        assert regression.prepare_calls[-2:] == [1, 0]
        assert regression.automatic(1, prime=False) == 0  # No proof survives commit.
        assert regression.read(regression.module.find('CurrentVaultProbe')) == 0
        cases += 1

        # Cancel transfer on changed bank, identity, released stick, a native
        # hop taking priority, manual A or failed stable validation.
        for change in ('bank', 'handle', 'resident', 'release', 'native', 'manual', 'invalid', 'pointer', 'tool', 'turn', 'contact', 'skip'):
            cancel = VaultMachine(base)
            assert cancel.automatic(0, prime=False) == 2
            cancel.vault_calls.clear()
            if change == 'bank': cancel.u.mem_write(cancel.actor + 0x284, struct.pack('<f', .01))
            elif change == 'handle': cancel.write(cancel.player + 0x230, 1, 4)
            elif change == 'resident': cancel.write(cancel.player + 0x1ee8, 1, 1)
            elif change == 'release': cancel.stick(0., 0.)
            elif change == 'native': cancel.hop_result = 1
            elif change == 'tool': cancel.tool = 8
            elif change == 'turn': cancel.input_direction = 0x40000000
            elif change == 'contact': cancel.contact = False
            elif change == 'skip':
                cancel.stick(0., 0.)
                assert cancel.automatic(0, prime=False) == 0
                cancel.stick(0., 1.)
            elif change == 'manual':
                assert cancel.geometry(automatic=0) == 0
            elif change == 'invalid':
                cancel.vaultable = False
                assert cancel.automatic(0, prime=False) == 0
                cancel.vault_calls.clear()
            elif change == 'pointer':
                # Different actor/player in the same resident slot must not
                # inherit a cached validation from the previous owner.
                other = cancel.player + 0x3000
                cancel.u.mem_write(other, bytes(cancel.u.mem_read(cancel.player, 0x2000)))
                cancel.player = other
            assert cancel.automatic(1, prime=False) == (1 if change == 'native' else 0)
            assert cancel.vault_calls == []
            cases += 1

        # Scoped check-only preparation still restores an enclosing different
        # player scope and never changes ordinary manual preparation.
        enclosing = regression.ram + 0xe400
        regression.write(enclosing, regression.player + 0x4000)
        scope = regression.module.find('CurrentVaultProbe')
        regression.write(scope, enclosing)
        regression.write(regression.player + 0x6a0, 0, 2)
        assert regression.automatic(0, prime=False) == 2
        assert regression.read(scope) == enclosing and regression.read(enclosing + 8, 1) == 0
        regression.commit_ok = False
        for i, value in enumerate((regression.player, regression.ram + 0xf000, 0)):
            regression.u.reg_write(UC_ARM64_REG_X0 + i, value)
        regression.call(regression.module.find('AutoVaultPrepare', 'Callback'))
        assert regression.reg(0) == 0 and regression.prepare_calls[-1] == 0
        regression.write(scope, 0)
        cases += 1

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
            assert m.vault_calls == [0]
            m.tick(result)
            assert m.read(m.player + 0x6a0, 2) == min(count + 1, 31)
            cases += 1
        m.vault_calls.clear()
        assert m.automatic(0) == 1 and m.vault_calls == [0, 0]
        m.stick(0., 0.)
        m.tick(m.automatic(1))
        assert m.read(m.player + 0x6a0, 2) == 0
        m.stick(0., 1.)
        assert m.automatic(1) == 2  # Releasing starts the hold over.
        m.vaultable = False
        m.tick(m.automatic(1))
        assert m.read(m.player + 0x6a0, 2) == 0

        for offset in DEPENDENCIES + ((0x2219fc0, 0x239a6c0, 0x239a5cc) if DIAGNOSTIC else ()):
            conflict = Machine(base, {'controls': CONFIG})
            conflict.write(base + offset, 0, 4)
            conflict.boot(); conflict.activate()
            assert len(conflict.hooks) == 2 and 'auto vault: DISABLED' in conflict.text
            cases += 1
        for setting in (b'auto_vault=false\n',):
            disabled = Machine(base, {'controls': DEFAULT + b'hold_to_hoover=false\nhold_to_advance_dialogue=false\nauto_ladder=false\nauto_equip_traversal=false\n' + setting})
            disabled.boot(); disabled.activate()
            assert len(disabled.hooks) == 2
        overlay = Machine(base, {'controls': CONFIG, 'controls.custom': b'auto_vault=false'})
        overlay.boot(); overlay.activate()
        assert len(overlay.hooks) == 2
        enabled = Machine(base, {'controls': DEFAULT + b'auto_ladder=false\nauto_equip_traversal=false\n', 'controls.custom': b'auto_vault=true'})
        enabled.boot(); enabled.activate()
        assert base + GEOMETRY in enabled.hooks and len(enabled.hooks) == 16 + EXTRA
        absent = Machine(base, {'controls.custom': b'auto_vault=true'})
        absent.boot(); absent.activate()
        assert len(absent.hooks) == 2
    print(f'PASS: {cases} auto vault cases at two bases; compiled policy, native automatic hop guards and timer; live terrain/animation pending')


if __name__ == '__main__': main()
