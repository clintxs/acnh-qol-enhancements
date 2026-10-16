"""Regression for the Switch cursor-target/pane type confusion.

Crash build 8B64CD25 faults at Core+B4E8: ldr x2,[x2,#18], with x2=1.
Use a live-row target whose non-pane +18 chain reaches 1. The preserved old
ELF can replay the fault; the current build must tap without reading that chain.
Native animation playback is modeled, as in feedback_arm_test.py.
"""
from pathlib import Path
from unittest.mock import patch
from unicorn import UcError, UC_HOOK_MEM_INVALID, UC_PROT_NONE
from unicorn.arm64_const import UC_ARM64_REG_PC
from feedback_arm_test import FeedbackMachine

BASELINE=Path(__file__).resolve().parents[1]/'dist/switch-startup-fix/original/project.elf'

def nonpane_target(machine):
    target=machine.root+0x7000
    parent=machine.root+0x7100
    machine.write(target+0x18,parent)
    machine.write(parent+0x18,1)
    return target

if BASELINE.exists():
    baseline=BASELINE.read_bytes()
    read_bytes=Path.read_bytes
    def old_module(path):
        if path.name=='project.elf' and path.parent==Path(__file__).resolve().parents[1]: return baseline
        return read_bytes(path)
    for base in (0,0x7100000000):
        with patch.object(Path,'read_bytes',old_module):
            old=FeedbackMachine(base)
        nonpane_target(old)
        if base==0: old.u.mem_protect(0,4096,UC_PROT_NONE)
        faults=[]
        def fault(u,access,address,size,value,data):
            faults.append((address,u.reg_read(UC_ARM64_REG_PC)))
            return False
        old.u.hook_add(UC_HOOK_MEM_INVALID,fault)
        try: old.toggle()
        except UcError: pass
        else: raise AssertionError('old build did not reproduce the cursor crash')
        assert faults==[(0x19,old.mod.address+0xb4e8)],faults
        print(f'PASS: old ELF reproduces Switch read 0x19 at Core+B4E8, base {base:#x}')
else:
    print('SKIP: optional preserved pre-fix ELF unavailable for crash replay')

for base in (0,0x7100000000):
    m=FeedbackMachine(base)
    target=nonpane_target(m)
    m.toggle()
    assert m.size()==1 and m.taps==m.prepares==1
    m.toggle()
    assert m.size()==0 and m.taps==m.prepares==2
    # Unmatched/null/small cursor identities must never be dereferenced.
    for unrelated in (0,1,m.root+0x7200):
        m.write(m.cursor+0x218,unrelated)
        m.toggle()
        assert m.taps==m.prepares==2
    # Matching target with invalid non-pane fields still works.
    m.write(m.cursor+0x218,target)
    m.write(target+0x18,1)
    m.toggle()
    assert m.taps==m.prepares==3
    # No target for this row: skip feedback without disturbing bag behavior.
    m.write(m.root+0x5218,0)
    m.toggle()
    assert m.taps==m.prepares==3
    print(f'PASS: compiled add/remove with non-pane cursor target, invalid parent payload and null/unrelated identities, base {base:#x}')
