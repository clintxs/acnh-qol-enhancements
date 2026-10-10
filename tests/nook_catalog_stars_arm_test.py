"""Compiled Nook message hook + native history, eligibility and dialogue copy.

Two ASLR bases test the hook with name/registration service doubles. At base zero,
execute the real name-object constructor and 01ac09d8/01ac0258 owned text copy;
only the resource lookup/name builder and heap allocation are substituted.
Local game assets are required and never included in the release.
"""
import struct
import subprocess
import tempfile
from pathlib import Path
from catalog_stars_arm_test import Stars, symbol, bcsv, field_offsets, nonwall
from catalog_native_text_test import NativeText, ROOT, read_sarc, read_msbt
from unicorn.arm64_const import *

FURNITURE=[i for i,row in nonwall if struct.unpack_from('<I',bcsv,row+field_offsets[0x5270eb75])[0]==0x7e0e8482]

class Nook(NativeText):
    def __init__(self,base,native=False):
        super().__init__(base)
        self.native=native; self.action=self.ram+0x175000; self.event=self.ram+0x176000
        self.holder=self.ram+0x177000; self.result=self.ram+0x178000
        self.bank=self.ram+0x179000; self.label=self.bank+0x100
        self.formatter=self.ram+0x17a000
        self.heap=self.ram+0x1b0000; self.key=self.ram+0x17b000
        self.write(self.action,base+0x4833a58); self.write(self.action+8,self.event)
        self.write(self.event+0x290,self.holder); self.write(self.event+0x2a8,0,1)
        self.write(self.holder+0x20,self.holder+0x100)
        self.write(self.holder+0x100,self.holder+0x200)
        self.write(self.holder+0x208,self.formatter)
        self.write(base+0x4ab2230,self.result+0x300)
        self.write(self.result+0x300,self.result+0x400)
        self.write(self.result+0x428,self.result)
        self.write(self.ctx,self.action); self.write(self.ctx+8,self.bank); self.write(self.ctx+16,self.label)
        # Observed live dispatch strips directories from the full message ID.
        self.string(self.bank,'SP_rcm_00_common'); self.string(self.label,'101')
        self.name_type=0;self.capacity=128;self.shift=0
        self.text_value='\x0e\x32\0\x04\0\0blue chair'
        self.registered=[];self.constructed=[];self.copy_objects=[]
        if native:
            self.write(base+0x4ab2698,self.key+0x100)
            self.write(self.key+0x100,0)
            self.write(self.formatter+0x18,self.formatter+0x18)
            self.write(self.formatter+0x2c,8,4)
            self.u.mem_write(self.key,struct.pack('<III',125,0,0))

    def string(self,address,text):
        self.write(address+8,address+0x20)
        self.u.mem_write(address+0x20,text.encode()+b'\0')

    def hook(self,u,pc,size,data):
        off=pc-self.base
        if off==0x1b0b8d4 and not self.native or off==0x1b0bafc and self.native:
            name=self.reg(0)
            self.constructed.append(bytes(u.mem_read(self.reg(1),8)))
            self.write(name+8,self.name_type,4); self.write(name+0x18,name+0xf0+self.shift)
            self.write(name+0x20,self.capacity,4)
            u.mem_write(name+0xf0,self.text_value.encode('utf-16-le')+b'\0\0')
        elif off==0x1aaf070:
            assert self.reg(0)==self.formatter and self.reg(2)==0
            name=self.reg(1);text=int.from_bytes(u.mem_read(name+0x18,8),'little')
            payload=bytes(u.mem_read(text,len(self.text_value.encode('utf-16-le'))+6))
            self.registered.append(payload)
            if self.native:
                # Actual native owned-string entry creation, used by 01aaf070.
                u.reg_write(UC_ARM64_REG_X0,self.formatter)
                u.reg_write(UC_ARM64_REG_X1,self.key)
                u.reg_write(UC_ARM64_REG_X2,name+0x10)
                u.reg_write(UC_ARM64_REG_PC,0x1ac09d8)
                return
        elif off==0x2478c0 and self.native:
            allocation=self.heap; self.heap+=(self.reg(0)+15)&~15
            assert self.heap<self.ram+0x1d0000
            self.copy_objects.append((allocation,self.reg(0)))
            u.reg_write(UC_ARM64_REG_X0,allocation)
        else:
            return super().hook(u,pc,size,data)
        u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))

    def check(self,item_id,owned=False,marked=True,flags=0,payload=0,player=0,marker='★'):
        item=struct.pack('<HBBI',item_id,0,flags,payload)
        self.u.mem_write(self.result+0x2c,item)
        self.write(self.event+0x2a8,player,1);self.write(self.player,player,1)
        bits=bytearray(0x754)
        if owned: bits[item_id//8]|=1<<(item_id%8)
        self.u.mem_write(self.bits,bytes(bits))
        context=bytes(self.u.mem_read(self.ctx,31*8))
        old=len(self.registered)
        self.u.reg_write(UC_ARM64_REG_X0,self.ctx)
        self.call(self.mod+symbol('NookDisplayCatalogStar','Callback'))
        assert len(self.registered)==old+int(marked),(item_id,owned,self.logs[-1:])
        if marked:
            first=6 if self.text_value.startswith('\x0e') else 0
            expected=self.text_value[:first]+marker+' '+self.text_value[first:]+'\0'
            assert self.registered[-1]==expected.encode('utf-16-le')
            assert self.constructed[-1]==item
        assert bytes(self.u.mem_read(self.bits,0x754))==bytes(bits)
        assert bytes(self.u.mem_read(self.result+0x2c,8))==item
        assert bytes(self.u.mem_read(self.ctx,31*8))==context


def main():
    count=0
    for base in (0x7100000000,0x7200000000):
        q=Nook(base)
        for item in FURNITURE:
            for owned in (False,True):
                q.check(item,owned,not owned);count+=1
        item=FURNITURE[0]
        # Execute the actual dispatcher pointer selection, then compare the hook.
        # A direct-only context reproduces 0.5.11's missing formatter: +20 is null.
        for direct,reference,formatter,marked in (
            (q.holder+0x200,0,q.formatter,True),
            (q.holder+0x200,1,q.formatter,True), # Poisoned fallback must not be read.
            (0,q.holder+0x100,q.formatter,True),
            (q.holder+0x200,q.holder+0x100,0,False),
        ):
            q.write(q.holder+0x10,direct);q.write(q.holder+0x20,reference)
            q.write(q.holder+0x208,formatter)
            q.u.reg_write(UC_ARM64_REG_X19,q.action)
            q.u.emu_start(base+0x9d886c,base+0x9d8890,count=20)
            assert q.u.reg_read(UC_ARM64_REG_PC)==base+0x9d8890
            assert q.u.reg_read(UC_ARM64_REG_X21)==q.holder+0x200
            q.check(item,marked=marked);count+=1
        q.write(q.holder+0x10,0);q.write(q.holder+0x20,q.holder+0x100)
        q.write(q.holder+0x208,q.formatter)
        for bank,label in [('SP_rcm_00_common','102'),('SP_rcm_00_common','4101'),
                           ('SP_rcm_00_common','1010'),('SP_rcm_00_common','103'),
                           ('SP_rcm_00_common','902_01'),('SYS_CloseMenu','011'),('SYS_Save','001'),
                           ('TalkSNpc/rcm/SP_rcm_00_common','101'),('SP_rcm_00_common_extra','101'),('other','101')]:
            q.string(q.bank,bank);q.string(q.label,label);q.check(item,marked=False);count+=1
        q.string(q.bank,'SP_rcm_00_common');q.string(q.label,'101')
        for invalid in (4969,5073,65535):q.check(invalid,marked=False);count+=1
        for player in (8,255):q.check(item,player=player,marked=False);count+=1
        for flag in (1,2,3):q.check(item,flags=flag,marked=False);count+=1
        for payload in (0,1,0x12345678):q.check(item,payload=payload);count+=1
        for text in ('\x0e\x31\0\4\0\0name','a\nb','a'*126,'★ name',''):
            q.text_value=text;q.check(item,marked=False);count+=1
        q.text_value='blue chair'
        for name,value in [('name_type',1),('capacity',127),('shift',2)]:
            old=getattr(q,name);setattr(q,name,value);q.check(item,marked=False);setattr(q,name,old);count+=1
        for address in (q.action,q.action+8,q.event+0x290,q.holder+0x20,q.holder+0x100,q.holder+0x208,q.result+0x428):
            old=bytes(q.u.mem_read(address,8));q.write(address,0);q.check(item,marked=False);q.u.mem_write(address,old);count+=1
        q.have_save=False;q.check(item,marked=False);q.have_save=True;count+=1
        q.check(item,marked=True);q.check(item,owned=True,marked=False);count+=2
    # Trace entries are bounded, preserve all registers, and reserve a separate
    # budget for Nook messages so startup/unrelated dialogue cannot hide them.
    q=Nook(0x7300000000)
    from catalog_stars_arm_test import symbols
    verbose = any('NookMessageTrace' in name and 'Callback' in name for name in symbols)
    if verbose:
        full=q.bank+0x300
        q.string(full,'TalkNNpc/other:1');q.write(q.ctx+16,full)
        snapshot=bytes(q.u.mem_read(q.ctx,31*8))
        for _ in range(60):
            q.u.reg_write(UC_ARM64_REG_X0,q.ctx)
            q.call(q.mod+symbol('NookMessageTrace','Callback'))
            assert bytes(q.u.mem_read(q.ctx,31*8))==snapshot
        assert not q.logs # Even a diagnostic build requires log_debug=true.
        q.string(full,'TalkSNpc/rcm/SP_rcm_00_common:101')
        q.u.reg_write(UC_ARM64_REG_X0,q.ctx);q.call(q.mod+symbol('NookMessageTrace','Callback'))
        assert not q.logs
    q.write(q.ctx+16,q.label)
    q.write(q.action,0);q.check(FURNITURE[0],marked=False)
    assert not q.logs
    assert not q.logs
    q.write(q.action,q.base+0x4833a58)
    q.write(q.holder+0x20,0);q.check(FURNITURE[0],marked=False)
    assert not q.logs
    count+=63 if verbose else 2
    with tempfile.TemporaryDirectory(prefix='nook-names-') as folder:
        archive=Path(folder)/'String.sarc'
        archive.write_bytes(subprocess.check_output(['zstd','-d','-q','-c',str(ROOT/'vanilla-3.0.3/Message/String_USen.sarc.zs')]))
        path=Path(folder)/'Ftr.msbt'
        path.write_bytes(dict(read_sarc(archive)[1])['Item/STR_ItemName_00_Ftr.msbt'])
        names={int(label.rsplit('_',1)[1]):text for label,text in read_msbt(path)}
    q=Nook(0,native=True)
    for item_id in FURNITURE:
        q.text_value=names[item_id]
        first=6 if q.text_value.startswith('\x0e') else 0
        if first: assert q.text_value[:4]=='\x0e\x32\0\4'
        supported=len(q.text_value)<126 and not any(ord(c)<32 for c in q.text_value[first:])
        q.heap=q.ram+0x1b0000;q.copy_objects.clear()
        q.write(q.formatter+0x18,q.formatter+0x18);q.write(q.formatter+0x28,0,4)
        q.check(item_id,marked=supported);count+=1
        if not supported:
            assert not q.copy_objects
            continue
        assert len(q.copy_objects)==2,q.copy_objects
        entry,_=q.copy_objects[0];buf,size=q.copy_objects[1]
        expected=q.registered[-1]
        actual=bytes(q.u.mem_read(buf,size))
        assert expected==actual,(item_id,expected,actual)
        assert int.from_bytes(q.u.mem_read(entry+0x38,8),'little')==buf
        # Overwrite callback stack: copied text survives deferred display.
        q.u.mem_write(q.sp-0x4000,b'\xcc'*0x4000)
        assert bytes(q.u.mem_read(buf,size))==expected
    print(f'PASS: {count} Nook hook cases; {len(FURNITURE)} furniture IDs, two bases, scope/guards, purchase recheck, native constructor and owned text copy')

if __name__=='__main__':main()
