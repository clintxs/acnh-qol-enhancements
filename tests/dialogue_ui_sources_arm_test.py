"""Exercise all native UI-result item source paths, including the crafting bug.

The actual ARM argument setup determines source_item. A decoy uncataloged item
257 in SP+8 reproduces 0.8.0 incorrectly starring a known crafted item.
"""
from dialogue_item_stars_arm_test import Dialogue, SITES
from unicorn.arm64_const import *

count=0
for base in (0x7100000000,0x7200000000):
    q=Dialogue(base)
    for mode in (4,19,21,23,24,49):
        q.select(SITES[3]);stack=q.frame-0x1860
        q.write(q.frame-4,mode,4)
        q.write(stack+8,257) # Unrelated scratch bytes: these must not determine the marker.
        q.u.reg_write(UC_ARM64_REG_SP,stack)
        q.u.reg_write(UC_ARM64_REG_X29,q.frame)
        if mode==19:
            # Recipe output is converted to an item, then copied from SP+158
            # to FP-68 before reusing SP+158 as the native name object.
            q.write(stack+0x158,3084)
            q.u.reg_write(UC_ARM64_REG_X22,q.frame-0x68)
            q.u.emu_start(base+0xc5e0cc,base+0xc5e134)
        elif mode in (4,23,49):
            q.u.reg_write(UC_ARM64_REG_X21,q.item)
            q.write(q.ctx+21*8,q.item)
            q.u.emu_start(base+0xc5ddac,base+0xc5e134)
        else:
            q.u.emu_start(base+0xc5e124,base+0xc5e134)
        q.name,q.source_item=q.reg(0),q.reg(1)
        assert q.name==stack+0x158
        q.check_name(3084,owned=True);count+=1
        q.check_name(3084,'★');count+=1
        # Reverse the status to ensure the decoy also cannot suppress a real star.
        if mode not in (21,24):
            q.write(stack+8,0xfffe)
            q.check_name(3084,'★');count+=1
        q.write(q.frame-4,63,4)
        q.check_name(3084);count+=1
print(f'PASS: {count} UI-result source cases with native setup, misleading scratch data and unknown-tag guard')
