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

cases=0
# Cooking supplies its own page/footer while inheriting the same native state
# table. Keep the fixture tied to those real vtable entries, not only mocks.
for offset,target in ((0x1e0,0x34942f0),(0x218,0x2cbb710),(0x248,0x2cbb99c)):
    assert struct.unpack_from('<Q',RAW,0x49c8c50+offset)[0] == target
for base,kitchen in product((0x7100000000,0x7200000000),(False,True)):
    m=ShortcutMachine(base,kitchen)
    for valid,materials,multiple,supports in product((False,True),repeat=4):
        m.valid,m.materials,m.multiple=valid,materials,multiple
        m.write(m.seq+0x1c1f0,int(supports),1)
        m.enter();m.reset_footer();m.hook(m.footer_offset,m.seq)
        labels=m.hints()
        allowed=valid and materials and multiple and supports
        assert labels==[(6,'2004'),(5,'2003'),(0xffffffff,'2005')]+([(7,'qol_crafting.set_quantity')] if allowed else []),labels
        m.input_y();before=m.native_inputs;m.input_y(0x80)
        assert m.read(m.seq+0x1f0,4)==(9 if allowed else 0xffffffff)
        assert m.native_inputs==before+int(not allowed)
        cases+=1
    m.valid=m.materials=m.multiple=True;m.write(m.seq+0x1c1f0,1,1)
    for capacity in (0,1,2,3,4,5):
        m.reset_footer(capacity);m.hook(m.footer_offset,m.seq)
        assert len(m.hints())==min(capacity,4)
        cases+=1
    # Held Y cannot open on entry. Blocked/consumed/competing/pending input
    # cannot queue a selector; the next accepted gesture requires release.
    for consumed,blocked,buttons,pending,ready in ((0,0,0x80,-1,True),(0x80,0,0x80,-1,True),
            (0,1,0x80,-1,True),(0,0,0x90,-1,True),(0,0,0xa0,-1,True),
            (0,0,0x80,4,True),(0,0,0x80,-1,False)):
        m.enter();m.ready=ready
        if (consumed,blocked,buttons,pending,ready)!=(0,0,0x80,-1,True):m.input_y()
        m.write(m.seq+0x1f0,pending&0xffffffff,4)
        m.input_y(buttons,consumed,blocked)
        assert m.read(m.seq+0x1f0,4)==pending&0xffffffff
        cases+=1
    m.ready=True;m.enter();m.input_y();m.input_y(0x80)
    assert m.read(m.seq+0x1f0,4)==9 and m.read(m.seq+0x239,1)&4
    m.hook(0x3494e7c,m.seq) # Native Play exit disables the preview keys.
    assert m.read(m.input+0x54,1)==1
    m.write(m.seq+0x1b0,9,4);m.write(m.seq+0x1f0,0xffffffff,4)
    m.invoke(base+0x3496160,m.seq) # Native quantity opening/material transfer.
    assert m.attached==1 and m.read(m.child+0x165,1)==1
    m.closed=False;m.invoke(base+0x349634c,m.seq)
    assert m.read(m.seq+0x1f0,4)==0xffffffff
    m.confirmed=True;m.invoke(base+0x349634c,m.seq)
    assert m.read(m.seq+0x1c1c8,4)==0 # Native crafting result propagates unchanged.
    m.confirmed=False;m.write(m.seq+0x1c1c8,0xffffffff,4)
    m.closed=True;m.invoke(base+0x349634c,m.seq) # B/cancel returns to preview.
    assert m.detached==1 and m.read(m.seq+0x1f0,4)==3
    m.reset_footer();m.enter(prior=9)
    assert m.read(m.input+0x54,1)==0 and (7,'qol_crafting.set_quantity') in m.hints()
    # Leaving clears ownership: same page receiving later Y stays native.
    m.hook(0x3494e7c,m.seq);m.write(m.input+0x54,0,1)
    m.input_y();m.input_y(0x80)
    assert m.read(m.seq+0x1f0,4)==0xffffffff
    # X now favorites through the native Play action, once per press. Y must
    # be consumed even for a recipe that cannot open quantity.
    m.enter();m.write(m.seq+0x1c1cc,0xffffffff,4);m.input_y()
    m.input_y(0x40)
    assert m.read(m.seq+0x1c1cc,4)==3 and m.read(m.row+0x3c,4)&0x40
    m.invoke(base+0x3494d00,m.seq) # Native Play dispatches the favorite toggle.
    assert m.favorites==1
    m.write(m.seq+0x1c1cc,0xffffffff,4);m.input_y(0x40)
    assert m.read(m.seq+0x1c1cc,4)==0xffffffff
    m.materials=False;m.input_y();m.input_y(0x80)
    assert m.read(m.row+0x3c,4)&0x80 and m.read(m.seq+0x1c1cc,4)==0xffffffff
    m.materials=True
    # Footer building restores the key binding after copying the X icon.
    wrapper=m.read(m.page+0x570);key_pointer=m.read(wrapper+0xa0)
    assert m.read(key_pointer+0x14,4)==7
    cases+=4
    saved_type=m.read(m.page);m.write(m.page,base+0x49de8f0)
    m.enter();m.input_y();m.input_y(0x80)
    assert m.read(m.seq+0x1f0,4)==0xffffffff and not m.read(m.row+0x3c,4)&0x80
    m.write(m.page,saved_type);cases+=1
    # Existing localized caption routing, including with Pockets installed.
    regs,key,source=m.alloc(0x200),m.alloc(),m.alloc()
    m.write(key+8,source)
    for label in ('qol_crafting.set_quantity','2004','qol_pockets.put_in_storage'):
        m.u.mem_write(source,label.encode()+b'\0');m.write(regs+16,123);m.write(regs+24,key)
        m.hook(0x2de4940,regs)
        if label=='qol_crafting.set_quantity':
            assert m.string(m.read(m.read(regs+16)+8))=='DIYDialogConfirmNum'
            assert m.string(m.read(m.read(regs+24)+8))=='0001'
        else:assert m.read(regs+16)==123 and m.read(regs+24)==key
        cases+=1
    # Module conflicts isolate the shortcut, including with every other
    # Crafting setting disabled. No partial shortcut hooks get installed.
    for site in (0x3494b2c,0x3494e7c,0x346ec1c,0x2cbb99c,0x2cbb710,0x2de4940,0x3496160,0x2de2b00):
        q=Machine(base,{'crafting':DEFAULT+b'diy_app_preview=false\nworkbench_preview_tilt=false\ncrafting_quantity_wrap=false'})
        q.write(base+site,0xdeadbeef,4);q.boot();q.activate()
        assert 'quantity shortcut: DISABLED' in q.text and 'BOOT_PARTIAL' in q.text
        assert not any(base+off in q.hooks for off in (0x3494b2c,0x3494e7c,0x346ec1c,0x2cbb99c,0x2de4940))
        assert base+0x22fc6bc in q.hooks and base+0x2279a14 in q.hooks
        cases+=1
print(f'PASS: {cases} quantity-shortcut cases at two bases; native workbench/cooking preview/footer, quantity open/cancel, material gates, fresh Y, caption routing and isolated conflicts. Live Eden test pending.')
