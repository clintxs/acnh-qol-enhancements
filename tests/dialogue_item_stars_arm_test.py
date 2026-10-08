"""Five event item setters with native argument setup and ownership queries.

Source/name addresses come from executing original constructor argument setup.
Resource lookup and UI rendering are excluded; no live save is used.
"""
import struct
from pocket_recipe_arm_test import Recipes, DIY_IDS, COOKING_IDS
from catalog_variants_arm_test import Variants, furniture
from catalog_stars_arm_test import symbol, nonwall, bcsv, field_offsets
from storage_ground_arm_test import GroundFurniture
from unicorn.arm64_const import *

SITES = (
    ('DialogueValueItemStar', 0xc592b4, 0xc592c4, 0x1b50),
    ('DialogueStoredItemStar', 0xc59ba0, 0xc59bb0, 0x1b50),
    ('DialogueInventoryItemStar', 0xc59bc8, 0xc59bd8, 0x1b50),
    ('DialogueUiResultItemStar', 0xc5e124, 0xc5e134, 0x1860),
    ('DialogueScriptItemStar', 0xa46580, 0xa46598, 0x1bd0),
)
MONEY = [i for i,r in nonwall if struct.unpack_from('<I',bcsv,r+field_offsets[0x5270eb75])[0]==0x761e3ce0]
assert len(MONEY)==108
NOUN = '\x0e\x32\0\x04\0\0'

class Dialogue(Recipes, Variants):
    def select(self, site):
        self.site = site
        hook, setup, end, frame_bytes = site
        self.u.reg_write(UC_ARM64_REG_SP, self.frame-frame_bytes)
        self.u.reg_write(UC_ARM64_REG_X29, self.frame)
        self.u.emu_start(self.base+setup, self.base+end)
        self.name, self.source_item = self.reg(0), self.reg(1)
        if hook == "DialogueUiResultItemStar":
            self.write(self.frame-4,24,4) # cBridgeSlopeKitName uses the SP+8 source.
        self.write(self.ctx+20*8, self.action)
        self.write(self.ctx+29*8, self.frame)
        self.write(self.ctx+1*8, self.name)

    def check_name(self, item, marker='', owned=False, extra=0, player=0,
                   learned=False, kind=0, capacity=128, text=NOUN+'blue chair', repeat=False):
        data=struct.pack('<HBBI',item,0,0,extra)
        self.u.mem_write(self.source_item,data)
        self.write(self.event+0x2a8,player,1); self.write(self.player,player,1)
        bits=bytearray(0x754)
        if owned: bits[item//8]|=1<<(item%8)
        self.u.mem_write(self.bits,bytes(bits))
        history=bytearray(512)
        if learned: history[(extra&0xffff)//8]|=1<<(extra%8)
        self.u.mem_write(self.history,bytes(history))
        self.u.mem_write(self.name,bytes(0x3d0))
        self.write(self.name+8,kind,4)
        for offset in (0,0x1e0):
            self.write(self.name+offset+0x18,self.name+offset+0xf0)
            self.write(self.name+offset+0x20,capacity,4)
            self.write(self.name+offset+0x28,0x12345678,4)
            self.u.mem_write(self.name+offset+0xf0,(text+'\0').encode('utf-16-le'))
        context=bytes(self.u.mem_read(self.ctx,31*8))
        for _ in range(2 if repeat else 1):
            self.u.reg_write(UC_ARM64_REG_X0,self.ctx)
            self.call(self.mod+symbol(self.site[0],'Callback'))
        for offset in (0,0x1e0):
            visible=marker if offset==0 or kind in (1,2) else ''
            prefix=NOUN if text.startswith(NOUN) else ''
            expected=prefix+(visible+' ' if visible else '')+text[len(prefix):]+'\0'
            assert bytes(self.u.mem_read(self.name+offset+0xf0,len(expected)*2))==expected.encode('utf-16-le'),(self.site[0],item,self.logs[-1:])
            assert bytes(self.u.mem_read(self.name+offset+0x28,4))==struct.pack('<I',0x12345678)
        assert bytes(self.u.mem_read(self.ctx,31*8))==context
        assert bytes(self.u.mem_read(self.source_item,8))==data
        assert bytes(self.u.mem_read(self.bits,len(bits)))==bytes(bits)
        assert bytes(self.u.mem_read(self.history,len(history)))==bytes(history)

count=0
for base in (0x7100000000,0x7200000000):
    q=Dialogue(base)
    for site in SITES:
        q.select(site)
        q.write(q.ctx+2*8,3);q.write(q.ctx+27*8,3);q.write(q.ctx+19*8,3)
        for player in range(8):
            q.mapped_player=7-player
            q.check_name(117,'★',player=player,repeat=True);count+=1
            q.check_name(117,owned=True,player=player);count+=1
        q.mapped_player=0
        item,remake,_=furniture[0]
        for owned,body,marker in ((False,None,'★'),(True,1,'☆'),(True,0,'')):
            Variants.history(q,remake,body)
            q.check_name(item,marker,owned=owned);count+=1
        for recipe in (DIY_IDS[0],COOKING_IDS[0],737):
            for kind in (0,1,2):
                q.check_name(5794,'★',extra=recipe,kind=kind);count+=1
                q.check_name(5794,extra=recipe,learned=True,kind=kind);count+=1
        for item in MONEY+[5851,9964,2543,3075,3247,*range(8773,8782),9771,0xfffe]:
            q.check_name(item);count+=1
        for kind,capacity in ((3,128),(0,127)):
            q.check_name(117,kind=kind,capacity=capacity);count+=1
        for player in (8,255):q.check_name(117,player=player);count+=1
        for text in ('','★ blue chair','☆ blue chair','\x0e\x01bad','x'*126):
            q.check_name(117,text=text);count+=1
        q.write(q.action+8,0);q.check_name(117);count+=1
        q.write(q.action+8,q.event)
    ground=GroundFurniture(base)
    for item in MONEY:
        ground.display(item,text='Bells',expected='Bells');count+=1
print(f'PASS: {count} dialogue/ground cases: five native source layouts, stars, variants, '
      'DIY/cooking/counted names, all Bells IDs, guards, slots and unchanged history')
