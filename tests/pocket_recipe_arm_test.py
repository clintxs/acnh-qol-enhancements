"""Compiled pockets hook + real native learned-recipe getter at two ASLR bases.

Player/save resolution and lock callbacks are substituted; DIY 0251e658 and
cooking 0251288c execute unchanged. Real RecipeCraftParam IDs seed the runtime validity mask. History,
items and metadata are protected read-only to ARM; no user save is used.
"""
import struct
from catalog_stars_arm_test import Stars, symbol, ROOT
from unicorn import UC_PROT_READ
from unicorn.arm64_const import *

recipe_data = (ROOT/'vanilla-3.0.3/Bcsv/RecipeCraftParam.bcsv').read_bytes()
rows, stride, fields, version = struct.unpack_from('<IIHB', recipe_data)
header = 28 if version else 12
columns = dict(struct.unpack_from('<II', recipe_data, header+8*i) for i in range(fields))
recipe_ids = [struct.unpack_from('<H', recipe_data, header+8*fields+i*stride+columns[0x54706054])[0]
              for i in range(rows)]
assert len(set(recipe_ids)) == 924 and max(recipe_ids) < 2048
# RecipeCraftParam distinguishes DIY and Cooking in its category enum. The
# previous fixture incorrectly put all recipes into the DIY-only validity map.
recipe_kinds = {recipe: struct.unpack_from('<I', recipe_data,
                header+8*fields+i*stride+columns[0xffe9b35b])[0]
                for i,recipe in enumerate(recipe_ids)}
DIY_IDS = [recipe for recipe in recipe_ids if recipe_kinds[recipe] == 0xbbdee112]
COOKING_IDS = [recipe for recipe in recipe_ids if recipe_kinds[recipe] == 0x89c6dff6]
assert len(DIY_IDS) == 783 and len(COOKING_IDS) == 141

class Recipes(Stars):
    def __init__(self, base):
        super().__init__(base)
        self.history = self.ram+0x4000
        self.recipe_manager = self.ram+0x10000
        self.write(base+0x4ab46f0, self.ram+0x3060)
        self.write(self.ram+0x3060, self.recipe_manager)
        valid = bytearray(512)
        for recipe in DIY_IDS: valid[recipe//8] |= 1 << (recipe%8)
        for recipe in COOKING_IDS: valid[256+recipe//8] |= 1 << (recipe%8)
        self.valid_bits = self.recipe_manager+0xe98e0
        self.u.mem_write(self.valid_bits, bytes(valid))
        self.write(self.other+0x8690, self.history)
        self.write(self.other+0x8698, 0)
        self.write(self.owner, base+0x4a06e28)
        self.write(self.owner+0x258, self.window)
        self.write(self.frame+8, base+0x30883d4)
        self.u.mem_protect(self.history, 0x1000, UC_PROT_READ)
        self.u.mem_protect(self.valid_bits&~4095, 0x1000, UC_PROT_READ)
        self.native_queries = 0

    def hook(self, u, pc, size, data):
        if pc == self.base+0x24830c0:
            assert self.reg(0) == self.manager and self.reg(2) == 1
            assert bytes(u.mem_read(self.reg(1),1)) == bytes(u.mem_read(self.player,1))
            u.reg_write(UC_ARM64_REG_X0, self.save if self.have_save else 0)
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
        else:
            if pc in (self.base+0x251e658, self.base+0x251288c): self.native_queries += 1
            super().hook(u,pc,size,data)

    def check(self, recipe, learned=False, expected='★ Wooden chair', player=0,
              item_id=5794, flags=0, text='wooden chair', capacity=128, kind=0,
              locked=False, adjacent=False):
        item = struct.pack('<HBBI',item_id,0,flags,recipe)
        bits = bytearray(512)
        if learned: bits[(recipe&0xffff)//8] |= 1 << (recipe%8)
        if adjacent: bits[256+(recipe&0xffff)//8] |= 1 << (recipe%8)
        self.u.mem_write(self.history, bytes(bits))
        self.u.mem_write(self.item,item)
        self.write(self.owner+0x1f8,player,1); self.write(self.player,player,1)
        self.u.mem_write(self.text,b'\0'*256)
        self.u.mem_write(self.text,text.encode('utf-16-le')+b'\0\0')
        self.write(self.name+8,kind,4);self.write(self.name+0x18,self.text)
        self.write(self.name+0x20,capacity,4)
        self.write(self.other+0x86b0,self.lock if locked else 0)
        self.lock_calls.clear()
        saved = bytes(self.u.mem_read(self.ctx,31*8))
        self.u.reg_write(UC_ARM64_REG_X0,self.ctx)
        self.call(self.mod+symbol('CabinetCatalogStar','Callback'))
        rendered = bytes(self.u.mem_read(self.text,256)).decode('utf-16-le').split('\0',1)[0]
        assert rendered == expected,(recipe,learned,rendered,expected)
        assert bytes(self.u.mem_read(self.ctx,31*8)) == saved
        assert bytes(self.u.mem_read(self.item,8)) == item
        assert bytes(self.u.mem_read(self.history,512)) == bytes(bits)


def main():
    count=0
    for base in (0x7100000000,0x7200000000):
        q=Recipes(base)
        q.u.reg_write(UC_ARM64_REG_X0,0)
        q.call(q.mod+symbol('ValidateRecipeBindings'))
        assert q.reg(0)==1
        for recipe in recipe_ids:
            q.check(recipe);q.check(recipe,True,expected='wooden chair');count+=2
        recipe=recipe_ids[0]
        for player in range(8):
            q.mapped_player=7-player
            q.check(recipe,player=player,locked=True)
            assert q.lock_calls==[0x200,0x204]
            q.check(recipe,True,expected='wooden chair',player=player,locked=True)
            count+=2
        q.mapped_player=0
        for cooking in (COOKING_IDS[0],COOKING_IDS[-1]):
            for player in range(8):
                q.check(cooking,player=player,locked=True,adjacent=True)
                assert q.lock_calls==[0x200,0x204]
                q.check(cooking,True,player=player,locked=True,expected='wooden chair')
                count+=2
        # A corrupt category overlap must not be mistaken for a missing recipe.
        cooking_byte=q.valid_bits+256+recipe//8
        old=bytes(q.u.mem_read(cooking_byte,1))
        q.write(cooking_byte,old[0] | (1 << (recipe%8)),1)
        q.check(recipe,expected='wooden chair');count+=1
        q.u.mem_write(cooking_byte,old)
        q.check(recipe,adjacent=True);count+=1 # adjacent state is not learned
        q.check(recipe | 0xabcd0000);count+=1 # only low 16 payload bits identify recipe
        q.check(recipe,locked=True);count+=1
        q.lock_result=0;q.check(recipe,locked=True);assert q.lock_calls==[0x200];count+=1
        for item in (1,1286,5697,0xffff):
            q.check(recipe,item_id=item,expected='wooden chair');count+=1
        for flags in (1,2,3):q.check(recipe,flags=flags,expected='wooden chair');count+=1
        for player in (8,255):q.check(recipe,player=player,expected='wooden chair');count+=1
        for invalid in (2048,65535):q.check(invalid,expected='wooden chair');count+=1
        absent=next(i for i in range(2048) if i not in recipe_ids)
        q.check(absent,expected='wooden chair');count+=1
        for capacity,kind in ((127,0),(128,3)):
            q.check(recipe,capacity=capacity,kind=kind,expected='wooden chair');count+=1
        for text in ('','★ Wooden chair','x'*126,'x\ny','\x0e\x31bad'):
            q.check(recipe,text=text,expected=text);count+=1
        q.check(recipe,text='étoile',expected='★ Étoile');count+=1
        for addr in (q.owner,q.owner+0x258,q.frame+8,q.frame+0x58,q.ctx+22*8,
                     base+0x4ab46f0,q.ram+0x3060,base+0x54021a0,q.other+0x8690):
            saved=bytes(q.u.mem_read(addr,8));q.write(addr,0)
            before=q.native_queries;q.check(recipe,expected='wooden chair')
            assert q.native_queries==before
            q.u.mem_write(addr,saved);count+=1
        for addr in (q.status+0x13d4,q.status+0x13d6):
            q.write(addr,0,1);q.check(recipe,expected='wooden chair');q.write(addr,1,1);count+=1
        q.have_save=False;q.check(recipe,expected='wooden chair');q.have_save=True;count+=1
        q.mapped_player=8;q.check(recipe,expected='wooden chair');q.mapped_player=0;count+=1
        q.write(q.other+8,0);q.write(q.ram+0x3020+0x20,q.other)
        q.check(recipe);count+=1
        q.write(base+0x5474040,0);q.check(recipe,expected='wooden chair');count+=1
        q.write(q.other+8,1)
        # A duplicate card is rechecked on the next focus update after learning.
        q.check(recipe);q.check(recipe,True,expected='wooden chair');q.check(recipe);count+=3
    # Execute the real pockets item-copy call and native name-buffer construction.
    # Only localized resource lookup and memset are substituted in this path.
    class NativePockets(Recipes):
        def hook(self,u,pc,size,data):
            if pc == 0x3f04450:
                u.mem_write(self.reg(0),bytes([self.reg(1)&255])*self.reg(2))
            elif pc == 0x1b0bafc:
                assert self.reg(0) == self.name
                assert bytes(u.mem_read(self.reg(1),8)) == struct.pack('<HBBI',5794,0,0,self.selected_recipe)
                u.mem_write(self.text,self.localized_name.encode('utf-16-le')+b'\0\0')
            else:
                return super().hook(u,pc,size,data)
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR))

    q=NativePockets(0)
    for recipe, learned in ((DIY_IDS[0],False),(DIY_IDS[0],True),
                            (COOKING_IDS[0],False),(COOKING_IDS[0],True)):
        q.selected_recipe=recipe
        q.localized_name='\x0e\x32\0\x04\0\0wooden chair'
        bits=bytearray(512)
        if learned: bits[recipe//8] |= 1 << (recipe%8)
        q.u.mem_write(q.history,bytes(bits))
        controls=q.ram+0x17b000;label=controls+0x1000;renderer=label+0x100
        q.write(q.window+0x210,controls);q.write(controls+0x270,label);q.write(label+0x48,renderer)
        caller_sp=q.frame+0x60
        q.u.mem_write(caller_sp,b'\0'*0x100)
        for reg,value in ((UC_ARM64_REG_SP,caller_sp),(UC_ARM64_REG_X19,q.owner),
                          (UC_ARM64_REG_X23,5794),(UC_ARM64_REG_X24,0),
                          (UC_ARM64_REG_X21,0),(UC_ARM64_REG_X25,recipe)):
            q.u.reg_write(reg,value)
        q.u.emu_start(0x30883b8,0x30117c8,count=50000)
        assert q.u.reg_read(UC_ARM64_REG_PC)==0x30117c8
        assert q.u.reg_read(UC_ARM64_REG_X29)==q.frame
        assert int.from_bytes(q.u.mem_read(q.frame+8,8),'little')==0x30883d4
        assert int.from_bytes(q.u.mem_read(q.frame+0x58,8),'little')==q.owner
        assert q.u.reg_read(UC_ARM64_REG_X19)==q.window
        assert q.u.reg_read(UC_ARM64_REG_X22)==caller_sp+0x20
        for i in range(31):
            reg=UC_ARM64_REG_X0+i if i<29 else (UC_ARM64_REG_X29 if i==29 else UC_ARM64_REG_LR)
            q.write(q.ctx+i*8,q.u.reg_read(reg))
        q.u.reg_write(UC_ARM64_REG_X0,q.ctx)
        q.call(q.mod+symbol('CabinetCatalogStar','Callback'))
        expected=q.localized_name if learned else q.localized_name[:6]+'★ Wooden chair'
        encoded=expected.encode('utf-16-le')+b'\0\0'
        assert bytes(q.u.mem_read(q.text,len(encoded)))==encoded
        assert bytes(q.u.mem_read(q.history,512))==bytes(bits)
        count+=1
    print(f'PASS: {count} pockets recipe cases; 783 DIY + 141 cooking IDs, two bases, native learned-bit queries, '
          'player remapping, read locks, duplicate refresh, scope/text guards and read-only history')


if __name__ == "__main__": main()
