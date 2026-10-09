"""Native RSTC allocation lookup with the compiled, narrowly scoped size hook.

Executes 3.0.3 CRC32/binary search and the final ELF callback. Only SafeString's
termination virtual method is modeled. No global resource table is modified.
"""
from pathlib import Path
from compression import zstd
import re
import struct
import sys
import zlib
from core_arm_test import Machine,RAW,DEFAULT
from unicorn.arm64_const import *
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import build_shopping_card_layout as card
from resource_sizes import read_rstc,allocation

QUERY = 0x3e3cad0
PATH = 'Layout/LBellCounterRT.Nin_NX_NVN'
header = (card.ROOT/'source/program/shopping/resource_budget.hpp').read_text()
assert f'"{PATH}"' in header
MINIMUM = int(re.search(r'CounterResourceMinimumBytes = (\d+)',header)[1])
raw,count,names = read_rstc((card.ROOT.parent/'vanilla-3.0.3/System/Resource/ResourceSizeTable.srsizetable').read_bytes())
vanilla_budget = allocation(raw,count,PATH)
assert vanilla_budget == 77952
expanded = len(zstd.decompress(next(iter(card.build().values()))))
assert vanilla_budget < expanded and expanded+8192 <= MINIMUM
print(f'PASS: reproduces missing size allowance: native {vanilla_budget}, expanded archive {expanded}, scoped floor {MINIMUM}')

class ResourceMachine(Machine):
    def __init__(self,base,enabled,disabled=False):
        files = {'shopping-card':DEFAULT if enabled else DEFAULT.replace(b'true',b'false')} if enabled or disabled else {}
        super().__init__(base,files)
        self.boot();self.activate()
        assert (base+QUERY in self.hooks) == (enabled or disabled)
        assert (base+0x257abf0 in self.hooks) == enabled
        self.u.mem_write(base+QUERY,RAW[QUERY:QUERY+20])
        self.callback = self.module.find('CounterResourceBudget','Callback')
        self.write(self.module.find('CounterResourceBudget','OrigRef'),base+QUERY)
        self.manager,self.path,self.output,self.vtable = (self.ram+n for n in (0x8000,0x9000,0xa000,0xb000))
        self.table = self.ram+0x20000
        self.u.mem_write(self.table,raw)
        self.write(self.manager+8,1,1)
        self.write(self.manager+0x68,count,4)
        self.write(self.manager+0x70,self.table+12)
        self.write(self.manager+0x78,names,4)
        self.write(self.manager+0x80,self.table+12+count*12)
        self.write(self.path,self.vtable)
        self.write(self.path+8,self.path+0x100)
        self.write(self.vtable+0x18,base+0x1000)
        self.services[base+0x1000]=('terminate_resource_path',None)
    def service(self,u,pc,size,data):
        if self.services.get(pc) == ('terminate_resource_path',None):
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR));return
        super().service(u,pc,size,data)
    def query(self,path,hook):
        self.u.mem_write(self.path+0x100,path.encode()+b'\0')
        self.u.mem_write(self.output,bytes([0xa5])*8)
        for i,value in enumerate((self.manager,self.output,self.path)):
            self.u.reg_write(UC_ARM64_REG_X0+i,value)
        self.call(self.callback if hook else self.base+QUERY)
        result = bytes(self.u.mem_read(self.output,8))
        assert result[5:] == bytes([0xa5])*3
        return self.reg(0),result

for base in (0x7100000000,0x7200000000):
    m = ResourceMachine(base,True)
    native = m.query(PATH,False)
    changed = m.query(PATH,True)
    assert native[0] == changed[0] == 1
    assert int.from_bytes(native[1][:4],'little') == vanilla_budget
    assert int.from_bytes(changed[1][:4],'little') == MINIMUM
    assert changed[1][4:] == native[1][4:]
    # Exact scope: no substring/extension/other folder matches. Native failure
    # results and output bytes are preserved for unrelated/missing resources.
    for path in ('Layout/LNookMileage.Nin_NX_NVN','Layout/LBellCounterRT.Nin_NX_NVN.zs',
                 'Layout/LBellCounterRT.Nin_NX_NVN_extra','Other/LBellCounterRT.Nin_NX_NVN',''):
        assert m.query(path,True) == m.query(path,False)
    assert bytes(m.u.mem_read(m.table,len(raw))) == raw
    # Existing larger overrides and metadata are preserved.
    index = next(i for i in range(count) if struct.unpack_from('<I',raw,12+12*i)[0] == zlib.crc32(PATH.encode()))
    entry = m.table+12+12*index
    for value in (MINIMUM,MINIMUM+65536):
        m.write(entry+4,value,4);m.write(entry+8,1,1)
        assert m.query(PATH,True) == m.query(PATH,False)
    # Missing/empty table still receives the target floor and ordinary flags.
    m.write(m.manager+0x68,0,4);m.write(m.manager+0x78,0,4)
    found,data = m.query(PATH,True)
    assert found == 1 and int.from_bytes(data[:4],'little') == MINIMUM and data[4] == 0
    assert m.query('Layout/Unrelated',True) == m.query('Layout/Unrelated',False)
    off = ResourceMachine(base,False)
    assert int.from_bytes(off.query(PATH,False)[1][:4],'little') == vanilla_budget
    disabled = ResourceMachine(base,False,disabled=True)
    assert int.from_bytes(disabled.query(PATH,True)[1][:4],'little') == MINIMUM
    # A damaged resource-lookup entry rejects support and all Shopping Card behavior.
    conflict = Machine(base,{'shopping-card':DEFAULT})
    conflict.write(base+QUERY,0xdeadbeef,4)
    conflict.boot();conflict.activate()
    assert len(conflict.hooks) == 1 and 'resource support unavailable' in conflict.text
    print(f'PASS: native RSTC lookup plus compiled floor, exact scope, larger overrides/flags, missing table, disabled behavior, removal and conflict, base {base:#x}')
