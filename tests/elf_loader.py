"""ELF loader adapted from the workspace coexistence tests."""
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[2]
u16=lambda b,o:struct.unpack_from("<H",b,o)[0]
u64=lambda b,o:struct.unpack_from("<Q",b,o)[0]
class Module:
    def __init__(self,machine,folder,address):
        self.folder,self.address=folder,address
        e=(ROOT/folder/'project.elf').read_bytes();u=machine.u
        segments=[struct.unpack_from('<IIQQQQQQ',e,u64(e,32)+i*u16(e,54)) for i in range(u16(e,56))]
        # Match the ELF memory size, including .bss. Imports live after the
        # image so larger retained UI-resource buffers cannot overwrite stubs.
        image_size=(max(va+ms for typ,flags,off,va,pa,sz,ms,al in segments if typ==1)+0xfff)&~0xfff
        mapped_size=image_size+0x10000
        u.mem_map(address,mapped_size)
        for typ,flags,off,va,pa,sz,ms,al in segments:
            if typ==1:u.mem_write(address+va,e[off:off+sz])
        sections=[struct.unpack_from('<IIQQQQIIQQ',e,u64(e,40)+i*u16(e,58)) for i in range(u16(e,60))]
        self.symbols={};tables={}
        for index,s in enumerate(sections):
            if s[1] not in (2,11):continue
            names=sections[s[6]];strings=e[names[4]:names[4]+names[5]];table=[]
            for off in range(s[4],s[4]+s[5],s[9]):
                n,info,other,sec,val,size=struct.unpack_from('<IBBHQQ',e,off)
                name=strings[n:strings.find(b'\0',n)].decode();table.append((name,val,sec))
                if sec and name:self.symbols[name]=address+val
            tables[index]=table
        imports={}
        for s in sections:
            if s[1]!=4:continue
            for off in range(s[4],s[4]+s[5],s[9]):
                target,info,addend=struct.unpack_from('<QQq',e,off);typ=info&0xffffffff
                if typ==1027:value=address+addend
                elif typ in (257,1025,1026):
                    name,val,sec=tables[s[6]][info>>32]
                    if sec:value=address+val+addend
                    else:
                        if name not in imports:
                            imports[name]=address+image_size+16*len(imports)
                            machine.services[imports[name]]=('import',name)
                        value=imports[name]+addend
                else:raise AssertionError(typ)
                machine.write(address+target,value)
        machine.write(self.find('s_ModuleBitset'),6)
        machine.write(self.find('s_SelfModuleIdx'),2,4)
        main=self.find('s_ModuleInfos')+72
        for off,value in ((0,machine.base),(8,0x5800000),(16,machine.base),(24,0x3f07000),
                          (32,machine.base+0x3f07000),(40,0x81e000),
                          (48,machine.base+0x4725000),(56,0x18e1000),
                          (72,address),(80,mapped_size),(88,address),(96,mapped_size)):
            machine.write(main+off,value)
        machine.services[self.find('SvcLogger','LogRaw')]=('log',folder)
        machine.services[self.find('nx64','10InitializeEv')]=('initialize',folder)
        if any('HookInline' in name for name in self.symbols):
            machine.services[self.find('nx64','HookInline')]=('install',folder)
        machine.services[self.find('nx64','4HookEmmb')]=('install',folder)

    def find(self,*needles):
        if len(needles)==1 and needles[0] in self.symbols:return self.symbols[needles[0]]
        matches=[(k,v) for k,v in self.symbols.items() if all(n in k for n in needles)]
        if len(matches)>1:
            functions=[(k,v) for k,v in matches if not k.startswith('_ZZ') and '.part.' not in k]
            if len(functions)==1:matches=functions
        found=[v for k,v in matches]
        assert len(found)==1,(needles,found)
        return found[0]
