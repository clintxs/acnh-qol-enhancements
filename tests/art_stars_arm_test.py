"""Real 3.0.3 art rows, compiled shop hook and native museum/catalog lookups."""
import struct
from museum_creature_arm_test import Museum
from catalog_stars_arm_test import nonwall, bcsv, field_offsets, symbol
from unicorn.arm64_const import *

REAL = [i for i,r in nonwall if struct.unpack_from('<I',bcsv,r+field_offsets[0x5270eb75])[0] in (0xd9667615,0x5db00df4)]
FAKE = [i for i,r in nonwall if struct.unpack_from('<I',bcsv,r+field_offsets[0x5270eb75])[0] in (0x96ec4b4f,0x9d3ccf03)]
assert (len(REAL),len(FAKE)) == (43,27)
count=0
for base in (0x7100000000,0x7200000000):
    q=Museum(base)
    def configure(enabled):
        data=f'art_donation_stars = {str(enabled).lower()}\n'.encode()
        ptr=q.m+0x330000;q.u.mem_write(ptr,data)
        q.u.reg_write(UC_ARM64_REG_X0,len(data));q.u.reg_write(UC_ARM64_REG_X1,ptr)
        q.call(q.mod+symbol('catalog_aware_shops','Configure'))
        assert q.reg(0)==0
    for enabled in (True,False):
        configure(enabled)
        for item in REAL+FAKE:
            for owned in (False,True):
                for donor in (255,7):
                    q.prepare(item,donor)
                    bits=bytearray(0x754)
                    if owned:bits[item//8]|=1<<(item%8)
                    q.u.mem_write(q.bits,bytes(bits))
                    marker=('★' if donor==255 else '') if enabled and item in REAL else ('☆' if enabled else '★') if not owned else ''
                    q.run_hook('CabinetCatalogStar', marker+' Sea bass' if marker else 'sea bass')
                    assert bytes(q.u.mem_read(q.bits,len(bits)))==bytes(bits)
                    count+=1
                    if item in (REAL[0], FAKE[0]):
                        source=q.frame-0x1bd0+0x17ec
                        q.u.mem_write(source,bytes(q.u.mem_read(q.item,8)))
                        q.name=q.frame-0x1bd0+0x17f8;q.text=q.name+0xf0
                        q.reset_name()
                        q.run_hook('DialogueScriptItemStar', marker+' sea bass' if marker else 'sea bass')
                        assert bytes(q.u.mem_read(source,8))==bytes(q.u.mem_read(q.item,8))

    configure(True)
    q.getter_ready=False
    q.prepare(REAL[0]);q.run_hook('CabinetCatalogStar','sea bass')
    q.prepare(FAKE[0]);q.u.mem_write(q.bits,bytes(0x754))
    q.run_hook('CabinetCatalogStar','☆ Sea bass')
    q.have_save=False
    q.prepare(FAKE[0]);q.run_hook('CabinetCatalogStar','sea bass')
print(f'PASS: {count} art shop cases; genuine museum state, forged catalog state, setting off and missing-data guards')
