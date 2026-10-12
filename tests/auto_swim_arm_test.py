"""Compiled swim hooks plus native stroke branches at two relocated bases.

Loaded parameter resources are modeled. Native animation/collision scheduling
and continuous stroke cadence require a live Eden playtest.
"""
import struct
from core_arm_test import Machine, DEFAULT, RAW
from unicorn.arm64_const import *

SURFACE, UNDERWATER, PARAMETERS = 0x2367e3c, 0x236d558, 0x241e98c
CONFIG = DEFAULT + b'hold_to_hoover=false\nhold_to_advance_dialogue=false\nauto_vault=false\nauto_ladder=false\n'

class SwimMachine(Machine):
    def __init__(self, base, **kwargs):
        self.ready = False
        super().__init__(base, {'controls': CONFIG}, **kwargs)
        self.boot(); self.activate()
        assert len(self.hooks) == 3 and 'auto swim: READY' in self.text, self.text
        self.player = self.ram + 0x1000
        self.context = self.ram + 0x4000
        self.parameters = self.ram + 0x5000
        self.vtable = self.ram + 0x6000
        self.getter = self.ram + 0x7000
        self.value = self.ram + 0x8000
        self.write(self.parameters + 0x120, self.vtable)
        self.write(self.vtable + 0xf0, self.getter)
        self.u.mem_write(self.value, struct.pack('<f', .2))
        self.ready = True
    def service(self, u, pc, size, context):
        if self.ready and pc in (self.base + PARAMETERS, self.getter):
            assert self.reg(0) == (1 if pc == self.base + PARAMETERS else self.parameters + 0x120)
            u.reg_write(UC_ARM64_REG_X0, self.parameters if pc == self.base + PARAMETERS else self.value)
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
            return
        return super().service(u, pc, size, context)
    def stroke(self, underwater, x, y, physical=0, enabled=1, mode=0):
        self.write(self.player + 0x178, mode, 4)
        self.write(self.player + 0x12f8, enabled, 1)
        self.u.mem_write(self.player + 0xeec, struct.pack('<ff', x, y))
        # Prove the entire player/input snapshot is untouched by the hook.
        before = bytes(self.u.mem_read(self.player, 0x3000))
        self.u.mem_write(self.context, bytes(0x100))
        self.write(self.context + 19 * 8, self.player)
        register = 8 if underwater else 9
        self.write(self.context + register * 8, physical)
        self.u.reg_write(UC_ARM64_REG_X0, self.context)
        if not underwater:
            self.call(self.module.find('AutoSwimSurface', 'Callback'))
        else:
            assert self.base + UNDERWATER not in self.hooks
            assert bytes(self.u.mem_read(self.base + UNDERWATER, 20)) == RAW[UNDERWATER:UNDERWATER+20]
        result = self.read(self.context + register * 8)
        assert bytes(self.u.mem_read(self.player, 0x3000)) == before
        # Execute the game's actual A branch. A taken branch lands at the
        # native stroke request construction; idle continues the original path.
        gate = UNDERWATER if underwater else SURFACE
        self.u.mem_write(self.base + gate, RAW[gate:gate+4])
        self.u.reg_write(UC_ARM64_REG_X8 if underwater else UC_ARM64_REG_X9, result)
        self.u.emu_start(self.base + gate, self.base + gate + 4, count=1)
        target = 0x236d584 if underwater else 0x2367ee8
        assert (self.u.reg_read(UC_ARM64_REG_PC) == self.base + target) == bool(result & 1)
        if result & 1:
            self.u.reg_write(UC_ARM64_REG_X19, self.player)
            self.u.reg_write(UC_ARM64_REG_X29, self.ram + 0xa000)
            self.u.reg_write(UC_ARM64_REG_X23, self.player + 0x1980)
            if not underwater:
                self.u.reg_write(UC_ARM64_REG_X8, self.player + 0x1980)
                self.u.reg_write(UC_ARM64_REG_X1, 1)
            self.u.emu_start(self.base + target, self.base + 0x3dbb160, count=200)
            assert self.u.reg_read(UC_ARM64_REG_PC) == self.base + 0x3dbb160
            assert self.reg(1) == (0xfb if underwater else 0xf7)
            assert self.read(self.player + 0x1880, 1) == 1
            assert bytes(self.u.mem_read(self.player + 0xec0, 0x50)) == before[0xec0:0xf10]
        return result


def main():
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        m = SwimMachine(base)
        for underwater in (False, True):
            for x, y, moving in ((0,0,False), (.1,.1,False), (.2,0,False), (.21,0,True),
                                  (1,0,True), (-1,0,True), (0,1,True), (0,-1,True),
                                  (.15,.15,True), (1,1,True), (float('nan'),0,False), (float('inf'),0,False)):
                for enabled, mode in ((1,0), (0,0), (1,1)):
                    for physical in (0, 1, 0x80):
                        assert m.stroke(underwater,x,y,physical,enabled,mode) == (physical | int(not underwater and moving and enabled and mode != 1))
                        cases += 1
            for _ in range(30):
                assert m.stroke(underwater,1,0) == int(not underwater)
                assert m.stroke(underwater,0,0) == 0
        for settings, custom, expected in ((CONFIG,None,True), (CONFIG,b'auto_swim=false',False),
                                           (CONFIG+b'auto_swim=false\n',b'auto_swim=true',True),
                                           (CONFIG+b'auto_swim=false\n',None,False)):
            files={'controls':settings}
            if custom is not None: files['controls.custom']=custom
            n=Machine(base,files); n.boot(); n.activate()
            assert (base+SURFACE in n.hooks) == expected
            assert base+UNDERWATER not in n.hooks
        for offset in (SURFACE, PARAMETERS):
            n=Machine(base,{'controls':DEFAULT})
            n.write(base+offset,0xdeadbeef,4); n.boot(); n.activate()
            assert 'auto swim: DISABLED' in n.text
            assert all(base+a not in n.hooks for a in (SURFACE,UNDERWATER))
            assert base+0x2222a34 in n.hooks  # Independent pickup still installs.
        n=Machine(base,{'controls':CONFIG})
        n.write(base+UNDERWATER,0xdeadbeef,4); n.boot(); n.activate()
        assert base+SURFACE in n.hooks and 'auto swim: READY' in n.text
        assert base+UNDERWATER not in n.hooks
    print(f'PASS: {cases} surface auto strokes/underwater manual strokes, deadzone, release, physical A, input guards, overrides and conflicts')

if __name__ == '__main__': main()
