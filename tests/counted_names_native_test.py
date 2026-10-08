"""Reproduce fence grammar with native stack/count getters and name-type selection.

ItemKind is local, unshipped game data. Only the fence's resolved enum index is
needed; this fixture does not pretend to rebuild every ItemParam kind mapping.
"""
from storage_ground_arm_test import NativeGround
from catalog_stars_arm_test import ROOT,bcsv,field_offsets,nonwall,ids
from unicorn.arm64_const import *
import struct
q=NativeGround(0)
raw=bytearray((ROOT/'vanilla-3.0.3/Bcsv/ItemKind.bcsv').read_bytes())
n,stride,fields,v=struct.unpack_from('<IIHB',raw);header=28 if v else 12
columns=dict(struct.unpack_from('<II',raw,header+8*i)for i in range(fields))
kind_by_hash={}
start=header+8*fields
for i in range(n):
 row=start+i*stride;struct.pack_into('<I',raw,row,row)
 kind_by_hash[struct.unpack_from('<I',raw,row+columns[0x6b749e8a])[0]]=i
kind_data=0x23000000;q.u.mem_map(kind_data,0x10000);q.u.mem_write(kind_data,bytes(raw))
kt=q.ram+0x1d0000;rows=kt+0x1000;cache=kt+0x2000;index=kt+0x4000
q.write(0x522a778,kt);q.write(kt+0x1c0,n,4);q.write(kt+0x1b0,n,4);q.write(kt+0x1b8,rows)
for i in range(n):q.write(rows+i*8,kind_data+start+i*stride)
q.write(0x4abfeb0,cache+0x1000);q.write(cache+0x1000,1,1)
q.write(0x4abfeb8,cache);q.u.mem_write(cache,b'\xff'*0x200)
q.write(0x4ac00c8,index);q.write(index,len(bcsv),4);q.write(index+8,index+0x100)
item_header=28+8*struct.unpack_from('<H',bcsv,8)[0];item_stride=struct.unpack_from('<I',bcsv,4)[0]
fence_row=dict(nonwall)[13275]
fence_kind=struct.unpack_from('<I',bcsv,fence_row+field_offsets[0x5270eb75])[0]
q.write(index+0x100+2*((fence_row-item_header)//item_stride),kind_by_hash[fence_kind],2)
accessor=q.ram+0x17b000
row=dict(nonwall)[13275]
q.write(accessor+0x10,q.data+row)
q.u.mem_write(accessor+8,struct.pack('<HBBI',13275,0,0,0))
q.u.reg_write(UC_ARM64_REG_X0,accessor);q.call(0x19f0020);assert q.reg(0)==1
q.u.reg_write(UC_ARM64_REG_X0,accessor);q.call(0x19f02d0);assert q.reg(0)==1
q.write(q.name,0x493f8b8)
for payload in (0,9):
 q.write(accessor+12,payload,4)
 for reg,value in ((UC_ARM64_REG_X19,q.name),(UC_ARM64_REG_X20,accessor),(UC_ARM64_REG_X23,1),(UC_ARM64_REG_SP,q.sp)):
  q.u.reg_write(reg,value)
 q.u.emu_start(0x1b0c358,0x1b0c378,count=50000)
 name_type=int.from_bytes(q.u.mem_read(q.name+8,4),'little')
 assert name_type==(1 if payload==0 else 2)
 for learned in (False,True):
  plural=q.name+0x2d0
  q.write(q.name+0x1f8,plural);q.write(q.name+0x200,128,4)
  q.u.mem_write(plural,'spooky fences\0'.encode('utf-16-le'))
  q.check(737,learned,kind=name_type,text='spooky fence',
          expected='spooky fence' if learned else '★ Spooky fence')
  expected='spooky fences' if learned else '★ Spooky fences'
  assert bytes(q.u.mem_read(plural,(len(expected)+1)*2))==(expected+'\0').encode('utf-16-le')
print('PASS: real fence metadata produces native name types 1/2; compiled hook marks both forms only when recipe 737 is unlearned')
