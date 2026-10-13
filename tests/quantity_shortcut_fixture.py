"""Workbench/cooking Y shortcut: compiled input/hooks and native open/cancel/footer paths.

Layout allocation, engine type checks, recipe data and child readiness are
modeled. Native Play entry/exit, MultiCreation entry/update and footer record
copying run directly. Live controller hints and visuals still require Eden.
"""
from itertools import product
import struct
from core_arm_test import Machine, RAW, DEFAULT
from unicorn.arm64_const import *

class ShortcutMachine(Machine):
    def __init__(self, base, kitchen=False):
        self.footer_offset = 0x2cbb99c if kitchen else 0x346ec1c
        self.stubs = {}
        self.next = base + 0x10030000
        super().__init__(base, {'crafting': DEFAULT, 'pockets': DEFAULT})
        self.boot(); self.activate()
        assert 'quantity shortcut: READY' in self.text
        self.native_inputs = self.attached = self.detached = self.favorites = 0
        self.valid = self.materials = self.multiple = self.ready = True
        self.closed = self.confirmed = False
        self.seq, self.page, self.child = self.alloc(0x1c300), self.alloc(0x700), self.alloc(0x400)
        self.methods, self.child_methods = self.alloc(0x300), self.alloc(0x300)
        self.write(self.seq, self.methods); self.write(self.child, self.child_methods)
        self.write(self.page, base+(0x49c88a0 if kitchen else 0x4a3b3e0))
        self.write(self.seq+0x1c1f8, self.page); self.write(self.seq+0x1c1e8,self.child)
        self.write(self.seq+0x1c1f0,1,1); self.write(self.seq+0x264,17,4)
        self.write(self.seq+0x1f8,2,1); self.write(self.seq+0x260,3,4)
        self.write(self.seq+0x1c1c8,0xffffffff,4); self.write(self.seq+0x1c1cc,0xffffffff,4)
        for method, action in ((0x218,lambda:self.page),(0x220,lambda:0),
                               (0x288,lambda:int(self.valid)),(0x290,lambda:int(self.materials)),
                               (0x2a8,lambda:int(self.multiple)),(0x298,self.favorite),(0x2a0,self.recipe_data)):
            self.write(self.methods+method,self.stub(action))
        for method, action in ((0xf0,lambda:int(self.ready)),(0x208,self.set_recipe),
                               (0x210,lambda:int(self.confirmed)),(0xe8,lambda:int(self.closed))):
            self.write(self.child_methods+method,self.stub(action))
        self.write(self.child+0xa8,2,4)
        self.write(self.methods+0x248,self.hooks[base+self.footer_offset])
        if kitchen: self.write(self.methods+0x218,base+0x2cbb710) # Actual inherited page getter.
        # Restore native originals behind the four trampoline hooks.
        for name,offset in (('EnterQuantityShortcutPreview',0x3494b2c),
                            ('LeaveQuantityShortcutPreview',0x3494e7c),('BuildQuantityShortcutHint',0x346ec1c),
                            ('BuildCookingQuantityShortcutHint',0x2cbb99c)):
            self.u.mem_write(base+offset,RAW[offset:offset+20])
            self.write(self.module.find(name,'OrigRef'),base+offset)
        self.write(self.module.find('SharedPageInput','OrigRef'), self.stub(self.native_input))
        self.stubs[base+0x3769260]=self.attach
        self.stubs[base+0x3768c7c]=self.detach
        self.stubs[base+0x3f04470]=self.copy
        self.stubs[base+0x2de2de0]=lambda:0 if self.reg(0)==0xffffffff else self.reg(0) # Native button priority.
        self.input, self.focus = self.alloc(), self.alloc()
        self.write(self.page+0x210,self.input);self.write(self.input+0x48,self.focus)
        for off,button in ((0x570,7),(0x578,0),(0x580,1),(0x588,5)):
            wrapper,key=self.alloc(),self.alloc()
            self.write(self.page+off,wrapper);self.write(wrapper+0xa0,key)
            self.write(key+0x10,2,4);self.write(key+0x14,button,4)
        self.snapshot,self.rows=self.alloc(),self.alloc(8*0x48)
        self.row=self.rows+2*0x48
        self.write(self.snapshot+8,8,4);self.write(self.snapshot+0x10,self.rows)
        self.app,self.footer,self.entries,self.pool=self.alloc(),self.alloc(0x300),self.alloc(),self.alloc(0x400)
        slot=self.alloc();self.write(slot,self.app);self.write(base+0x4ab2230,slot)
        self.write(base+0x5639190,self.app)
        self.write(self.app+0x58,self.footer)
        methods,guard=self.alloc(),self.alloc()
        self.write(self.footer,methods);self.write(methods,self.stub(lambda:1))
        self.write(guard,1,1);self.write(base+0x4ac7280,guard);self.write(base+0x4ac7278,self.alloc())
        for got in (0x4ab2020,0x4ab2bc0,0x4ab37e0):
            self.write(base+got,base+struct.unpack_from('<Q',RAW,got)[0])
        string_vt=self.read(base+0x4ab2bc0)+16
        self.write(string_vt+0x18,self.stub(lambda:0))
        self.reset_footer();self.enter()
    def alloc(self,size=0x100):
        out=self.next;self.next+=(size+15)&~15
        assert self.next < self.ram+0x1d0000
        return out
    def stub(self,action):
        out=self.alloc(16);self.stubs[out]=action;return out
    def service(self,u,pc,size,ctx):
        if pc in self.stubs:
            value=self.stubs[pc]()
            u.reg_write(UC_ARM64_REG_X0,value or 0)
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))
        else: super().service(u,pc,size,ctx)
    def invoke(self,address,*args):
        for i,v in enumerate(args):self.u.reg_write(UC_ARM64_REG_X0+i,v)
        self.call(address)
    def hook(self,offset,*args):self.invoke(self.hooks[self.base+offset],*args)
    def native_input(self):self.native_inputs+=1
    def favorite(self):
        assert (self.reg(0),self.reg(1))==(self.seq,17)
        self.favorites+=1
    def attach(self):
        assert (self.reg(0),self.reg(1))==(self.seq,self.child)
        self.attached+=1
    def detach(self):
        assert (self.reg(0),self.reg(1))==(self.seq,self.child)
        self.detached+=1
    def recipe_data(self):
        assert self.reg(0)==self.seq and self.reg(1)==17
        # Distinct material-limited recipe data must reach the existing child.
        self.u.mem_write(self.reg(2),bytes(range(0xc0)))
    def set_recipe(self):
        assert self.reg(0)==self.child
        assert bytes(self.u.mem_read(self.reg(1),0xc0))==bytes(range(0xc0))
    def copy(self):
        self.u.mem_write(self.reg(0),bytes(self.u.mem_read(self.reg(1),self.reg(2))))
        return self.reg(0)
    def reset_footer(self,capacity=5):
        self.write(self.footer+0x1b0,2,4);self.write(self.footer+0x1f0,0xffffffff,4);self.write(self.footer+0x1f4,0xffffffff,4)
        self.write(self.footer+0x218,0,4);self.write(self.footer+0x21c,capacity,4)
        self.write(self.footer+0x220,self.entries);self.write(self.footer+0x228,self.pool)
        for i in range(5):
            self.u.mem_write(self.pool+i*0x88,bytes(0x88))
            self.write(self.pool+i*0x88,self.pool+(i+1)*0x88 if i<4 else 0)
    def enter(self,prior=2):
        self.write(self.seq+0x1b0,3,4);self.write(self.seq+0x1b8,prior,4)
        self.write(self.seq+0x1f0,0xffffffff,4);self.write(self.seq+0x1f4,0xffffffff,4)
        self.write(self.seq+0x200,0,1)
        self.hook(0x3494b2c,self.seq)
    def input_y(self,buttons=0,consumed=0,blocked=0):
        self.write(self.row,buttons,4);self.write(self.row+0x3c,consumed,4)
        self.hook(0x3c58d24,self.page,self.snapshot,blocked)
    def hints(self):
        return [(self.read(p,4),self.string(self.read(p+0x18))) for p in
                (self.read(self.entries+i*8) for i in range(self.read(self.footer+0x218,4)))]
