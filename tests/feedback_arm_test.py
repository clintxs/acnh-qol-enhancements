"""Compiled badge-curve and cursor feedback tests with modeled pane hierarchy."""
from runtime_arm_test import Machine
from unicorn.arm64_const import *
import struct
class FeedbackMachine(Machine):
    def __init__(self,base=0):
        super().__init__(base)
        self.root=self.ram+0x90000;self.pane=self.root+0x1000;self.cursor=self.root+0x2000
        self.anim=self.root+0x3000;self.taps=0;self.prepares=0;self.rendered_scale=None
        self.write(self.pane+0x28,self.pane+0x20) # empty child-list sentinel
        self.write(self.page+0x590,self.root+0x4000)
        self.write(self.root+0x4208,1,4);self.write(self.root+0x4210,self.root+0x4800)
        self.write(self.root+0x4800,self.root+0x5000)
        self.write(self.root+0x42e8,1,4);self.write(self.root+0x43cc,1,4)
        # Real row -> price binding -> pane parent -> named button hierarchy.
        # Do not stub row/target resolution: its old type assumption hid the crash.
        text=self.root+0xd000;button=self.root+0xe000
        binding=self.root+0xf000;adapter=self.root+0x10000;methods=self.root+0x11000
        self.write(self.root+0x52e0,binding);self.write(binding,binding+0x100)
        self.write(binding+0x148,adapter);self.write(adapter+8,text)
        self.write(adapter+0x18,methods);self.write(methods+0x5e0,base+0x3c1b588)
        self.write(text+0x18,self.root);self.write(text+0x28,text+0x20)
        self.write(self.root+0x28,button+8);self.write(button+0x18,self.root)
        self.write(button+0x10,self.root+0x20)
        self.u.mem_write(button+0xb0,b'B_Btn_00\0')
        self.write(base+0x4ab2230,self.root+0x6000);self.write(self.root+0x6000,self.root+0x6100)
        self.write(self.root+0x61d0,self.cursor);self.write(self.cursor,base+0x4a0ea80)
        self.write(self.cursor+0x218,self.root+0x7000)
        self.write(self.root+0x5218,self.root+0x7000)
        self.write(self.cursor+0x220,self.root+0x8000);self.write(self.root+0x8208,self.anim)
        self.write(self.anim,self.root+0x9000);self.write(self.anim+8,self.root+0xa000)
        self.write(self.anim+0x18,self.root+0xb000)
        self.write(self.root+0x9018,self.stub+0x900);self.write(self.root+0xb190,self.stub+0x920)
        self.services[self.stub+0x900]=('prepare',None);self.services[self.stub+0x920]=('tap',None)
    def service(self,u,pc,size,data):
        v=self.services.get(pc);result=None
        if v and v[0]=='import' and v[1]=='strncmp':
            def value(pointer): return bytes(u.mem_read(pointer,self.reg(2))).split(b'\0',1)[0]
            result=int(value(self.reg(0))!=value(self.reg(1)))
        elif v and v[0]=='prepare':
            assert self.read(self.reg(1)+8,4)==1;self.prepares+=1;result=0
        elif v and v[0]=='tap':
            assert self.reg(0)==self.root+0xa000
            assert u.reg_read(UC_ARM64_REG_S0)==0x3f800000
            self.taps+=1;result=0
        elif pc==self.base+0x27d1d0:result=self.pane
        elif pc==self.base+0x27d74c:
            self.rendered_scale=struct.unpack('<f',u.mem_read(self.reg(0)+0x48,4))[0];result=0
        if result is not None:
            u.reg_write(UC_ARM64_REG_X0,result);u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR));return
        super().service(u,pc,size,data)
    def badge(self):self.call(self.mod.find('L11UpdateBadge'),self.root+0x5000,self.item)
    def render(self):
        # Execute native dirty-transform propagation with no parent invalidation.
        self.call(self.base+0x27e460,self.pane,self.root+0xc000,0)
        assert self.rendered_scale==self.scale()
        assert not self.read(self.pane+0x58,1)&0x10
    def scale(self):return struct.unpack('<f',self.u.mem_read(self.pane+0x48,4))[0]
m=FeedbackMachine();m.toggle();assert m.taps==m.prepares==1
m.badge();m.render();assert m.scale()==.5 and m.read(m.pane+0x59,1)==0
for f in range(1,8):
    m.call(m.mod.find('4Play8Callback'),m.owner);m.badge();m.render()
    if f==3:assert abs(m.scale()-1.4)<.0001
    if f==6:assert abs(m.scale()-.9)<.0001
    if f==7:assert m.scale()==1
    assert m.read(m.pane+0x59,1)==255
m.call(m.mod.find('4Play8Callback'),m.owner);m.badge();assert m.scale()==1
m.toggle();assert m.taps==2;m.badge();assert m.read(m.pane+0x58,1)&1
m.call(m.mod.find('4Play8Callback'),m.owner);m.badge();assert not m.read(m.pane+0x58,1)&1
m.write(m.cursor+0x26c,1,1);m.toggle();assert m.taps==2
m.write(m.cursor+0x26c,0,1);m.write(m.root+0x5218,0);m.toggle();assert m.taps==2
print('PASS: native scale keys and dirty-transform propagation without cursor movement, alpha, removal, successful Y tap, hidden/unrelated cursor guards')

# Cross both the reported 16-item threshold and animation-ring rollover.
# Each selected item remains in the bag while the cursor stays on its row.
m=FeedbackMachine()
for i in range(33):
    m.write(m.item+4,i*7,4)
    m.toggle();assert m.size()==i+1
    m.badge();m.render();assert m.scale()==.5
    for frame in range(8):
        m.call(m.mod.find('4Play8Callback'),m.owner);m.badge();m.render()
    assert m.scale()==1 and m.read(m.pane+0x59,1)==255 and m.read(m.pane+0x58,1)&1
assert m.taps==33
print('PASS: 33 distinct bag additions across 16/17 and 32/33, animation ring reuse and final transforms')
