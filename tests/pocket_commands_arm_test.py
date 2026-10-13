"""Extend the preserved pocket regressions with stage 4 service boundaries.

All native item/menu/timing assertions still execute through compiled callbacks.
The inherited fixture models registration and engine services, not live Eden.
"""
from pathlib import Path
import runpy
from core_arm_test import Machine
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1

for base in (0x7100000000, 0x7200000000):
    for rows in ([], [(0x80, 0), (0x80, 1)], [(0x480, 0)], [(0x80, 3)], [(0x80, 99)]):
        m = Machine(base, {})
        data = m.ram + 0x1000
        for index, (button, action) in enumerate(rows):
            m.write(data + index * 8, button, 4)
            m.write(data + index * 8 + 4, action, 4)
        m.u.reg_write(UC_ARM64_REG_X0, data)
        m.u.reg_write(UC_ARM64_REG_X1, len(rows))
        m.call(m.module.find('InstallShortcuts'))
        assert m.reg(0) == 0 and not m.hooks and not m.opened
print('PASS: invalid shortcut registrations decline before native publication at two bases')
runpy.run_path(str(Path(__file__).with_name('arm_stack_test.py')),
               init_globals={'CHECK_POCKET_COMMANDS': True})
