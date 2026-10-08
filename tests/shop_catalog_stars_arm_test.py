"""New display routes and recipe exclusions against compiled ARM + native history.

Uses local ItemParam/ItemRemake fixtures; no game assets are distributed. Dialog
resource lookup is substituted as in the existing Nook tests. Live message
selection is established separately from the shipped EventFlow data.
"""
import struct
from catalog_variants_arm_test import Variants, furniture
from nook_catalog_stars_arm_test import Nook
from catalog_stars_arm_test import Stars, ids, nonwall, bcsv, field_offsets

ROUTES=(('SYS_Wherearen_OfficeShop','001','hhp'),('SP_kpg','151','hotel'),('SP_hgh','051','able'),('SP_hgh','052','able'))
RECIPE_KINDS=(0xeea2c69e,0x62291ce3,0x6693e43a,0x5280a2c3,0x890e26a5,0x698b48d4)
RECIPES=[i for i,r in ids+nonwall if struct.unpack_from('<I',bcsv,r+field_offsets[0x5270eb75])[0] in RECIPE_KINDS]

def main():
    count=0
    for base in (0x7100000000,0x7200000000):
        q=Variants(base)
        item,remake,colors=furniture[0]
        for bank,label,shop in ROUTES:
            q.string(q.bank,bank);q.string(q.label,label)
            # Each routing path must preserve both furniture marker states and
            # remove the star after the same color enters native history.
            for known,body,marker in ((False,None,'★'),(True,1,'☆'),(True,0,'')):
                q.check_variant(item,remake,colors,0,known,body,marker);count+=1
            assert any('shop='+shop in s for s in q.logs)
            for clothing in (117,118):
                for owned in (False,True):
                    q.check(clothing,owned=owned,marked=not owned);count+=1
            for recipe in RECIPES:
                for owned in (False,True):
                    q.check(recipe,owned=owned,marked=False);count+=1
            # Native direct context, used instead of the reference context.
            q.write(q.holder+0x10,q.holder+0x200);q.write(q.holder+0x20,1)
            q.check(117);count+=1
            q.write(q.holder+0x10,0);q.write(q.holder+0x20,q.holder+0x100)
            for wrong_bank,wrong_label in ((bank,label+'0'),(bank,'0'+label),(bank+'_extra',label),
                                           ('TalkSys/'+bank,label),(bank,'999')):
                q.string(q.bank,wrong_bank);q.string(q.label,wrong_label)
                q.check(117,marked=False);count+=1
            q.string(q.bank,bank);q.string(q.label,label)
            q.have_save=False;q.check(117,marked=False);q.have_save=True;count+=1
            q.write(q.action,0);q.check(117,marked=False);q.write(q.action,base+0x4833a58);count+=1
        # Shared shop balloon: all actual recipe records, both ownership states.
        balloon=Stars(base)
        for recipe in RECIPES:
            for owned in (False,True):
                balloon.display(recipe,owned=owned,expected='Sample wall');count+=1
        # Test both language encodings, independently of this asset's encoding.
        row=dict(nonwall)[RECIPES[0]]
        address=balloon.data+row+field_offsets[0x5270eb75]
        original=bytes(balloon.u.mem_read(address,4))
        for kind in RECIPE_KINDS:
            balloon.write(address,kind,4)
            balloon.display(RECIPES[0],expected='Sample wall');count+=1
        balloon.u.mem_write(address,original)
        balloon.display(4969,expected='★ Sample wall');count+=1 # ordinary item remains eligible
    # Real native constructor + owned text copy for each supported new route. The buffer
    # must survive destruction of the temporary hook stack.
    q=Nook(0,native=True)
    for bank,label,_ in ROUTES:
        q.string(q.bank,bank);q.string(q.label,label)
        q.heap=q.ram+0x1b0000;q.copy_objects.clear()
        q.write(q.formatter+0x18,q.formatter+0x18);q.write(q.formatter+0x28,0,4)
        q.check(117);count+=1
        assert len(q.copy_objects)==2
        buf,size=q.copy_objects[1]
        expected=q.registered[-1]
        assert bytes(q.u.mem_read(buf,size))==expected
        q.u.mem_write(q.sp-0x4000,b'\xcc'*0x4000)
        assert bytes(q.u.mem_read(buf,size))==expected
    print(f'PASS: {count} shop cases; four exact routes, filled/hollow/none, clothing colors, {len(RECIPES)} recipe exclusions, context guards and native owned text copy')

if __name__=='__main__':main()
