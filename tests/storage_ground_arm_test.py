"""Compiled storage/ground hooks, native name buffers and real recipe/catalog reads.

No live save is used. The common fixtures protect history and source item data
against ARM writes. Ground furniture cases execute native variant-history code.
"""
import struct
from pocket_recipe_arm_test import Recipes, DIY_IDS, COOKING_IDS, recipe_ids
from catalog_variants_arm_test import Variants, furniture
from catalog_stars_arm_test import symbol
from unicorn.arm64_const import *

class Storage(Recipes):
    def __init__(self, base):
        super().__init__(base)
        self.owner=base+0x30000000
        self.u.mem_map(self.owner,0xab000)
        self.write(self.owner,base+0x4a16028)
        self.write(self.owner+0xaa9d8,self.window)
        self.write(self.frame+0x58,self.owner)
        self.write(self.frame+8,base+0x316a5a0)

class GroundContext:
    def __init__(self, base):
        super().__init__(base)
        self.name=self.frame-0xaf0;self.text=self.name+0xf0
        self.write(self.ctx+21*8,self.item)
        self.write(self.frame-0xb11,0,1)

    def call(self, address):
        if address==self.mod+symbol('CabinetCatalogStar','Callback'):
            address=self.mod+symbol('GroundCatalogStar','Callback')
        return super().call(address)

class GroundRecipes(GroundContext,Recipes):
    def check(self,*args,**kwargs):
        self.write(self.frame-0xb11,kwargs.get('player',0),1)
        return super().check(*args,**kwargs)

class GroundFurniture(GroundContext,Variants):
    pass

# Real native constructors write the name buffer; only resource lookup and
# memset are substituted. Preserve noun tags exactly as on the existing UIs.
class NativeName:
    def hook(self,u,pc,size,data):
        if pc==0x3f04450:
            u.mem_write(self.reg(0),bytes([self.reg(1)&255])*self.reg(2))
        elif pc==0x1b0bafc:
            assert self.reg(0)==self.name
            assert bytes(u.mem_read(self.reg(1),8))==self.selected_item
            u.mem_write(self.text,self.localized_name.encode('utf-16-le')+b'\0\0')
        else:
            return super().hook(u,pc,size,data)
        u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))

class NativeStorage(NativeName,Storage): pass
class NativeGround(NativeName,GroundRecipes): pass

def copy_context(q):
    for i in range(31):
        reg=UC_ARM64_REG_X0+i if i<29 else (UC_ARM64_REG_X29 if i==29 else UC_ARM64_REG_LR)
        q.write(q.ctx+i*8,q.u.reg_read(reg))


def main():
    count=0
    for base in (0x7100000000,0x7200000000):
        for cls in (Storage,GroundRecipes):
            q=cls(base)
            for recipe in recipe_ids:
                q.check(recipe);q.check(recipe,True,expected='wooden chair');count+=2
            for recipe in (DIY_IDS[0],COOKING_IDS[0]):
                for player in range(8):
                    q.mapped_player=7-player
                    q.check(recipe,player=player,locked=True)
                    assert q.lock_calls==[0x200,0x204]
                    q.check(recipe,True,player=player,expected='wooden chair');count+=2
                q.mapped_player=0
                for player in (8,255):q.check(recipe,player=player,expected='wooden chair');count+=1
                for flags in (1,2,3):q.check(recipe,flags=flags,expected='wooden chair');count+=1
                for capacity,kind in ((127,0),(128,3)):
                    q.check(recipe,capacity=capacity,kind=kind,expected='wooden chair');count+=1
                for learned in (False,True,False):
                    q.check(recipe,learned,expected='wooden chair' if learned else '★ Wooden chair');count+=1
            if cls is Storage:
                for mode in (1,2,255):
                    q.write(q.owner+0x220,mode,4);q.check(DIY_IDS[0],expected='wooden chair');count+=1
                q.write(q.owner+0x220,0,4)
                for addr in (q.owner,q.owner+0xaa9d8,q.frame+8,q.frame+0x58):
                    old=bytes(q.u.mem_read(addr,8));q.write(addr,0)
                    q.check(DIY_IDS[0],expected='wooden chair');q.u.mem_write(addr,old);count+=1
                q.check(DIY_IDS[0],item_id=1286,expected='wooden chair');count+=1
        q=GroundFurniture(base)
        for item,remake,variants in furniture:
            for known,body,marker in ((False,None,'★'),(True,0,''),(True,1,'☆')):
                before=q.history(remake,body)
                q.display(item,owned=known,text='furniture',expected=(marker+' Furniture') if marker else 'furniture')
                assert bytes(q.u.mem_read(q.variants,2000))==before
                count+=1
        q.write(q.frame-0xb11,8,1)
        q.display(furniture[0][0],expected='Sample wall');count+=1

    for cls in (NativeStorage,NativeGround):
        q=cls(0)
        for recipe in (DIY_IDS[0],COOKING_IDS[0]):
            for learned in (False,True):
                q.selected_item=struct.pack('<HBBI',5794,0,0,recipe)
                q.localized_name='\x0e\x32\0\x04\0\0wooden chair'
                bits=bytearray(512)
                if learned:bits[recipe//8]|=1<<(recipe%8)
                q.u.mem_write(q.history,bytes(bits));q.u.mem_write(q.item,q.selected_item)
                q.write(q.player,0,1)
                if cls is NativeStorage:
                    controls=q.ram+0x17b000;label=controls+0x1000;renderer=label+0x100
                    q.write(q.window+0x210,controls);q.write(controls+0x270,label);q.write(label+0x48,renderer)
                    caller_sp=q.frame+0x60;caller_fp=caller_sp+0xb0
                    q.u.mem_write(caller_sp,b'\0'*0x180)
                    q.u.mem_write(caller_fp-0x1c,q.selected_item)
                    for reg,value in ((UC_ARM64_REG_SP,caller_sp),(UC_ARM64_REG_X29,caller_fp),
                                      (UC_ARM64_REG_X19,q.owner),(UC_ARM64_REG_X25,q.owner+0xaa8d0),
                                      (UC_ARM64_REG_X22,controls),(UC_ARM64_REG_X1,caller_sp)):
                        q.u.reg_write(reg,value)
                    q.u.emu_start(0x316a56c,0x30117c8,count=50000)
                    assert q.u.reg_read(UC_ARM64_REG_PC)==0x30117c8
                    assert int.from_bytes(q.u.mem_read(q.frame+8,8),'little')==0x316a5a0
                    assert int.from_bytes(q.u.mem_read(q.frame+0x58,8),'little')==q.owner
                else:
                    sp=q.frame-0xb60
                    for reg,value in ((UC_ARM64_REG_SP,sp),(UC_ARM64_REG_X29,q.frame),
                                      (UC_ARM64_REG_X27,q.name),(UC_ARM64_REG_X21,q.item),
                                      (UC_ARM64_REG_X23,0xffffffffffffffff),
                                      (UC_ARM64_REG_X20,0xffffffff00000000),(UC_ARM64_REG_X25,0x40)):
                        q.u.reg_write(reg,value)
                    q.write(sp+0x4f,0,1)
                    q.u.emu_start(0x2ddc568,0x2ddc74c,count=50000)
                    assert q.u.reg_read(UC_ARM64_REG_PC)==0x2ddc74c
                assert q.u.reg_read(UC_ARM64_REG_X29)==q.frame
                assert int.from_bytes(q.u.mem_read(q.name+0x18,8),'little')==q.text
                assert int.from_bytes(q.u.mem_read(q.name+0x20,4),'little')==128
                copy_context(q);q.u.reg_write(UC_ARM64_REG_X0,q.ctx)
                q.call(q.mod+symbol('CabinetCatalogStar','Callback'))
                expected=q.localized_name if learned else q.localized_name[:6]+'★ Wooden chair'
                assert bytes(q.u.mem_read(q.text,len(expected.encode('utf-16-le'))+2))==(expected+'\0').encode('utf-16-le')
                assert bytes(q.u.mem_read(q.history,512))==bytes(bits)
                count+=1
    print(f'PASS: {count} storage/ground cases; all DIY/cooking IDs, furniture filled/hollow/none, '
          'two bases, native buffer construction, storage scope and per-player history')


if __name__ == "__main__": main()
