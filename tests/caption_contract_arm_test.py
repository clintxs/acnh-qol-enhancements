"""Owned caption fallback/bounds and deliberate invalid-adapter suppression."""
from fitting_hint_arm_test import HintMachine
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_PC, UC_ARM64_REG_LR

for base in (0x7100000000,0x7200000000):
    for text, expected in ((None,'Bag'), ('','Bag'), ('x'*128,'Bag'), ('x'*127,'x'*127)):
        m=HintMachine(base)
        data=m.ram+0x180000
        if text is not None:m.u.mem_write(data,text.encode('utf-16-le')+b'\0\0')
        provider=m.module.find('BagCaption')
        def caption(u,pc,size,state):
            if pc!=provider:return
            u.reg_write(UC_ARM64_REG_X0,0 if text is None else data)
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))
        m.u.hook_add(UC_HOOK_CODE,caption)
        m.caption()
        assert m.texts==[expected]
        for invalid in ('bad_adapter','bad_prepare','bad_payload'):
            m.caption(**{invalid:True})
            assert m.texts==[expected] and not m.text_events
print('PASS: missing/empty/overlength owned captions use bounded fallback; valid maximum text publishes; invalid setter/prepare/payload suppress nonexistent native translation without publication at two bases')
