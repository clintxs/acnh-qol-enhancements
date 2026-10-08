"""Miles icon badges on the actual patched row hierarchy, with native queries.

The native row renderer is stubbed; recursive pane lookup and ownership reads
execute original ARM. Fixtures protect history and verify adjacent rows.
"""
from catalog_badges_arm_test import Badges
from pocket_recipe_arm_test import Recipes, recipe_ids
from catalog_variants_arm_test import furniture, Variants
from catalog_stars_arm_test import nonwall, bcsv, field_offsets
import struct

EXCLUDED=(5851,9964,2543,3075,3247,*range(8773,8782),9771)
class MilesBadges(Recipes,Badges):
    pass

count=0
for base in (0x7100000000,0x7200000000):
    q=MilesBadges(base);q.load_layout('Miles')
    for grouped in (False,True):
        q.use_group(grouped)
        for recipe in recipe_ids:
            for learned in (False,True):
                bits=bytearray(512)
                if learned:bits[recipe//8]|=1<<(recipe%8)
                q.u.mem_write(q.history,bytes(bits))
                Badges.check(q,'Miles',5794,False,not learned,payload=recipe)
                assert bytes(q.u.mem_read(q.history,512))==bytes(bits)
                count+=1
    for item,remake,_ in furniture:
        for owned,body in ((False,None),(True,0),(True,1)):
            Variants.history(q,remake,body)
            Badges.check(q,'Miles',item,owned,not owned);count+=1
    for item,row in nonwall:
        kind=struct.unpack_from('<I',bcsv,row+field_offsets[0x5270eb75])[0]
        if item in EXCLUDED or kind in (0x8d9857c4,0xeea2c69e,0x6693e43a):
            # A reused row must clear a previous star for excluded rewards.
            Badges.check(q,'Miles',4969,False,True)
            Badges.check(q,'Miles',item,False,False);count+=2
    for player in range(8):
        q.write(q.player,player,1);q.mapped_player=7-player
        Badges.check(q,'Miles',4969,False,True,player=player);count+=1
    q.write(q.player,0,1);q.mapped_player=0
    Badges.check(q,'Miles',4969,False,False,player=8);count+=1
    Badges.check(q,'Miles',0xfffe,False,False);count+=1
    q.u.mem_write(q.badges[0]+0xb0,b'X')
    Badges.check(q,'Miles',4969,False,False);count+=1
    q.u.mem_write(q.badges[0]+0xb0,b'T')
    # Fail locally when the icon parent is absent; never touch the neighbor.
    icon=int.from_bytes(q.u.mem_read(q.badges[0]+0x18,8),'little')
    q.u.mem_write(icon+0xb0,b'X')
    Badges.check(q,'Miles',4969,False,False);count+=1
    # Catalog exclusions also apply consistently in the other icon catalogs.
    for shop in ('Nook','Hhp','Hotel'):
        q.load_layout(shop)
        for item in EXCLUDED:
            Badges.check(q,shop,item,False,False);count+=1
print(f'PASS: {count} Miles badge cases: recipes, family status, exclusions, recycling, '
      'two bases, both name adapters and neighboring row isolation')
