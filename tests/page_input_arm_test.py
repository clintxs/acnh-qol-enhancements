"""Production shared input ordering, consumption and cancellation contract."""
from core_arm_test import Machine
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_PC, UC_ARM64_REG_LR

for base in (0x7100000000, 0x7200000000):
    m = Machine(base, {})
    events, returns = [], {}
    def call(name, *args):
        for i, value in enumerate(args): m.u.reg_write(UC_ARM64_REG_X0 + i, value)
        m.call(m.module.find(name))
        return m.reg(0)
    addresses = {name:m.ram + 0x1000 + i*0x100 for i,name in enumerate(('late','first','middle','cancel-late','cancel-middle','before','after','native'))}
    def observe(u, pc, size, data):
        if pc not in addresses.values(): return
        name = next(name for name,address in addresses.items() if address == pc)
        events.append((name, m.reg(2)))
        u.reg_write(UC_ARM64_REG_X0, returns.get(name, 0))
        u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
    m.u.hook_add(UC_HOOK_CODE, observe)
    assert call('RegisterInputAdapter', addresses['late'], 50, addresses['cancel-late']) == 0
    assert call('RegisterInputAdapter', addresses['first'], 10, 0) == 0
    assert call('RegisterInputAdapter', addresses['middle'], 30, addresses['cancel-middle']) == 0
    assert call('RegisterInputAdapter', addresses['middle'], 30, addresses['cancel-middle']) == 1
    assert call('RegisterInputAdapter', addresses['middle'], 20, addresses['cancel-middle']) == 3
    assert call('RegisterObserverAdapter', addresses['before'], addresses['after'], 60, 0) == 0
    m.write(m.module.find('SharedPageInput', 's_FnPtr'), addresses['native'])
    def dispatch(blocked=0):
        events.clear()
        for index,value in enumerate((m.ram+0x4000,m.ram+0x5000,blocked)):
            m.u.reg_write(UC_ARM64_REG_X0+index,value)
        m.call(m.module.find('SharedPageInput','Callback'))
    dispatch()
    assert events == [(name,0) for name in ('first','middle','late','before','native','after')]
    returns['first'] = 1
    dispatch()
    assert events == [('first',0),('cancel-middle',0x80000001),('cancel-late',0x80000001),('before',1)]
    returns['first'] = 0
    dispatch(1)
    assert events == [(name,1) for name in ('first','middle','late','before','native','after')]
print('PASS: production input priority independent of registration order, conflict rejection, before/native/after ordering and suppression cancellation at two bases')
