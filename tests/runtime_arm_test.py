"""Execute compiled prototype callbacks in ARM64 emulation.

Native pricing traversal and native Play dispatch execute in the final cases;
UI drawing, storage queries, item-list collection, and engine services are stubs.
No save data is loaded or written. This does not replace an Eden test.
"""
from pathlib import Path
import struct, hashlib
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import *
from elf_loader import Module, ROOT
RAW=(ROOT/'Grab 5/working/main.raw').read_bytes()
assert hashlib.sha256(RAW).hexdigest()=='b1ae8e2a0d82f9f38f711c132d1751b221baf9d6411b2be06ebc4bb502ac912d'

class Machine:
    def __init__(self,base):
        self.base=base;self.u=Uc(UC_ARCH_ARM64,UC_MODE_ARM);self.services={};self.calls=[];self.native_price=False
        self.u.mem_map(base,0x6000000);self.u.mem_write(base,RAW)
        self.ram=base+0x10000000;self.u.mem_map(self.ram,0x300000)
        self.mod=Module(self,'qol-enhancements',base+0x7000000)
        self.owner=self.ram;self.page=self.ram+0x20000;self.snapshot=self.page+0x1000
        self.hint=self.page+0x2000;self.hintinput=self.hint+0x200;self.rows=self.page+0x3000
        self.context=self.page+0x4000;self.app=self.page+0x5000;self.masks=self.page+0x6000
        self.stock=self.page+0x7000;self.indices=self.page+0x8000;self.item=self.page+0x9000
        self.session=self.mod.find('L7sessionE');self.bag=self.session+8;self.count=self.bag+256*12
        self.stub=self.page+0xf000;self.write(self.stub,0xd65f03c0,4)
        for name in ('Begin','Destroy','Focus','Row','Play','Checkout','Price','Collect','Dialog','GrossPrice','PurchaseRecord'):
            p=self.stub+0x20*(1+len(self.calls));self.calls.append(name)
            self.services[p]=('original',name);self.write(self.mod.find(str(len(name))+name+'EE7OrigRef'),p)
        self.calls=[]
        self.write(self.mod.find('L13originalInputE'),self.stub)
        self.storage=100;self.pockets=10;self.purchasecount=0;self.oldcount=0
        self.write(self.owner,base+0x4a10620);self.write(self.owner+0x210,self.page)
        self.write(self.owner+0x1b0,3,4);self.write(self.owner+0x1f0,0xffffffffffffffff)
        self.write(self.owner+0x1c318,0xffffffff,4);self.write(self.owner+0x1c4c0,1,1)
        self.write(self.owner+0x1c338,3,2);self.write(self.owner+0x1c340,self.stock)
        self.write(self.owner+0x1c358,3,4);self.write(self.owner+0x1c368,3,4)
        self.write(self.owner+0x1c360,self.indices)
        for i in range(3):
            self.write(self.indices+i*2,i,2);self.write(self.stock+i*8,self.item+i*8)
            self.u.mem_write(self.item+i*8,struct.pack('<HBBI',117+i,0,0,i*7))
        for i in range(10):self.write(self.owner+0x1c36c+i*12+4,0xffffffff,4)
        self.write(self.page+0x570,self.hint);self.write(self.hint+0xa0,self.hintinput)
        self.write(self.hintinput+0x10,0,4);self.write(self.hintinput+0x14,5,4)
        self.write(self.snapshot+8,1,4);self.write(self.snapshot+0x10,self.rows)
        self.write(self.page+0x210,self.context);self.write(base+0x59f83f0,self.app)
        self.write(self.app+0x418,self.masks)
        self.u.hook_add(UC_HOOK_CODE,self.service)
        self.call(self.mod.find('5Begin8Callback'),self.owner)
        self.call(self.mod.find('5Focus8Callback'),self.owner,0)
    def write(self,p,v,n=8):self.u.mem_write(p,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
    def read(self,p,n=8):return int.from_bytes(self.u.mem_read(p,n),'little')
    def reg(self,i):return self.u.reg_read(UC_ARM64_REG_X0+i)
    def service(self,u,pc,size,_):
        service=self.services.get(pc);result=0
        off=pc-self.base
        if service:
            kind,name=service
            if kind=='original':
                self.calls.append(name)
                if name=='Price':result=500
            elif kind=='import':
                if name in ('memset','__memset_chk'):u.mem_write(self.reg(0),bytes([self.reg(1)&255])*self.reg(2));result=self.reg(0)
                elif name in ('memcpy','memmove','__memcpy_chk'):u.mem_write(self.reg(0),bytes(u.mem_read(self.reg(1),self.reg(2))));result=self.reg(0)
                elif name=='memcmp':result=int(bytes(u.mem_read(self.reg(0),self.reg(2)))!=bytes(u.mem_read(self.reg(1),self.reg(2))))
                else:raise AssertionError(('unexpected import',name))
            elif kind!='log':raise AssertionError(service)
        elif off==0x311ca40:
            p=self.reg(1);assert self.read(p)==10<<32
            assert self.read(p+8)==p+0x70 and self.read(p+0x10)==p+0x20
            for i in range(9):assert self.read(p+0x20+i*8)==p+0x28+i*8
            assert self.read(p+0x68)==0
            self.write(p,self.oldcount if self.reg(2) else self.purchasecount,4)
        elif off==0x257abf0:result=10000
        elif off==0x257c230:result=self.storage
        elif off==0x25731d0:result=self.pockets
        elif off==0x311e180 and not self.native_price:
            p=self.reg(0)
            if p==self.owner+0x1c330:
                u.reg_write(UC_ARM64_REG_PC,self.mod.find('5Price8Callback'));return
            assert self.read(p+0x40,4)==0
            assert all(self.read(p+0x40+i*12,4)==0xffffffff for i in range(1,10))
            ptr=self.read(self.read(p+0x10));assert self.read(ptr,2) in (117,118,119)
            result=100+self.read(ptr,2)
        elif off==0x2b12c24:result=0 # Counter lookup unavailable in emulation.
        elif off==0x267d6c4:result=self.read(self.reg(0),2)
        elif off==0x19ed730:
            assert self.read(self.reg(0)+8,2) in (117,118,119)
            result=217
        elif off==0x263f0a0:result=0
        else:return
        u.reg_write(UC_ARM64_REG_X0,result)
        u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))
    def call(self,pc,*args):
        for i,v in enumerate(args):self.u.reg_write(UC_ARM64_REG_X0+i,v)
        self.u.reg_write(UC_ARM64_REG_SP,self.ram+0x2f0000)
        self.u.reg_write(UC_ARM64_REG_LR,self.base+0x400)
        self.u.emu_start(pc,self.base+0x400,count=getattr(self,'instruction_budget',200000))
        assert self.u.reg_read(UC_ARM64_REG_PC)==self.base+0x400,hex(self.u.reg_read(UC_ARM64_REG_PC))
        return self.reg(0)
    def input(self,buttons,blocked=0,consumed=0):
        self.write(self.rows,buttons,4);self.write(self.rows+0x3c,consumed,4)
        # Use the real vtable adapter; its private helper can be inlined.
        self.call(self.mod.find('able_bag', 'L5InputE'),self.page,self.snapshot,blocked)
    def focus(self,i):self.call(self.mod.find('5Focus8Callback'),self.owner,i)
    def toggle(self):self.input(0);self.input(0x80)
    def size(self):return self.read(self.count,4)

def main():
    checks=0
    for base in (0,0x7100000000):
        m=Machine(base)
        m.input(0x80);assert m.size()==0 # Opening while held does not add.
        m.toggle();assert m.size()==1
        assert bytes(m.u.mem_read(m.bag,8))==bytes(m.u.mem_read(m.item,8))
        m.input(0x80);assert m.size()==1
        m.focus(1);m.toggle();assert m.size()==2
        m.storage=m.pockets=0;m.focus(2);m.toggle();assert m.size()==2
        assert m.read(m.owner+0x1f0,4)==4 and m.read(m.owner+0x1c4f0,4)==3
        m.write(m.owner+0x1f0,0xffffffff,4) # Simulate dismissing native full-space dialog.
        m.focus(0);m.toggle();assert m.size()==1 # Remove at capacity.
        for blocked,consumed,buttons in ((1,0,0x80),(0,0x80,0x80),(0,0,0x90),(0,0,0x81)):
            m.input(0);m.input(buttons,blocked,consumed);assert m.size()==1;checks+=1
        for value,offset,n in ((1,0x54,1),(1,0x58,4)):
            m.write(m.context+offset,value,n);m.write(m.masks+0x10,1,4)
            m.toggle();assert m.size()==1;m.write(m.context+offset,0,n)
        m.write(m.owner+0x1f0,4,4);m.toggle();assert m.size()==1;m.write(m.owner+0x1f0,0xffffffff,4)
        # No pending input leaks after blocked held presses.
        m.storage=100;m.input(0x80);assert m.size()==1
        m.toggle();assert m.size()==2
        total=m.call(m.mod.find('5Price8Callback'),m.owner+0x1c330);assert total==500+217+218
        m.write(m.owner+0x1c370,0,4)
        assert m.call(m.mod.find('5Price8Callback'),m.owner+0x1c330)==500+218
        # Independent checkout guard executes no native checkout for a bag.
        m.calls.clear();m.call(m.mod.find('8Checkout8Callback'),m.owner,0);assert 'Checkout' not in m.calls
        # Bag checkout opens native confirmation, including bag-only purchases.
        # Minimal counter container is sufficient because parameter lookup is stubbed.
        layout=m.page+0xc000;m.write(m.page+0x3f8,layout);m.write(layout,layout+0x100)
        m.write(m.mod.find('4PlayEE7OrigRef'),base+0x311a538)
        m.write(m.owner+0x1c318,1,4)
        m.call(m.mod.find('4Play8Callback'),m.owner)
        assert m.read(m.owner+0x1f0,4)==4 and m.read(m.owner+0x1c4f0,4)==4
        # Session clear on destruction, and regular checkout passthrough empty.
        m.call(m.mod.find('7Destroy8Callback'),m.owner);assert m.size()==0 and m.read(m.session)==0
        m.calls.clear();m.call(m.mod.find('8Checkout8Callback'),m.owner,0);assert m.calls==['Checkout']
        checks+=20
    # Execute native total-price iteration on the synthetic one-item layout at base 0.
    m=Machine(0);m.native_price=True
    m.write(m.mod.find('5PriceEE7OrigRef'),0x311e180)
    manager=m.ram+0x50000;table=manager+0x60000;rows=table+0x1000
    m.write(0x4ab43c0,m.ram+0x45000);m.write(m.ram+0x45000,manager)
    m.write(manager+0x57e58,table);m.write(table+0x1d8,1000,4);m.write(table+0x1e0,rows)
    for item in (117,118,119):m.write(rows+item*8,table+0x8000)
    m.write(0x4b487d4,0xffffffff,4);m.write(0x52274a0,0)
    m.toggle();assert m.size()==1 and m.read(m.bag+8,4)==217
    print(f'PASS: compiled ARM callbacks at two load bases; {checks} groups; native price and checkout-command dispatch')
if __name__=='__main__':main()
