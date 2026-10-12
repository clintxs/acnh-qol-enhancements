"""Compiled analog swimming, boost cooldown and native animation selectors.

Native world updates, loaded speed parameters and animation timing are modeled;
real sound, collision, creature catches and perceived smoothness need Eden.
"""
import struct
import math
from core_arm_test import Machine, DEFAULT
from unicorn.arm64_const import *

BINDINGS = (0x236b77c,0x236bea0,0x236bbb8,0x236cf20,0x236db00,0x236d6a0,0x2364b10,0x2365878)
CONFIG = DEFAULT + b'hold_to_hoover=false\nhold_to_advance_dialogue=false\nauto_vault=false\nauto_ladder=false\n'
TICKS = 19_200_000

class SwimMachine(Machine):
    def __init__(self, base, boost=True, custom=None):
        self.ready = False
        files = {'controls':CONFIG + (b'' if boost else b'swim_stroke_boost=false\n')}
        if custom is not None: files['controls.custom'] = custom
        super().__init__(base, files)
        self.boot(); self.activate()
        assert len(self.hooks) == 11 and all(base+a in self.hooks for a in BINDINGS), self.text
        self.player = self.ram + 0x1000
        self.parameters = self.ram + 0x5000
        self.tick = 100*TICKS
        self.now = self.module.find('svcGetSystemTick')
        self.stubs = {}
        self.samples = []
        self.outer_returns = []
        self.nest_entry = False
        self.arm_animation = False
        self.movement_results = []
        for i, name in enumerate(('SurfaceWait','SurfaceStride','SurfaceEntry','DiveWait','DiveStride','DiveEntry','WaitMovement','StrideMovement')):
            stub = self.ram + 0x10000 + i*16
            self.stubs[stub] = name
            self.write(self.module.find('AutoSwim'+name,'OrigRef'),stub)
        self.write(self.player+0x230,123,4)
        self.write(self.player+0x12f8,1,1)
        self.u.mem_write(self.parameters,struct.pack('<f',.8))
        self.ready = True
        self.native_return = self.ram + 0x11000
    def float(self, offset):
        return struct.unpack('<f',self.u.mem_read(self.player+offset,4))[0]
    def service(self,u,pc,size,context):
        if self.ready and pc == self.now:
            u.reg_write(UC_ARM64_REG_X0,self.tick)
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR)); return
        if self.ready and pc == self.native_return:
            self.movement_results.append(u.reg_read(UC_ARM64_REG_X0))
            u.reg_write(UC_ARM64_REG_PC,self.outer_returns.pop()); return
        if not self.ready or pc not in self.stubs:
            return super().service(u,pc,size,context)
        name=self.stubs[pc]
        if name.endswith('Movement'):
            # Model the native animation decelerating to zero. Compiled scoped
            # policy must replace this curve with analog speed and boost decay.
            for offset in (0x1820,0x1844): self.u.mem_write(self.player+offset,struct.pack('<f',.1))
            self.write(self.player+0x1854,0x12345678,4)  # Native steering result.
            u.reg_write(UC_ARM64_REG_X0,3)
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR)); return
        self.samples.append((name,self.read(self.player+0xec8),self.read(self.player+0xed0),self.read(self.player+0x13d8,4)))
        if name.endswith('Entry'): self.arm_animation=self.read(self.player+0x13d8,4)<=30
        # Real native stride callers scale slot zero by .8 for FlapFoot.
        self.u.mem_write(self.parameters,struct.pack('<f',1. if self.arm_animation else .8))
        self.outer_returns.append(u.reg_read(UC_ARM64_REG_LR))
        u.reg_write(UC_ARM64_REG_X1,self.parameters)
        u.reg_write(UC_ARM64_REG_LR,self.native_return)
        if self.nest_entry and name.endswith('Stride'):
            self.nest_entry=False
            target=name.replace('Stride','Entry')
        else:
            target='StrideMovement' if 'Stride' in name else 'WaitMovement'
        u.reg_write(UC_ARM64_REG_PC,self.module.find('AutoSwim'+target,'Callback'))
    def update(self,name,x=1.,y=0.,held=0,pressed=0,mode=0,enabled=1,locked=0,resident=0):
        self.write(self.player+0x178,mode,4)
        self.write(self.player+0x12f8,enabled,1)
        self.write(self.player+0x17ac,locked,1)
        self.write(self.player+0x1ee8,resident,1)
        self.u.mem_write(self.player+0xeec,struct.pack('<ff',x,y))
        self.write(self.player+0xec8,held)
        self.write(self.player+0xed0,pressed)
        self.write(self.player+0x13d8,7,4)
        before=bytes(self.u.mem_read(self.player+0xec0,0x50))
        self.u.reg_write(UC_ARM64_REG_X0,self.player)
        self.call(self.module.find('AutoSwim'+name,'Callback'))
        assert bytes(self.u.mem_read(self.player+0xec0,0x50)) == before
        assert self.read(self.player+0x13d8,4) == 7
        assert self.read(self.player+0x1854,4) == 0x12345678  # Steering preserved.
        return self.samples[-1][1:]
    def animation(self,underwater,frames):
        self.write(self.player+0x13d8,frames,4)
        self.u.reg_write(UC_ARM64_REG_X19,self.player)
        self.u.reg_write(UC_ARM64_REG_SP,self.ram+0x1f0000)
        start,end=(0x236d780,0x236d7b8) if underwater else (0x236bc90,0x236bcc8)
        self.u.emu_start(self.base+start,self.base+end,count=50)
        expected=(0x3f92b12 if underwater else 0x3f8d6b9) if frames>30 else (0x3fbbf29 if underwater else 0x3ff028a)
        assert self.u.reg_read(UC_ARM64_REG_X8) == self.base+expected
    def stride_gate(self,underwater,pending):
        # Real native compare/branch immediately after StrideMovement. An
        # ordinary return of 3 skips A; a pending boost's 0 must reach A now.
        start,end=(0x236def4,0x236defc) if underwater else (0x236c214,0x236c21c)
        self.u.reg_write(UC_ARM64_REG_W0,self.movement_results[-1])
        self.u.emu_start(self.base+start,self.base+end,count=2)
        assert (self.u.reg_read(UC_ARM64_REG_PC)==self.base+end)==pending

    def held_gate(self,underwater,held):
        gate=0x236e198 if underwater else 0x2367e64
        self.u.reg_write(UC_ARM64_REG_X8 if underwater else UC_ARM64_REG_X9,held)
        self.u.emu_start(self.base+gate,self.base+gate+4,count=1)
        target=0x236dfe8 if underwater else 0x2367e74
        assert (self.u.reg_read(UC_ARM64_REG_PC)==self.base+target) == (not bool(held&1))


def main():
    cases=0
    for base, boost in ((base, boost) for base in (0x7100000000,0x7200000000) for boost in (False,True)):
        m=SwimMachine(base, boost)
        for underwater in (False,True):
            prefix='Dive' if underwater else 'Surface'
            # Reset identity for each independent stick case. This tests radial
            # analog scaling, dead zone, diagonal clamping and invalid inputs.
            for x,y in ((0,0),(.1,0),(.15,0),(.2,0),(.5,0),(.85,0),(1,0),
                        (-1,0),(0,1),(0,-1),(.5,.5),(.8,.8),
                        (float('nan'),0),(float('inf'),0),(2,0)):
                m.write(m.player+0x230,m.read(m.player+0x230,4)+1,4)
                m.arm_animation=False
                held,pressed,frames=m.update(prefix+'Stride',x,y,held=0x80,pressed=0x40)
                squared=x*x+y*y
                throttle=(min(math.sqrt(squared),1)-.15)/.85 if .15**2<squared<=2 else 0.
                active=throttle>0
                assert (held,pressed,frames)==(0x80|active,0x40|active,0 if active else 31)
                assert m.movement_results[-1]==(0 if active else 3)
                m.stride_gate(underwater,active)
                assert abs(m.float(0x1820)-.8*throttle)<1e-6
                assert m.float(0x1820)==m.float(0x1844)
                m.animation(underwater,frames)
                m.held_gate(underwater,held)
                cases+=1
            # Opening stroke stays pending through a delayed native gate.
            m.write(m.player+0x230,m.read(m.player+0x230,4)+1,4)
            m.arm_animation=False
            assert m.update(prefix+'Stride') == (1,1,0)
            for frame in range(20):
                m.tick+=TICKS//30
                assert m.update(prefix+'Stride') == (1,1,0)
            assert m.update(prefix+'Entry') == (1,0,0)
            assert m.update(prefix+'Stride') == (1,0,31)
            assert abs(m.float(0x1820)-.8)<1e-6  # Cruise only during arm wind-up.
            # No further automatic arm strokes, even over several deadlines.
            # A short tap during cooldown is ignored, never queued for later.
            for frame in range(1,181):
                m.tick+=TICKS//30
                pressed=1 if frame==10 else 0
                assert m.update(prefix+'Stride',pressed=pressed)==(1,0,31)
                envelope=max(0.,1-(frame-15)/45) if frame>=15 else 0.
                assert abs(m.float(0x1820)-(.8+.5*envelope*boost))<1e-6
                if frame in (30,90,150):
                    assert m.update(prefix+'Entry')==(1,0,31)
                cases+=1
            # A single ready tap latches until native entry, including after
            # release, and starts the next cooldown only on actual entry.
            assert m.update(prefix+'Stride',pressed=1)==(1,1,0)
            for frame in range(16):
                m.tick+=TICKS//30
                assert m.update(prefix+'Stride')==(1,1,0)
            assert m.update(prefix+'Entry')==(1,0,0)
            # Holding A repeats at each cooldown boundary, with quiet kicking
            # throughout the remaining interval. Speed remains independent of
            # the arm/foot animation and proportional to current stick travel.
            for cycle in range(3):
                for frame in range(1,61):
                    m.tick+=TICKS//30
                    expected=(1,1,0) if frame==60 else (1,0,31)
                    assert m.update(prefix+'Stride',x=.575,held=1)==expected
                    assert m.movement_results[-1]==(0 if frame==60 else 3)
                    m.stride_gate(underwater,frame==60)
                    envelope=max(0.,1-(frame-15)/45) if frame>=15 else 0.
                    assert abs(m.float(0x1820)-(.4+.5*envelope*boost))<1e-6
                    cases+=1
                assert m.update(prefix+'Entry',x=.575,held=1)==(1,0,0)
                assert m.update(prefix+'Wait',x=.575,held=1)==(1,0,31)
                assert abs(m.float(0x1820)-.4)<1e-6  # Cruise continues during wind-up.
            # Ready strokes bypass an unfinished native kick (result 3) at
            # varying update rates. Model FSM entry on the following update.
            # There must be no early strokes and no animation-length delays.
            previous_entry=m.tick
            accepted=0
            steps=(TICKS//60,TICKS//30,TICKS//24,TICKS//20)
            index=0
            while accepted<5:
                step=steps[index%len(steps)]; index+=1
                m.tick+=step
                _,pressed,_=m.update(prefix+'Stride',held=1)
                pending=bool(pressed&1)
                m.stride_gate(underwater,pending)
                if pending:
                    m.tick+=TICKS//60
                    assert m.update(prefix+'Entry',held=1)==(1,0,0)
                    assert 2*TICKS<=m.tick-previous_entry<=2*TICKS+max(steps)+TICKS//60
                    previous_entry=m.tick
                    accepted+=1
                cases+=1
            # Stop/start cannot bypass the shared stroke cooldown.
            m.update(prefix+'Wait',0,0)
            assert m.update(prefix+'Stride')==(1,0,31)
            # After the impulse fades, neutral stops; a ready neutral A stroke
            # gives a small forward impulse along the native facing direction.
            for frame in range(60):
                m.tick+=TICKS//30
                m.update(prefix+'Wait',0,0)
            assert m.float(0x1820)==m.float(0x1844)==0
            assert m.update(prefix+'Wait',0,0,pressed=1)==(1,1,0)
            assert m.update(prefix+'Entry',0,0)==(1,0,0)
            m.update(prefix+'Stride',0,0)
            assert m.float(0x1820)==0  # Neutral stroke wind-up has no impulse.
            for frame in range(1,61):
                m.tick+=TICKS//30
                held,_,_=m.update(prefix+'Stride',0,0)
                envelope=max(0.,1-(frame-15)/45) if frame>=15 else 0.
                assert abs(m.float(0x1820)-.5*envelope*boost)<1e-6
                assert held==int(frame<60)
                cases+=1
            # Y takes priority over automatic/manual arm requests.
            assert m.update(prefix+'Stride',pressed=9)==(0,8,31)
            # Guarded players retain all original input and speed handling.
            for kwargs in ({'mode':1},{'enabled':0},{'locked':1},{'resident':8}):
                assert m.update(prefix+'Stride',.5,0,held=1,pressed=1,**kwargs)==(1,1,7)
                assert abs(m.float(0x1820)-.1)<1e-6
                cases+=1
            # Nested entry must not confuse borrowed held A with physical A.
            m.write(m.player+0x230,m.read(m.player+0x230,4)+1,4)
            m.nest_entry=True
            m.update(prefix+'Stride')
            assert m.samples[-1][0]==prefix+'Entry'
            assert m.update(prefix+'Stride')==(1,0,31)
        # Diving/surfacing must preserve an existing boost cooldown.
        assert m.update('SurfaceStride',held=1)==(1,0,31)
        for files in ({'swimming':DEFAULT},
                      {'controls':CONFIG.replace(b'enabled=true',b'enabled=false')},
                      {'controls.custom':b'enabled=true\nauto_swim=true'}):
            n=Machine(base,files); n.boot(); n.activate()
            assert all(base+a not in n.hooks for a in BINDINGS)
        # No scope: other callers of shared movement keep native speeds.
        m.u.reg_write(UC_ARM64_REG_X0,m.player)
        m.u.reg_write(UC_ARM64_REG_X1,m.parameters)
        m.call(m.module.find('AutoSwimWaitMovement','Callback'))
        assert abs(m.float(0x1820)-.1)<1e-6
        for regular,custom,expected in ((CONFIG,None,True),(CONFIG,b'auto_swim=false',False),
                                        (CONFIG+b'auto_swim=false\n',b'auto_swim=true',True)):
            files={'controls':regular}
            if custom: files['controls.custom']=custom
            n=Machine(base,files); n.boot(); n.activate()
            assert all((base+a in n.hooks)==expected for a in BINDINGS)
        # Both INI layers use the same boost preference; custom settings can
        # turn it off or restore it without changing stroke scheduling.
        for regular_boost, custom, expected_boost in (
            (True, b'swim_stroke_boost=false', False),
            (False, b'swim_stroke_boost=true', True),
            (False, b'swim_stroke_boost=true\nswim_stroke_boost=false', False),
            (False, b'swim_stroke_boost=bad', False),
        ):
            n=SwimMachine(base, regular_boost, custom)
            assert n.update('DiveStride') == (1,1,0)
            assert n.update('DiveEntry') == (1,0,0)
            n.tick += TICKS//2
            assert n.update('DiveStride') == (1,0,31)
            assert abs(n.float(0x1820)-(.8+.5*expected_boost)) < 1e-6
        for offset in BINDINGS:
            n=Machine(base,{'controls':DEFAULT})
            n.write(base+offset,0xdeadbeef,4); n.boot(); n.activate()
            assert 'auto swim: DISABLED' in n.text and all(base+a not in n.hooks for a in BINDINGS)
            assert base+0x2222a34 in n.hooks
    print(f'PASS: {cases} analog stick speed, opening stroke only, manual tap/hold cooldown, interruptible native stride gates, irregular-rate cadence, delayed entries, half-second wind-up and 1.5-second 62.5% decaying impulse, cosmetic strokes without propulsion, neutral boost, native steering, scope restoration, Y priority, surface/dive cruise, Controls config, ignored retired Swimming files and conflicts')

if __name__=='__main__': main()
