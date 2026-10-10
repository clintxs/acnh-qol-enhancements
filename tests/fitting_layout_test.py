"""Bound badge drawing cost and preserve native layout/animation resources."""
from pathlib import Path
import struct
import sys
from compression import zstd
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import build_layouts as layout
from bag_texture import NAME, CANVAS
from layout_io import resources, layout_sections, pane_name
from build_catalog_badge_layout import material_records

name = layout.ASSETS[0]
original = resources(zstd.decompress((layout.ROOT.parent / f'vanilla-3.0.3/Layout/{name}.Nin_NX_NVN.zs').read_bytes()))
modified = resources(zstd.decompress(layout.build()[f'romfs/Layout/{name}.Nin_NX_NVN.zs']))
assert original.keys() == modified.keys()
assert {key.rsplit('/',1)[-1] for key in original if original[key] != modified[key]} == {f'{name}.bflyt','__Combined.bntx'}
key = next(key for key in original if key.endswith('.bflyt'))
a,b = (layout_sections(data[key]) for data in (original,modified))
old = {pane_name(s):s for s in a if s[:4] in layout.PANE_TYPES}
new = {pane_name(s):s for s in b if s[:4] in layout.PANE_TYPES}
assert all(new[name] == pane for name,pane in old.items())
assert new.keys() - old.keys() == {'N_Bag_00','P_BagCircle_00','P_BagIcon_00'}
assert not new['N_Bag_00'][8] & 1
assert struct.unpack_from('<2f',new['N_Bag_00'],44) == (-80,60)
assert struct.unpack_from('<2f',new['N_Bag_00'],68) == (1,1)
assert struct.unpack_from('<2f',new['P_BagIcon_00'],76) == (CANVAS,CANVAS)
parents=[];last=None;children={}
for section in b:
    if section[:4] in layout.PANE_TYPES:
        last=pane_name(section)
        if parents: children.setdefault(parents[-1],[]).append(last)
    elif section[:4] == b'pas1': parents.append(last)
    elif section[:4] == b'pae1': parents.pop()
assert children['N_Bag_00'] == ['P_BagCircle_00','P_BagIcon_00']
# Entire badge animation transforms this same two-picture group at any count.
for visible in (1,16,17,32,128):
    assert len(children['N_Bag_00']) * visible == 2 * visible
old_mat,new_mat = (material_records(next(s for s in parts if s[:4] == b'mat1')) for parts in (a,b))
assert new_mat[:-1] == old_mat
picture = new['P_BagIcon_00']
assert struct.unpack_from('<H',picture,100)[0] == len(old_mat)
assert picture[102] == 1 and struct.unpack_from('<8f',picture,104) == (0,0,1,0,0,1,1,1)
table=next(s for s in b if s[:4] == b'txl1')
count=struct.unpack_from('<H',table,8)[0]
assert struct.unpack_from('<H',new_mat[-1],44)[0] == count-1
start=12+struct.unpack_from('<I',table,12+4*(count-1))[0]
assert table[start:table.index(0,start)].decode() == NAME
print('PASS: two pictures per badge (34 for 17 rows versus 1207), unchanged native panes/materials/animations, hidden group and texture binding')
