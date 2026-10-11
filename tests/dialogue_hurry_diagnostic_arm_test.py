"""Diagnostic logs must expose blocked Y with shipping logging defaults off."""
import re
from dialogue_hurry_arm_test import HurryMachine
from unicorn.arm64_const import *


def lines(m, stage):
    return [line for line in m.text.splitlines() if f'HURRY stage={stage} ' in line]


for base in (0x7100000000, 0x7200000000):
    m = HurryMachine(base, verbose=False)
    assert 'DIAGNOSTIC: hurry-v1' in m.text
    assert 'log_debug=false' in m.text and 'BOOT_OK' not in m.text
    # Every shared gate must report Y even though ordinary speed is retained.
    for args, gate in ((dict(state=4), 'manager-state'),
                       (dict(context_phase=1), 'context-phase'),
                       (dict(window=3), 'window'),
                       (dict(choice_phase=2), 'choice-active')):
        m.prepare(**({'state': 3} | args))
        result, _ = m.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
        row = lines(m, 'speed')[-1]
        assert f'gate={gate} ' in row and 'buttons=0010 y=1 controller=1' in row, row
        assert result[0x21b7] != 5
    for args, gate in ((dict(state=3), 'manager-state'),
                       (dict(context_phase=1), 'context-phase'),
                       (dict(choice_phase=1), 'choice-active'),
                       (dict(reveal=1), 'text-not-ready'),
                       (dict(timer=60), 'scripted-timer'),
                       (dict(message_phase=3), 'message-transition')):
        m.prepare(**args); m.advance()
        row = lines(m, 'advance')[-1]
        assert f'gate={gate} ' in row and 'y=1' in row and 'result=0' in row, row
    # Input logs run before the native reveal gates, including protected text.
    m.prepare(state=3)
    m.write(m.obj + 0x21b5, 1, 1); m.write(m.obj + 0x21b6, 1, 1)
    m.input(0x10)
    row = lines(m, 'input')[-1]
    assert 'buttons=0010 y=1' in row and 'forced=1 disabled=1' in row, row
    assert m.read(m.obj + 0x21b4, 1) == 0 # Native B/R input byte unchanged by Y.
    m.input(0)
    assert 'buttons=0000 y=0' in lines(m, 'input')[-1]
    for getter_null, fallback_null, present in ((True, False, 1), (True, True, 0)):
        m.input(0x10, getter_null=getter_null, fallback_null=fallback_null)
        row = lines(m, 'input')[-1]
        assert f'controller={present} ' in row and f'y={present} ' in row, row
    m.prepare()
    assert m.advance() == 7
    assert 'gate=ready buttons=0010 y=1' in lines(m, 'advance')[-1]
    assert 'result=1' in lines(m, 'advance')[-1]

    # Duplicate frames are quiet, and an exhausted unheld budget cannot hide Y.
    budget = HurryMachine(base, verbose=False)
    for state in range(110):
        budget.prepare(buttons=0, state=state)
        budget.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
    assert len(lines(budget, 'speed')) == 96
    budget.prepare(state=3)
    for _ in range(4): budget.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
    assert len(lines(budget, 'speed')) == 97
    assert 'y=1' in lines(budget, 'speed')[-1]
    for state in range(110):
        budget.prepare(state=state)
        budget.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
    assert len(lines(budget, 'speed')) == 192
    # Long identifiers still fit the native logger's 512-byte buffer.
    long = HurryMachine(base, verbose=False)
    long.prepare(state=0xffffffff, window=0xffffffff, context_phase=0xffffffff,
                 reveal=0xffffffff, choice_phase=0xffffffff, message_phase=0xffffffff, timer=-2147483648)
    long.write(long.meta + 0x24f8, long.ram + 0x6d100)
    long.u.mem_write(long.ram + 0x6d000, b'A' * 63 + b'\0')
    long.u.mem_write(long.ram + 0x6d100, b'B' * 63 + b'\0')
    long.input(0x10)
    assert re.search(r'bank=A{63} label=B{63}\n', long.text), long.text
    print(f'PASS {base:#x}: live Y evidence before rejected/protected gates; no INI edit, bounded held/unheld logs and intact long IDs')
