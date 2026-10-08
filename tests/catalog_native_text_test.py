"""Regression for 0.5.8's missing stars: actual native text construction/parsing.

Runs original name-buffer initialization and 01b0b154 on local USen messages,
then the compiled star hook plus native catalog query. Main is loaded at zero
so the local mapped image's vtables/GOT remain valid without a simulated linker.
The two-base tests live in catalog_stars_arm_test.py. No game assets are shipped.
"""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path
from catalog_stars_arm_test import Stars, ROOT, ids, symbol
from unicorn.arm64_const import *

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from sarc_tool import read_sarc
from msbt_tool import read_msbt

class NativeText(Stars):
    def hook(self,u,pc,size,data):
        if pc==0x3f04450:
            u.mem_write(self.reg(0),bytes([self.reg(1)&255])*self.reg(2))
        elif pc==0x3f04470:
            u.mem_write(self.reg(0),bytes(u.mem_read(self.reg(1),self.reg(2))))
        elif pc==0x400:
            u.reg_write(UC_ARM64_REG_X0,self.locale)
        else:
            return super().hook(u,pc,size,data)
        u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))

    def native_capitalized(self,text):
        # Execute the real formatter's character emission/case mapping. The
        # layout's capitalize-next-substitution state is supplied; locale is
        # the only virtual service stub. Rendering and font layout are excluded.
        renderer=self.ram+0x1b0000; vtable=renderer+0x1000; output=renderer+0x2000
        self.u.mem_write(renderer,b'\0'*0x600)
        self.write(renderer,vtable); self.write(vtable+0x58,0x400)
        self.write(renderer+0x2a0,output); self.write(renderer+0x2a8,output+512)
        self.write(renderer+0x4bb,1,1)
        encoded=text.encode('utf-16-le')
        for (unit,) in struct.iter_unpack('<H',encoded):
            self.u.reg_write(UC_ARM64_REG_X0,renderer)
            self.u.reg_write(UC_ARM64_REG_X1,unit)
            self.call(0x1ab5740)
        end=int.from_bytes(self.u.mem_read(renderer+0x2a0,8),'little')
        return bytes(self.u.mem_read(output,end-output)).decode('utf-16-le')

    def native_name(self,item_id,text):
        # Exactly the stack location used by 03011440; construct, don't fabricate,
        # the name object, capacity, buffer placement and both form buffers.
        sp=self.frame-0x450
        self.u.reg_write(UC_ARM64_REG_SP,sp)
        self.u.emu_start(0x30115c4,0x30117b4,count=5000)
        assert self.u.reg_read(UC_ARM64_REG_PC)==0x30117b4
        assert self.u.reg_read(UC_ARM64_REG_SP)==sp
        read=lambda p,n=8:int.from_bytes(self.u.mem_read(p,n),'little')
        assert read(self.name+8,4)==0 and read(self.name+0x20,4)==128
        assert read(self.name+0x18)==self.text
        self.u.mem_write(self.text,text.encode('utf-16-le')+b'\0\0')
        self.u.mem_write(self.item,struct.pack('<HBBI',item_id,0,0,0))
        parser=self.ram+0x1a0000
        self.u.reg_write(UC_ARM64_REG_X0,parser); self.call(0x1b0ae34)
        for reg,value in ((UC_ARM64_REG_X0,parser),(UC_ARM64_REG_X1,self.name),(UC_ARM64_REG_X2,self.item)):
            self.u.reg_write(reg,value)
        self.call(0x1b0b154)
        payload=text.encode('utf-16-le')+b'\0\0'
        # Hidden noun metadata actually survives native post-processing.
        assert bytes(self.u.mem_read(self.text,len(payload)))==payload

    def mark(self,item_id,owned,text,expected):
        self.native_name(item_id,text)
        bits=bytearray(0x754)
        if owned: bits[item_id//8]|=1<<(item_id%8)
        self.u.mem_write(self.bits,bytes(bits))
        context=bytes(self.u.mem_read(self.ctx,31*8))
        self.u.reg_write(UC_ARM64_REG_X0,self.ctx)
        self.call(self.mod+symbol('CabinetCatalogStar','Callback'))
        expected_bytes=expected.encode('utf-16-le')+b'\0\0'
        assert bytes(self.u.mem_read(self.text,len(expected_bytes)))==expected_bytes,(item_id,text,expected)
        assert bytes(self.u.mem_read(self.text,12))==text.encode('utf-16-le')[:12]
        assert bytes(self.u.mem_read(self.bits,0x754))==bytes(bits)
        assert bytes(self.u.mem_read(self.ctx,31*8))==context
        assert bytes(self.u.mem_read(self.item,8))==struct.pack('<HBBI',item_id,0,0,0)
        # Repeated callback on the same buffer must not accumulate stars.
        self.u.reg_write(UC_ARM64_REG_X0,self.ctx)
        self.call(self.mod+symbol('CabinetCatalogStar','Callback'))
        assert bytes(self.u.mem_read(self.text,len(expected_bytes)))==expected_bytes


def main():
    with tempfile.TemporaryDirectory(prefix='acnh-catalog-text-') as folder:
        archive=Path(folder)/'String.sarc'
        archive.write_bytes(subprocess.check_output(['zstd','-d','-q','-c',str(ROOT/'vanilla-3.0.3/Message/String_USen.sarc.zs')]))
        names={}
        for name,data in read_sarc(archive)[1]:
            if name.endswith(('STR_ItemName_50_RoomWall.msbt','STR_ItemName_51_RoomFloor.msbt')):
                path=Path(folder)/Path(name).name;path.write_bytes(data)
                names.update({int(label.rsplit('_',1)[1]):text for label,text in read_msbt(path)})
    q=NativeText(0);count=0
    for item_id,row in ids:
        text=names[item_id]
        assert text[:4]=='\x0e\x32\0\x04', (item_id,repr(text))
        assert 6<len(text)<123 and not any(ord(c)<32 for c in text[6:]),(item_id,repr(text))
        for owned in (False,True):
            expected=text if owned else text[:6]+'★ '+q.native_capitalized(text[6:])
            q.mark(item_id,owned,text,expected);count+=1
    # Cross-check the helper against every actual native case-table entry,
    # unchanged characters, and Dutch IJ (including the non-Dutch distinction).
    from catalog_query_arm_test import RAW
    for lower,upper in struct.iter_unpack('<HH',RAW[0x40a0fda:0x40a0fda+188*4]):
        text=chr(lower)+' wall'
        expected='★ '+q.native_capitalized(text)
        q.display(4969,text=text,expected=expected);count+=1
    for locale in (1,11):
        q.locale=locale
        for text in ('ijsvloer','Ijsvloer','IJsvloer','island','étoiles','ёжик','壁紙','😀 wall'):
            q.display(4969,text=text,expected='★ '+q.native_capitalized(text));count+=1
    q.locale=1
    # Malformed/unknown tags and embedded controls still leave the name unchanged.
    item_id=4969
    for text in ('\x0e\x31\0\x04\0\0wall','\x0e\x32\0\x06\0\0wall',
                 '\x0e\x32\0\x04\0\0','\x0e\x32\0\x04\0\0a\x0eb',
                 '\x0e\x32\0\x04\0\0'+'a'*120):
        q.display(item_id,text=text,expected=text);count+=1
    print(f'PASS: {count} native-text cases; all 527 real USen names, known/unknown, metadata preserved and repeat marking prevented')

if __name__=='__main__': main()
