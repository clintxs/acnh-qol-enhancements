"""Compare Controls' observable decisions/native call counts with a saved ELF.

Each machine boots and installs through production configuration. Native
scheduling fragments execute as in the inherited fixtures; geometry, resources
and animation services retain those fixtures' modeling limits.
"""
import itertools
import os
from pathlib import Path
from unittest.mock import patch
import struct
from hold_y_arm_test import HooverMachine
from auto_swim_arm_test import SwimMachine, TICKS
from swimming_camera_arm_test import CameraMachine
from auto_vault_arm_test import VaultMachine
from auto_traversal_swap_arm_test import SwapMachine, AXE, KINDS, EMPTY
from dialogue_hurry_arm_test import HurryMachine


def main():
    filename = os.environ.get('QOL_BASELINE_ELF')
    if not filename:
        print('SKIP: Controls baseline comparison; set QOL_BASELINE_ELF')
        return
    baseline = Path(filename).read_bytes()
    original_read = Path.read_bytes
    def saved_bytes(path):
        return baseline if path.name == 'project.elf' and path.parent.name == 'qol-enhancements' else original_read(path)
    def pair(factory, base, *args):
        current = factory(base, *args)
        with patch.object(Path, 'read_bytes', saved_bytes): previous = factory(base, *args)
        return current, previous
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        current, previous = pair(HooverMachine, base)
        for initial in (100, 1018, 1024, 1031, 999):
            for success in (False, True):
                for m in (current, previous): m.reserve_ok = success
                assert current.frame(initial, fresh=True) == previous.frame(initial, fresh=True)
                for target, actual in itertools.product((100, 1018, 1024, 1031, 999, None), repeat=2):
                    assert current.frame(target, actual=actual) == previous.frame(target, actual=actual)
                    cases += 1
                assert current.frame(initial, held=False) == previous.frame(initial, held=False)
                assert current.frame(initial) == previous.frame(initial)
                cases += 3
        for boost in (False, True):
            current, previous = pair(SwimMachine, base, boost)
            for index in range(96):
                name = ('SurfaceStride', 'SurfaceEntry', 'DiveStride', 'DiveEntry')[index % 4]
                x, y = ((0.,0.), (.15,0.), (.6,0.), (1.,1.))[index % 4]
                held, pressed = (1 if index % 7 == 0 else 0), (8 if index % 11 == 0 else 0)
                for m in (current, previous): m.tick += (TICKS // 10) * (1 + index % 6)
                assert current.update(name, x, y, held, pressed) == previous.update(name, x, y, held, pressed)
                for offset in (0x1820, 0x1844): assert current.float(offset) == previous.float(offset)
                assert current.movement_results == previous.movement_results
                cases += 1
        current, previous = pair(CameraMachine, base)
        for action, flags, service in itertools.product((0, 0xc, 0xf7, 0xf9, 0xffffffff), (0, 512), (False, True)):
            for m in (current, previous): m.prepare(action, flags, service=service)
            assert current.result(True) == previous.result(True)
            cases += 1
        current, previous = pair(VaultMachine, base)
        for hop, tool, valid, timer, probe in itertools.product((0,1,2), (0,0x11,0x28), (False,True), (30,31), (0,1)):
            for m in (current, previous):
                m.hop_result, m.tool, m.vaultable = hop, tool, valid
                m.write(m.player+0x6a0, timer, 2)
                m.vault_calls.clear(); m.geometry_calls.clear(); m.prepare_calls.clear()
                m.equipment_calls = m.parameter_calls = m.commit_calls = 0
            assert current.geometry(probe=probe) == previous.geometry(probe=probe), (hop, tool, valid, timer, probe)
            for attr in ('vault_calls','geometry_calls','prepare_calls','equipment_calls','parameter_calls','commit_calls'):
                assert getattr(current, attr) == getattr(previous, attr), (attr, hop, tool, valid, timer, probe)
            cases += 1
        for ladder, reason in itertools.product((False,True), ('land','release','turn','move','blocked','manual','identity','remove','reorder','failure')):
            current, previous = pair(SwapMachine, base)
            for m in (current, previous):
                m.begin(ladder)
                if reason == 'release': m.stick(0.,0.)
                if reason == 'turn': m.write(m.actor+0x2b4, 100, 4)
                if reason == 'move': m.u.mem_write(m.actor+0x278, struct.pack('<f',1.))
                if reason == 'blocked': m.vaultable = m.ladder_valid = False
                if reason == 'manual': m.held, m.tool = AXE, KINDS[AXE]
                if reason == 'identity': m.write(m.player+0x230, 99, 4)
                if reason == 'remove': m.inventory[0] = EMPTY; m.stick(0.,0.)
                if reason == 'reorder': m.inventory[0], m.inventory[3] = m.inventory[3], m.inventory[0]; m.stick(0.,0.)
                if reason == 'failure': m.commit_ok = m.ladder_result = False
            assert current.dispatch() == previous.dispatch()
            assert current.action_requests == previous.action_requests
            if reason == 'land':
                for m in (current, previous):
                    m.enter(5) # Queued gap cannot restore.
                    m.enter(0x8a if ladder else 0xb4)
                    m.stick(0.,0.)
                    m.write(m.player+0x12f8, 0, 1)
                    m.enter_idle(); m.enter(5) # Restoration is once only.
                assert current.action_requests == previous.action_requests
            for attr in ('vault_calls','prepare_calls','equipment_calls','parameter_calls','commit_calls'):
                assert getattr(current, attr) == getattr(previous, attr), (attr, ladder, reason)
            cases += 1
        for faster in (False,True):
            current = HurryMachine(base, faster_dialogue=faster)
            with patch.object(Path, 'read_bytes', saved_bytes): previous = HurryMachine(base, faster_dialogue=faster)
            for buttons, reveal, choice, timer, message, bank in itertools.product((0,16), (1,2,3,4), (0,1), (-1,0,2), (1,3), ('','SYS_Get_Fish')):
                for m in (current, previous):
                    m.prepare(buttons=buttons, reveal=reveal, choice_phase=choice, timer=timer, message_phase=message)
                    m.message_id(bank, '001')
                assert current.advance() == previous.advance()
                for attr in ('request','page_clears','notifications'): assert getattr(current,attr) == getattr(previous,attr)
                cases += 1
    print(f'PASS: {cases} current/baseline Controls comparisons at two ASLR bases; pickup events, swimming inputs/speeds, camera eligibility, traversal queries/requests, borrow cancellation/restoration, hurry native transitions')

if __name__ == '__main__': main()
