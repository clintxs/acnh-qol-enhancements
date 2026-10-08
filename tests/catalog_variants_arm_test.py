"""Real native variant index/count/history with compiled hooks at two load bases.

The ItemAccessor body-pointer service is substituted with its Ftr result (wrapper
+0xc). Index/count and read-only history routines execute original ARM, on real
ItemParam/ItemRemake rows. Resource lookup, allocators and save services use the
existing fixtures. Source items and both history stores are read-only to ARM.
"""
import struct
from pathlib import Path
from nook_catalog_stars_arm_test import Nook, FURNITURE
from catalog_stars_arm_test import ROOT, bcsv, field_offsets, nonwall, ids, symbol
from unicorn import UC_PROT_READ
from unicorn.arm64_const import *

raw=bytearray((ROOT/'vanilla-3.0.3/Bcsv/ItemRemake.bcsv').read_bytes())
entries,stride,fields,version=struct.unpack_from('<IIHB',raw)
header=28 if version else 12
fields_by_hash=dict(struct.unpack_from('<II',raw,header+8*i) for i in range(fields))
remakes={}
for i in range(entries):
    row=header+8*fields+i*stride;struct.pack_into('<I',raw,row,row)
    ident=struct.unpack_from('<H',raw,row+fields_by_hash[0x54706054])[0]
    count=struct.unpack_from('<b',raw,row+fields_by_hash[0xb0304b0d])[0]+1
    remakes[ident]=(row,count)
rows_by_id=dict(nonwall)
furniture=[]
for item in FURNITURE:
    remake=struct.unpack_from('<h',bcsv,rows_by_id[item]+field_offsets[0xcb5eb33f])[0]
    if remake in remakes and remakes[remake][1]>1:furniture.append((item,remake,remakes[remake][1]))

class Variants(Nook):
    def __init__(self,base):
        super().__init__(base)
        self.rtable=self.ram+0x17c000;self.rrows=self.ram+0x1d0000
        self.variants=self.ram+0x1c8000
        self.rdata=base+0x22000000
        self.u.mem_map(self.rdata,(len(raw)+4095)&~4095);self.u.mem_write(self.rdata,bytes(raw))
        self.write(base+0x522a768,self.table);self.write(base+0x522a7c8,self.rtable)
        self.write(self.rtable+0x1f8,0,2);self.write(self.rtable+0x1d8,max(remakes)+1,4)
        self.write(self.rtable+0x1e0,self.rrows)
        for ident,(row,_) in remakes.items():self.write(self.rrows+8*ident,self.rdata+row)
        self.write(self.other+0x10e0,self.variants)
        for got,flag,cache,size in ((0x4abfe90,0x4abfe88,self.ram+0x1c0000,0x1ac),
                                  (0x4abff50,0x4abff48,self.ram+0x1c1000,0xe8)):
            self.write(base+got,cache);self.write(base+flag,cache+0x300)
            self.write(cache+0x300,1,1);self.u.mem_write(cache,b'\xff'*size)
        self.u.mem_protect(self.rdata,(len(raw)+4095)&~4095,UC_PROT_READ)
        self.u.mem_protect(self.variants,0x1000,UC_PROT_READ)
        self.index_calls=0

    def hook(self,u,pc,size,data):
        if pc-self.base==0x19fd600:
            u.reg_write(UC_ARM64_REG_X0,self.reg(0)+0xc)
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR));return
        if pc-self.base==0x1a00960:self.index_calls+=1
        return super().hook(u,pc,size,data)

    def history(self,remake,body):
        bits=bytearray(2000)
        if body is not None:
            index=remake*8+body;bits[index//8]|=1<<(index%8)
        self.u.mem_write(self.variants,bytes(bits))
        return bytes(bits)

    def check_variant(self,item,remake,count,body,base_owned,known_body,expected):
        before=self.history(remake,known_body)
        self.check(item,owned=base_owned,marked=bool(expected),payload=body,marker=expected or '★')
        assert bytes(self.u.mem_read(self.variants,2000))==before


def main():
    total=0
    for base in (0x7100000000,0x7200000000):
        q=Variants(base)
        for item,remake,count in furniture:
            # First/last bodies and distinct history states, independent of shop RNG.
            for body in {0,count-1}:
                for base_owned,known_body,marker in ((False,None,'★'),(True,body,''),(True,(body+1)%count,'☆')):
                    q.check_variant(item,remake,count,body,base_owned,known_body,marker);total+=1
        item,remake,count=furniture[0]
        q.check_variant(item,remake,count,0,True,None,'☆');total+=1
        # Non-body payload bits (fabric/pattern) do not create fake missing colors.
        q.history(remake,0);q.check(item,owned=True,marked=False,payload=0x100);total+=1
        q.history(remake,0)
        # Missing variant history must not look like a missing cataloged variant.
        for address in (q.other+0x10e0,base+0x522a768,base+0x522a7c8):
            value=bytes(q.u.mem_read(address,8));q.write(address,0)
            q.check(item,owned=True,marked=False);q.u.mem_write(address,value);total+=1
        # Invalid body is not silently normalized to body zero.
        short=next(t for t in furniture if t[2]<8)
        q.check(short[0],owned=True,marked=False,payload=7);total+=1
        # Execute native variant read-lock callbacks, including failed acquisition.
        q.write(q.other+0x1100,q.lock)
        for result,expected_calls in ((1,[0x200,0x204]),(0,[0x200])):
            q.lock_result=result;q.lock_calls.clear()
            q.check_variant(item,remake,count,0,True,1,'☆')
            assert q.lock_calls==expected_calls,q.lock_calls
            total+=1
        q.write(q.other+0x1100,0);q.lock_result=1
        # Hook output for shared shop balloon (not only Nook dialogue).
        q.write(q.ctx+29*8,q.frame);q.write(q.ctx+19*8,q.window);q.write(q.ctx+22*8,q.item)
        q.write(q.frame+8,base+0x310be34);q.write(q.frame+0x58,q.owner)
        q.write(q.owner+0x360,q.window)
        q.history(remake,1)
        q.display(item,owned=True,text='blue furniture',expected='☆ Blue furniture');total+=1
        context=bytes(q.u.mem_read(q.ctx,31*8))
        q.u.reg_write(UC_ARM64_REG_X0,q.ctx);q.call(q.mod+symbol('CabinetCatalogStar','Callback'))
        assert bytes(q.u.mem_read(q.text,len('☆ Blue furniture'.encode('utf-16-le'))+2))=='☆ Blue furniture\0'.encode('utf-16-le')
        assert bytes(q.u.mem_read(q.ctx,31*8))==context;total+=1
        # The experimental shop list permits every ordinary ItemKind, not just
        # the old wallpaper/flooring allowlist. Sample every real kind.
        kinds={}
        for ident,row in ids+nonwall:
            kind=struct.unpack_from('<I',bcsv,row+field_offsets[0x5270eb75])[0]
            kinds.setdefault(kind,ident)
        for kind,ident in kinds.items():
            excluded=(kind in (0xeea2c69e,0x6693e43a,0x890e26a5,0x761e3ce0) or
                      ident in (2543,3075,3247,5851,9964,*range(8773,8782),9771))
            q.display(ident,owned=False,expected='Sample wall' if excluded else '★ Sample wall');total+=1
        assert q.index_calls>0
    print(f'PASS: {total} variant cases across {len(furniture)} furniture records at two bases; real native index/count/history, filled/hollow/none and unchanged history')

if __name__=='__main__':main()
