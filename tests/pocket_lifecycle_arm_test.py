"""Bounded Play-entry notification, idempotency and pre-publication capacity."""
from core_arm_test import Machine
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_X0

for base in (0x7100000000, 0x7200000000):
    m = Machine(base, {})
    def call(name, argument):
        m.u.reg_write(UC_ARM64_REG_X0, argument)
        m.call(m.module.find(f'{len(name)}{name}'))
        return m.reg(0)
    first, second, owner = m.ram + 0x1000, m.ram + 0x1100, m.ram + 0x2000
    m.u.mem_write(first, b'\xc0\x03\x5f\xd6')
    seen = []
    m.u.hook_add(UC_HOOK_CODE, lambda u, pc, size, data: seen.append(m.reg(0)) if pc == first else None)
    assert call('RegisterBeforePlayEntry', 0) == 0
    assert call('RegisterBeforePlayEntry', first) == 1
    assert call('RegisterBeforePlayEntry', first) == 1
    assert call('RegisterBeforePlayEntry', second) == 0
    call('BeforePlayEntry', owner)
    assert seen == [owner]
    # A different listener reserves the sole slot: HoldSelect must decline
    # before installing its observer/input/feedback behavior.
    assert call('InstallHoldSelect', 0) == 0 and not m.hooks
print('PASS: one bounded Play reset listener, identical reuse, conflict rejection and dependent publication failure at two bases')
