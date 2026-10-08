"""Reproduce Miles' native binding selection, including a price binding first.

The real instructions at 03125aa4 choose slot 2 when the binding count exceeds
2. Earlier synthetic rows only populated slot 0, hiding this distinction.
"""
from catalog_badges_arm_test import Badges
from unicorn.arm64_const import UC_ARM64_REG_X19

cases = 0
for base in (0x7100000000, 0x7200000000):
    q = Badges(base)
    q.load_layout('Miles')
    for grouped in (False, True):
        q.use_group(grouped)
        for count in (1, 2, 3, 4):
            bindings = q.ui + 0x500
            q.write(q.row + 0x2d8, count, 4)
            q.write(bindings, 0)
            q.write(bindings + 16, 0)
            chosen = 2 if count > 2 else 0
            q.write(bindings + chosen * 8, q.ui + 0x600)
            # Run the unchanged native selection, rather than trusting our fixture.
            q.u.reg_write(UC_ARM64_REG_X19, q.row)
            q.u.emu_start(base + 0x3125aa4, base + 0x3125abc)
            assert q.reg(22) == q.ui + 0x600
            q.check('Miles', 4969, False, True)
            q.check('Miles', 4969, True, False)
            cases += 2
print(f'PASS: {cases} Miles name-binding cases with native slot selection')
