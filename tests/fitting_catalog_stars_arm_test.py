"""Fitting-room hook regression with real selected-item caller and name buffer.

Historical membership and compiled hooks run at two load bases. At base zero,
the original fitting-room item-copy caller and shared balloon routine run through
the hook site; archive lookup is replaced with real local clothing-name strings.
No game assets are packaged. Rendering/purchasing still need Eden verification.
"""
import struct
import sys
import zlib
import tempfile
import subprocess
from pathlib import Path
from catalog_stars_arm_test import symbol, bcsv, field_offsets, nonwall
from catalog_native_text_test import NativeText, ROOT, read_sarc, read_msbt
from unicorn.arm64_const import *

KINDS={0xc0939284,0x55067543,0x0581ce14,0xdf4042c0,0xd50eae93,0xa17ec151,0x7d06eb90,0x2340a7bf}
CLOTHES=[(i,row) for i,row in nonwall if struct.unpack_from('<I',bcsv,row+field_offsets[0x5270eb75])[0] in KINDS]

class Fitting(NativeText):
    def __init__(self,base):
        super().__init__(base)
        self.page=self.ram+0x174000
        self.write(self.owner+0x220,self.page);self.write(self.page+0x218,self.window)
        self.write(self.frame+8,base+0x3114c14)
        self.builder_text=None

    def hook(self,u,pc,size,data):
        if pc-self.base==0x1b0bafc and self.builder_text is not None:
            assert self.reg(0)==self.name
            assert bytes(u.mem_read(self.reg(1),8))==bytes(u.mem_read(self.item,8))
            u.mem_write(self.text,self.builder_text.encode('utf-16-le')+b'\0\0')
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))
            return
        super().hook(u,pc,size,data)

    def native_selection(self,item_id,text):
        # Execute the actual focus callback's index->item selection and call.
        collection=self.ram+0x17a000;rows=collection+0x100;indices=rows+0x100
        self.write(collection,rows);self.write(rows,self.item)
        self.write(collection+0x18,1,4);self.write(collection+0x20,indices)
        self.write(collection+0x28,1,4);self.write(indices,0,2)
        controls=self.ram+0x17b000;label=controls+0x1000;renderer=label+0x100
        self.write(self.window+0x210,controls);self.write(controls+0x270,label)
        self.write(label+0x48,renderer)
        self.u.mem_write(self.item,struct.pack('<HBBI',item_id,0,0,0))
        original_sp=self.frame+0x60
        self.u.mem_write(original_sp,b'\0'*0xb0)
        self.builder_text=text
        for reg,value in ((UC_ARM64_REG_SP,original_sp),(UC_ARM64_REG_X19,self.owner),
                          (UC_ARM64_REG_X20,0),(UC_ARM64_REG_X21,self.window),
                          (UC_ARM64_REG_X22,collection)):
            self.u.reg_write(reg,value)
        self.u.emu_start(0x3114bb4,0x30117c8,count=50000)
        assert self.u.reg_read(UC_ARM64_REG_PC)==0x30117c8
        assert self.u.reg_read(UC_ARM64_REG_X29)==self.frame
        assert int.from_bytes(self.u.mem_read(self.frame+8,8),'little')==0x3114c14
        assert int.from_bytes(self.u.mem_read(self.frame+0x58,8),'little')==self.owner
        assert self.u.reg_read(UC_ARM64_REG_X19)==self.window
        assert self.u.reg_read(UC_ARM64_REG_X22)==original_sp+8
        for i in range(31):
            reg=UC_ARM64_REG_X0+i if i<29 else (UC_ARM64_REG_X29 if i==29 else UC_ARM64_REG_LR)
            self.write(self.ctx+i*8,self.u.reg_read(reg))
        self.u.reg_write(UC_ARM64_REG_X0,self.ctx)
        self.call(self.mod+symbol('CabinetCatalogStar','Callback'))
        expected=text[:6]+'★ '+self.native_capitalized(text[6:])
        assert bytes(self.u.mem_read(self.text,len(expected.encode('utf-16-le'))+2))==expected.encode('utf-16-le')+b'\0\0'
        self.builder_text=None


def main():
    count=0
    for base in (0x7100000000,0x7200000000):
        q=Fitting(base)
        for item_id,row in CLOTHES:
            for owned in (False,True):q.display(item_id,owned,text='Sample clothing');count+=1
        # 117 and 118 are different colors of the same short-sleeved uniform top.
        q.display(117,owned=True);count+=1
        q.u.mem_write(q.item,struct.pack('<HBBI',118,0,0,0))
        q.u.reg_write(UC_ARM64_REG_X0,q.ctx)
        q.call(q.mod+symbol('CabinetCatalogStar','Callback'))
        expected='★ Sample wall'.encode('utf-16-le')+b'\0\0'
        assert bytes(q.u.mem_read(q.text,len(expected)))==expected
        assert q.u.mem_read(q.bits+117//8,1)[0] & (1<<(117%8))
        assert not q.u.mem_read(q.bits+118//8,1)[0] & (1<<(118%8))
        count+=1
        item_id=CLOTHES[0][0]
        for address,bad,good,size in ((q.frame+8,base+0x3114c18,base+0x3114c14,8),
                (q.owner+0x220,0,q.page,8),(q.page+0x218,0,q.window,8),
                (q.owner+0x1f8,8,0,1),(q.other+0x1070,0,q.bits,8)):
            q.write(address,bad,size);q.display(item_id,expected='Sample wall');q.write(address,good,size);count+=1
        for other in (4969,5073,65535):q.display(other,expected='Sample wall');count+=1
        for kind,capacity,flags in ((3,128,0),(4,128,0),(0,127,0),(0,128,1)):
            q.display(item_id,expected='Sample wall',kind=kind,capacity=capacity,flags=flags);count+=1
        for owned in (False,True,False):q.display(item_id,owned);count+=1
    with tempfile.TemporaryDirectory(prefix='fitting-names-') as folder:
        p=Path(folder)/'String.sarc'
        p.write_bytes(subprocess.check_output(['zstd','-dq','-c',str(ROOT/'vanilla-3.0.3/Message/String_USen.sarc.zs')]))
        group_names={}
        for n,data in read_sarc(p)[1]:
            if 'STR_OutfitGroupName_' in n:
                f=Path(folder)/'names.msbt';f.write_bytes(data)
                group_names[n.rsplit('_',1)[1][:-5]]={int(label):text for label,text in read_msbt(f)}
        sys.path.insert(0,str(ROOT/'CylindricalEarth'))
        import bcsv as table_module, specs_200
        table=table_module.File(specs_200.ItemClothGroup)
        table.load((ROOT/'vanilla-3.0.3/Bcsv/ItemClothGroup.bcsv').read_bytes())
        groups={zlib.crc32(r.Label.encode()):r.UniqueID for r in table.rows}
        table=table_module.File(specs_200.ItemParam)
        table.load((ROOT/'vanilla-3.0.3/Bcsv/ItemParam.bcsv').read_bytes())
        names={r.UniqueID:group_names[r.ItemKind[0]][groups[int.from_bytes(r._data[0x690e3379],'little')]]
               for r in table.rows if r.ItemKind[0] in group_names and r.ItemKind[0] not in ('Bag','MarineSuit')}
    q=Fitting(0)
    # One actual native caller execution for each clothing kind.
    checked=set()
    for item_id,row in CLOTHES:
        kind=struct.unpack_from('<I',bcsv,row+field_offsets[0x5270eb75])[0]
        if kind not in checked:
            q.native_selection(item_id,names[item_id]);checked.add(kind);count+=1
    q=Fitting(0)
    for item_id,row in CLOTHES:
        text=names[item_id];first=6 if text.startswith('\x0e\x32\0\4') else 0
        supported=bool(text) and len(text)<126 and not any(ord(c)<32 for c in text[first:])
        expected=text[:first]+'★ '+q.native_capitalized(text[first:]) if supported else text
        q.display(item_id,text=text,expected=expected);count+=1
    print(f'PASS: {count} fitting-room cases; {len(CLOTHES)} clothing IDs, native selection/frame for all eight kinds, real USen names and two load bases')

if __name__=='__main__':main()
