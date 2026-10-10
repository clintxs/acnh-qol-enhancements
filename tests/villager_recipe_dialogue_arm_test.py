"""Villager craft target uses actual recipe knowledge, not result ownership.

Execute the compiled hook and native DIY/cooking learned getters at two ASLR
bases. Stub only NPC type/save services; the save-view read/lock runs in C++.
Inherited checks preserve source items, history, noun metadata and registers.
"""
from dialogue_item_stars_arm_test import Dialogue, SITES, DIY_IDS, COOKING_IDS
from unicorn import UC_PROT_READ
from unicorn.arm64_const import *

class Villager(Dialogue):
    def __init__(self, base):
        super().__init__(base)
        self.npc_memory = base + 0x34000000
        self.u.mem_map(self.npc_memory, 0x20000)
        self.npc = self.npc_memory
        self.npc_view = self.npc + 0x1000
        self.npc_data = self.npc + 0x2000
        self.npc_type = True
        self.npc_view_present = True
        self.acquired = True
        self.npc_locks = []
        self.write(self.event+0x48,self.npc)
        self.write(base+0x4ab37d8,self.npc+0x800)
        self.write(self.npc+0x800,1,1)
        self.write(base+0x4ab37d0,self.npc+0x808)
        self.write(self.npc,self.npc+0x900)
        self.write(self.npc+0x920,base+0x350)
        self.write(self.npc+0x508,7,4)
        self.write(self.npc_view+8,self.npc_data-0x100)
        self.write(self.npc_view+0x10,0x100)
        self.write(self.npc_view+0x28,self.npc+0xa00)
        for off, stub in ((0,0x354),(8,0x358)):
            obj=self.npc+0xb00+off*4
            self.write(self.npc+0xa00+off,obj)
            self.write(obj,obj+8)
            self.write(obj+8,base+stub)
        self.u.mem_protect(self.npc_data, 0x18000, UC_PROT_READ)
        self.select(SITES[-1])
        self.write(self.action+0xd8,16,4)

    def hook(self,u,pc,size,data):
        off=pc-self.base
        if off==0x255c2a0:
            assert self.reg(0)==7
            result=self.npc_view if self.npc_view_present else 0
        elif off==0x350:
            assert self.reg(0)==self.npc and self.reg(1)==self.npc+0x808
            result=int(self.npc_type)
        elif off in (0x354,0x358):
            self.npc_locks.append(off)
            result=int(self.acquired)
        else:
            return super().hook(u,pc,size,data)
        u.reg_write(UC_ARM64_REG_X0,result)
        u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))

count=0
for base in (0x7100000000,0x7200000000):
    q=Villager(base)
    for recipe in (DIY_IDS[0],COOKING_IDS[0],737):
        q.write(q.npc_data+0x13154,recipe,2)
        for player in range(8):
            q.mapped_player=7-player
            for kind in (0,1,2):
                for owned in (False,True):
                    for learned in (False,True):
                        q.npc_locks.clear()
                        q.check_name(117,'' if learned else '★',owned=owned,
                                     extra=recipe,learned=learned,kind=kind,player=player)
                        assert q.npc_locks==[0x354,0x358]
                        count+=1
    q.mapped_player=0
    # Missing/invalid context fails closed even though the result is uncataloged.
    q.npc_type=False;q.check_name(117);q.npc_type=True
    q.npc_view_present=False;q.check_name(117);q.npc_view_present=True
    q.write(q.event+0x48,0);q.check_name(117);q.write(q.event+0x48,q.npc)
    q.write(q.npc+0x800,0,1);q.check_name(117);q.write(q.npc+0x800,1,1)
    q.write(q.npc_view+8,0);q.check_name(117);q.write(q.npc_view+8,q.npc_data-0x100)
    q.have_save=False;q.check_name(117);q.have_save=True
    for invalid in (0x7ff,0x800,0xffff):
        q.write(q.npc_data+0x13154,invalid,2);q.check_name(117)
    recipe=DIY_IDS[0]
    q.write(q.npc_data+0x13154,recipe,2)
    q.acquired=False;q.npc_locks.clear()
    q.check_name(117,'★');assert q.npc_locks==[0x354]
    q.write(q.npc_view+0x28,0);q.check_name(117,'★')
    # Ordinary dialogue retains catalog semantics regardless of known recipe.
    q.write(q.action+0xd8,1,4)
    q.check_name(117,'★',extra=recipe,learned=True)
    q.check_name(117,owned=True,extra=recipe)
print(f'villager recipe dialogue: {count} learned/catalog/category/player/form combinations plus failure/scope checks passed')
