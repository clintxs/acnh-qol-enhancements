"""Native X ticket dispatch and Y bag separation at two ASLR bases.

Original button-unit dispatch executes ARM. Its event-emission helper is
modeled; real ticket assignment/removal and rendered glyphs need a live test.
"""
from fitting_hint_arm_test import HintMachine
from runtime_arm_test import Machine
from unicorn.arm64_const import *

class TicketMachine(HintMachine):
    def __init__(self,base):
        super().__init__(base)
        self.events=[]
        self.ticket=True
        self.invoke(base+0x3114c3c,self.owner,0)
        self.unit=self.read(self.read(self.page+0x580)+0xa0)
        self.snapshot=self.ram+0x70000
        self.records=self.snapshot+0x100
        self.repeat=self.snapshot+0x300
        self.write(self.snapshot+8,3,4);self.write(self.snapshot+0x10,self.records)
        self.write(self.repeat,15,4);self.write(self.repeat+4,3,4)
        self.write(self.unit+0x10,2,4)
        self.write(self.unit+0xbd,1,1)
    def service(self,u,pc,size,data):
        if pc==self.base+0x3c4b514:
            assert self.reg(0)==self.unit
            self.events.append(self.reg(1))
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR));return
        super().service(u,pc,size,data)
    def dispatch(self,buttons,consumed=0,blocked=0,enabled=True):
        self.events=[]
        self.write(self.records+2*0x48,buttons,4)
        self.write(self.records+2*0x48+0x3c,consumed,4)
        self.write(self.unit+0xb8,0xffffffff,4)
        self.write(self.unit+0xbc,int(enabled),1)
        self.invoke(self.base+0x3c4b20c,self.unit,self.snapshot,self.repeat,blocked)
        return list(self.events)

for base in (0x7100000000,0x7200000000):
    m=TicketMachine(base)
    assert m.read(m.unit+0x14,4)==6
    assert 0 in m.dispatch(0x40)  # X emits native fresh-press event.
    assert m.read(m.records+2*0x48+0x3c,4)&0x40
    assert not m.dispatch(0x80)  # Unconsumed Y cannot trigger ticket action.
    assert 0 in m.dispatch(0x10040)  # Native accompanying input flag.
    assert not m.dispatch(0x40,consumed=0x40)
    assert not m.dispatch(0x40,blocked=1)
    assert not m.dispatch(0x40,enabled=False)
    # Wrong input row must not activate the selected resident's ticket unit.
    m.write(m.records,0x40,4)
    assert not m.dispatch(0)

    # Compiled private input adapter rebinds reconstructed units even before
    # a footer rebuild; Bag sees physical Y, never physical X.
    bag=Machine(base)
    button=bag.ram+0xc0000;unit=button+0x200
    bag.write(bag.page+0x580,button);bag.write(button+0xa0,unit)
    bag.write(unit+0x14,7,4)
    bag.input(0)
    assert bag.read(unit+0x14,4)==6
    bag.input(0x40);assert bag.size()==0
    bag.input(0);bag.input(0x80);assert bag.size()==1
    bag.input(0);bag.input(0x40);assert bag.size()==1
    bag.input(0);bag.input(0x80);assert bag.size()==0
    # Do not overwrite a third-party non-Y binding.
    bag.write(unit+0x14,9,4);bag.input(0)
    assert bag.read(unit+0x14,4)==9
    # This remap is restricted to the owning fitting page.
    bag.write(unit+0x14,7,4)
    bag.call(bag.mod.find('able_bag','L5InputE'),bag.page+0x100,bag.snapshot,0)
    assert bag.read(unit+0x14,4)==7
    bag.write(button+0xa0,0);bag.input(0)
    print(f'PASS: native X/consumption/player dispatch, Y bag add/remove, cached-unit remap, suppression and scope guards at {base:#x}')
