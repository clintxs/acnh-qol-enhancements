"""Resolve DIY layout textures in its real BNTX, before capture rebinding.

Executes the game's texture dictionary traversal and texture-resource getter.
The loader's handle construction and GPU view are modeled. This reproduces
an unresolved reference's native null read, not the full asynchronous UILoad.
"""
from pathlib import Path
from unittest.mock import patch
import struct
from unicorn import Uc, UcError, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.arm64_const import *
from recipe_preview_fixture import PreviewMachine, MENU, ASSETS, RAW, resources, layout_sections, material_records, zstd

original=resources(zstd.decompress((ASSETS/'MenuRecipe.Nin_NX_NVN.zs').read_bytes()))
container=next(v for k,v in original.items() if k.endswith('.bntx'))
read=lambda d,o,f='I':struct.unpack_from('<'+f,d,o)[0]

def names(layout):
    table=next(s for s in layout_sections(layout) if s[:4]==b'txl1')
    return [table[12+read(table,12+4*i):].split(b'\0',1)[0].decode() for i in range(read(table,8,'H'))]

class NativeTextures:
    def __init__(self,base):
        self.base=base;self.ram=base+0x6000000
        self.u=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
        for start,end in ((0x263000,0x264000),(0x31a9000,0x31ab000),(0x3f03000,0x3f04000)):
            self.u.mem_map(base+start,end-start);self.u.mem_write(base+start,RAW[start:end])
        self.u.mem_map(self.ram,0x100000)
        data=bytearray(container);self.dic=read(data,56,'Q')
        for i in range(read(data,36)+1):
            offset=self.dic+16+i*16
            struct.pack_into('<Q',data,offset,self.ram+read(data,offset,'Q'))
        self.u.mem_write(self.ram,bytes(data));self.result=None;self.faults=[]
        self.u.hook_add(UC_HOOK_CODE,self.service)
        self.u.hook_add(UC_HOOK_MEM_INVALID,self.fault)
    def fault(self,u,access,address,size,value,data):
        self.faults.append((address,u.reg_read(UC_ARM64_REG_PC)));return False
    def service(self,u,pc,size,data):
        if pc==self.base+0x3f03380:
            def string(p):
                out=bytearray()
                while (b:=u.mem_read(p,1)[0]):out.append(b);p+=1
                return out
            u.reg_write(UC_ARM64_REG_X0,int(string(u.reg_read(UC_ARM64_REG_X0))!=string(u.reg_read(UC_ARM64_REG_X1))))
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))
        elif pc in (self.base+0x26346c,self.base+0x2633c0):
            self.result=pc==self.base+0x26346c;u.emu_stop()
    def resolves(self,name):
        string=self.ram+0x80000;self.u.mem_write(string,name.encode()+b'\0')
        for r,v in ((UC_ARM64_REG_X20,self.ram+self.dic),(UC_ARM64_REG_X19,string),(UC_ARM64_REG_X2,len(name))):self.u.reg_write(r,v)
        self.result=None;self.u.emu_start(self.base+0x263368,self.base+0x263480,count=10000)
        assert self.result is not None
        return self.result
    def texture_view(self,name):
        handle=self.ram+0x90000;resource=handle+0x100;view=handle+0x200;stop=handle+0x300
        self.u.mem_write(handle+0x10,struct.pack('<Q',resource if self.resolves(name) else 0))
        self.u.mem_write(resource+0x88,struct.pack('<Q',view))
        self.u.reg_write(UC_ARM64_REG_X0,handle);self.u.reg_write(UC_ARM64_REG_LR,stop)
        self.u.emu_start(self.base+0x31aa0b0,stop,count=10)
        assert self.u.reg_read(UC_ARM64_REG_PC)==stop
        return self.u.reg_read(UC_ARM64_REG_X0)==view

baseline=Path(__file__).resolve().parents[1]/'dist/switch-startup-fix/original/project.elf'
for base in (0x7100000000,0x7200000000):
    native=NativeTextures(base)
    assert not native.resolves('Capture^t') and native.resolves('White_00^r')
    if baseline.exists():
        old_elf=baseline.read_bytes();reader=Path.read_bytes
        def preserved(path):
            return old_elf if path.name=='project.elf' and path.parent.name=='qol-enhancements' else reader(path)
        with patch.object(Path,'read_bytes',preserved): old=PreviewMachine(base)
        old_names=names(old.patch(MENU))
        assert [name for name in old_names if not native.resolves(name)]==['Capture^t']
    try: native.texture_view('Capture^t')
    except UcError: pass
    else: raise AssertionError('missing texture did not reproduce native null resource read')
    assert native.faults[-1]==(0x88,base+0x31aa0b4)
    current=PreviewMachine(base);patched=current.patch(MENU)
    assert names(patched)==names(MENU)
    assert all(native.resolves(name) for name in names(patched))
    assert native.texture_view('White_00^r')
    parts=layout_sections(patched)
    material=material_records(next(s for s in parts if s[:4]==b'mat1'))[-1]
    assert names(patched)[read(material,44,'H')]=='White_00^r'
    # Unknown custom layout backing: decline modification instead of inventing
    # a texture. Its ordinary DIY grid remains available.
    without_white=MENU.replace(b'White_00^r',b'Absent00^r')
    assert current.patch(without_white)==without_white
    print(f'PASS: native missing Capture^t lookup/getter reproduces main+31AA0B4 read 88; compiled patch uses only local textures and preserves unsupported layouts, base {base:#x}')
