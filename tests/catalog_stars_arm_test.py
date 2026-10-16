"""Compiled star hook plus native catalog query, real ItemParam data, two ASLR bases.

Requires local (unpackaged) vanilla resources and Unicorn. Engine services use the
NativeQuery fixture; text construction and Exlaunch installation are not emulated.
"""
from pathlib import Path
import struct
from catalog_query_arm_test import NativeQuery, ROOT
from unicorn import UC_HOOK_CODE, UC_PROT_READ, UC_PROT_EXEC
from unicorn.arm64_const import *

elf = (Path(__file__).resolve().parents[1] / 'project.elf').read_bytes()
u16 = lambda b,o: struct.unpack_from('<H',b,o)[0]
u64 = lambda b,o: struct.unpack_from('<Q',b,o)[0]
sections = [struct.unpack_from('<IIQQQQIIQQ',elf,u64(elf,40)+i*u16(elf,58)) for i in range(u16(elf,60))]
symbols, tables = {}, {}
for index,s in enumerate(sections):
    if s[1] not in (2,11): continue
    names = sections[s[6]]; strings = elf[names[4]:names[4]+names[5]]; table=[]
    for off in range(s[4],s[4]+s[5],s[9]):
        n,info,other,sec,value,size=struct.unpack_from('<IBBHQQ',elf,off)
        name=strings[n:strings.find(b'\0',n)].decode(); table.append((name,value,sec))
        if sec and name and ("Callback" not in name or info&15==2): symbols[name]=value
    tables[index]=table

def symbol(*needles):
    found=[v for k,v in symbols.items() if all(n in k for n in needles)]
    assert len(found)==1,(needles,found)
    return found[0]

bcsv=bytearray((ROOT/'vanilla-3.0.3/Bcsv/ItemParam.bcsv').read_bytes())
entries,stride,fields,version=struct.unpack_from('<IIHB',bcsv)
header=28 if version else 12
field_offsets=dict(struct.unpack_from('<II',bcsv,header+8*i) for i in range(fields))
ids=[]; nonwall=[]
for i in range(entries):
    row=header+8*fields+i*stride
    struct.pack_into('<I',bcsv,row,row) # Runtime BCSV fixup: back-reference to header.
    item_id=u16(bcsv,row+field_offsets[0x54706054])
    kind=struct.unpack_from('<I',bcsv,row+field_offsets[0x5270eb75])[0]
    (ids if kind in (0x055c8679,0xc354e1f6) else nonwall).append((item_id,row))
assert len(ids)==527

class Stars(NativeQuery):
    def __init__(self,base):
        super().__init__(base)
        image_size=max(va+memsz for typ,flags,off,va,pa,sz,memsz,align in
                       (struct.unpack_from('<IIQQQQQQ',elf,u64(elf,32)+i*u16(elf,54)) for i in range(u16(elf,56))) if typ==1)
        self.mod=base+0x7000000; self.u.mem_map(self.mod,(image_size+4095)&~4095)
        for i in range(u16(elf,56)):
            typ,flags,off,va,pa,sz,memsz,align=struct.unpack_from('<IIQQQQQQ',elf,u64(elf,32)+i*u16(elf,54))
            if typ==1: self.u.mem_write(self.mod+va,elf[off:off+sz])
        for s in sections:
            if s[1]!=4: continue
            for off in range(s[4],s[4]+s[5],s[9]):
                target,info,addend=struct.unpack_from('<QQq',elf,off); typ=info&0xffffffff
                if typ==1027: value=self.mod+addend
                elif typ in (257,1025,1026):
                    name,val,sec=tables[s[6]][info>>32]
                    value=self.mod+val+addend if sec else base+0x300
                else: raise AssertionError(typ)
                self.write(self.mod+target,value)
        self.write(self.mod+symbol('s_ModuleBitset'),2)
        self.write(self.mod+symbol('s_ModuleInfos')+72,base)
        self.locale=1
        self.logs=[]
        self.log_address=self.mod+symbol("SvcLogger","LogRaw")
        self.u.hook_add(UC_HOOK_CODE,self.log_stub)
        self.data=base+0x20000000
        self.u.mem_map(self.data,(len(bcsv)+4095)&~4095); self.u.mem_write(self.data,bytes(bcsv))
        for item_id,row in ids+nonwall: self.write(self.rows+8*item_id,self.data+row)
        self.write(base+0x4ac0428,self.ram+0x3050); self.write(self.ram+0x3050,self.table)
        self.write(base+0x54021a0,self.manager)
        self.owner=self.ram+0x170000; self.frame=self.ram+0x180800
        self.name=self.frame-0x440; self.text=self.name+0xf0; self.ctx=self.ram+0x190000
        self.window=self.ram+0x172000
        self.write(self.owner+0x360,self.window)
        self.write(self.ctx+29*8,self.frame); self.write(self.ctx+19*8,self.window)
        self.write(self.ctx+22*8,self.item)
        self.write(self.frame+8,base+0x310be34); self.write(self.frame+0x58,self.owner)
        self.u.mem_protect(self.data,(len(bcsv)+4095)&~4095,UC_PROT_READ)
        self.u.mem_protect(self.item,0x2000,UC_PROT_READ)
    def log_stub(self,u,pc,size,_):
        if pc==self.log_address:
            self.logs.append(bytes(u.mem_read(self.reg(2),self.reg(1))).decode())
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))
        elif pc==self.base+0x37cefc4:
            u.reg_write(UC_ARM64_REG_X0,self.locale)
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))
        elif pc==self.base+0x300: raise AssertionError('unexpected module import')
    def call(self,addr):
        self.u.reg_write(UC_ARM64_REG_SP,self.sp); self.u.reg_write(UC_ARM64_REG_LR,self.stop)
        self.u.emu_start(addr,self.stop,count=50000)
        assert self.u.reg_read(UC_ARM64_REG_PC)==self.stop
        assert self.u.reg_read(UC_ARM64_REG_SP)==self.sp
    def display(self,item_id,owned=False,text='Sample wall',expected=None,kind=0,capacity=128,flags=0):
        item=struct.pack('<HBBI',item_id,0,flags,0); bits=bytearray(0x754)
        if owned and item_id<15000: bits[item_id//8]|=1<<(item_id%8)
        self.u.mem_write(self.item,item); self.u.mem_write(self.bits,bytes(bits))
        self.u.mem_write(self.text,b'\0'*256)
        self.u.mem_write(self.text,text.encode('utf-16-le')+b'\0\0')
        self.write(self.name+8,kind,4); self.write(self.name+0x18,self.text); self.write(self.name+0x20,capacity,4)
        self.u.reg_write(UC_ARM64_REG_X0,self.ctx)
        self.call(self.mod+symbol('CabinetCatalogStar','Callback'))
        if expected is None: expected=text if owned else '★ '+text
        expected_bytes=expected.encode('utf-16-le')+b'\0\0'
        result=bytes(self.u.mem_read(self.text,len(expected_bytes)))
        assert result==expected_bytes,(item_id,result,expected_bytes)
        assert bytes(self.u.mem_read(self.bits,0x754))==bytes(bits)
        assert bytes(self.u.mem_read(self.item,8))==item

def main():
    count=0
    for base in (0x7100000000,0x7200000000):
        q=Stars(base)
        q.call(q.mod+symbol('ValidateCatalogBindings')); assert q.reg(0)==1
        for item_id,row in ids:
            for owned in (False,True): q.display(item_id,owned); count+=1
        item_id=ids[0][0]
        for text in ('étoiles wall','ほしのかべ','壁紙','😀 wall','★ Sample wall','a'*125,'a'*126,'\x0eTagged'):
            skip=text.startswith(('★','\x0e')) or len(text)>125
            q.display(item_id,text=text,expected=text if skip else '★ '+text[0].upper()+text[1:]); count+=1
        for locale,expected in ((11,'★ IJsvloer'),(1,'★ Ijsvloer')):
            q.locale=locale
            q.display(item_id,text='ijsvloer',expected=expected); count+=1
        q.display(item_id,text='étoiles wall',owned=True,expected='étoiles wall'); count+=1
        for kind,capacity,flags in ((3,128,0),(4,128,0),(0,127,0),(0,128,1)):
            q.display(item_id,expected='Sample wall',kind=kind,capacity=capacity,flags=flags); count+=1
        for off,bad,good,size in ((q.frame+8,base+0x310be38,base+0x310be34,8),
                                 (q.owner+0x360,0,q.window,8),(q.owner+0x1f8,8,0,1),
                                 (q.status+0x13d4,0,1,1),(q.status+0x13d6,0,1,1),
                                 (q.ram+0x3050,0,q.table,8),(base+0x54021a0,0,q.manager,8),
                                 (q.other+0x1070,0,q.bits,8),(q.rows+item_id*8,0,q.data+ids[0][1],8)):
            q.write(off,bad,size); q.display(item_id,expected='Sample wall'); q.write(off,good,size); count+=1
        q.have_save=False; q.display(item_id,expected='Sample wall'); q.have_save=True; count+=1
        q.mapped_player=8; q.display(item_id,expected='Sample wall'); q.mapped_player=0; count+=1
        for other,row in nonwall[:20]:
            for owned in (False,True):
                q.display(other,owned,expected='Sample wall' if owned else '★ Sample wall'); count+=1
        # Rebuilding after registration removes the marker; no cached negative state.
        for owned in (False,True,False,True): q.display(item_id,owned); count+=1
    print(f'PASS: {count} compiled catalog-star cases at two load bases; native catalog query and all 527 wall/floor IDs')


if __name__ == "__main__": main()
