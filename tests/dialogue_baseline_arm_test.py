"""Compare native reveal/input results and identity-query counts with a saved ELF.

QOL_BASELINE_ELF is optional, as in native_bindings_arm_test. Both modules run
production installation and the same original native code/real SIMD adapter.
No project executable is overwritten for comparison.
"""
import os
from pathlib import Path
from unittest.mock import patch
from dialogue_arm_test import DialogueMachine
from dialogue_hurry_arm_test import HurryMachine

baseline_path = os.environ.get('QOL_BASELINE_ELF')
if not baseline_path:
    print('SKIP: Dialogue baseline comparison; set QOL_BASELINE_ELF to a preserved executable')
else:
    baseline = Path(baseline_path).read_bytes()
    read_bytes = Path.read_bytes
    def baseline_bytes(path):
        return baseline if path.name == 'project.elf' and path.parent == Path(__file__).resolve().parents[1] else read_bytes(path)
    class CountIdentity(DialogueMachine):
        def __init__(self, *args, **kwargs):
            self.identity_calls = 0
            super().__init__(*args, **kwargs)
        def service(self, u, pc, size, user):
            if getattr(self, 'native', False) and pc == self.is_npc: self.identity_calls += 1
            return super().service(u, pc, size, user)
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        for base_speed, boost_speed in ((1, 1), (2, 4), (3, 9), (30, 30)):
            config = f'base_speed={base_speed}\nboost_speed={boost_speed}\nboost_npc_list=Timmy'.encode()
            current = CountIdentity(base, config, verbose=False)
            with patch.object(Path, 'read_bytes', baseline_bytes):
                previous = CountIdentity(base, config, verbose=False)
            for name, kind, window, source, stale, bank in (
                ('rcm', 10, 0, True, False, ''), ('unlisted', 10, 0, True, False, ''),
                ('rcm', 10, 0, True, True, ''), ('rcm', 10, 0, False, False, ''),
                ('', 0x4f, 0, True, False, ''), ('rcm', 10, 1, True, False, ''),
                ('rcm', 10, 2, True, False, ''), ('rcm', 10, 2, True, False, 'SYS_Get_Fish'),
                ('rcm', 10, 1, True, False, 'FTR_WorkingTable')):
                for machine in (current, previous):
                    machine.speaker(name, kind=kind, window=window, source=source, stale=stale)
                    machine.bank(bank, native=False)
                    # Native active bank is independent of the pending buffer.
                    machine.u.mem_write(machine.meta + 0x2494, bank.encode() + b'\0')
                for held in (0, 1, 0):
                    for fast in (0., .6, 2.4, 3.6):
                        current.fast = previous.fast = fast
                        current.identity_calls = previous.identity_calls = 0
                        actual = current.run((1, 0, 0, 0, 0, 1, 1, -1.), held)
                        expected = previous.run((1, 0, 0, 0, 0, 1, 1, -1.), held)
                        assert actual == expected, (base_speed, boost_speed, name, kind, window, source, stale, bank, held, fast)
                        assert current.identity_calls == previous.identity_calls
                        cases += 1
            for gate in ((0, 0, 0, 0), (1, 1, 0, 0), (1, 0, 1, 0), (1, 0, 0, 1)):
                for override in (-1., 0., .75):
                    case = (*gate, 9, 1, 1, override)
                    assert current.run(case, 1) == previous.run(case, 1)
                    cases += 1
            for buttons in (0, 2, 0x2000, 0x4000, 0x4002):
                for old_latch in (0, 1):
                    for fallback in (False, True):
                        for machine in (current, previous): machine.input(buttons, previous=old_latch, getter_null=fallback)
                        assert current.read(current.obj + 0x21b4, 1) == previous.read(previous.obj + 0x21b4, 1)
                        cases += 1
        for faster in (False, True):
            current = HurryMachine(base, faster_dialogue=faster)
            with patch.object(Path, 'read_bytes', baseline_bytes):
                previous = HurryMachine(base, faster_dialogue=faster)
            for buttons in (0, 0x10):
                for choice_phase in (0, 1):
                    for machine in (current, previous): machine.prepare(buttons=buttons, state=3, choice_phase=choice_phase)
                    assert current.run((1, 0, 0, 0, 0, 1, 1, -1.), 0) == previous.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
                    cases += 1
    print(f'PASS: {cases} current/baseline Dialogue comparisons; native input/reveal bytes/parser calls, identity-query frequency, dynamic rates, gates and shared hurry precedence')
