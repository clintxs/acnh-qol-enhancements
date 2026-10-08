"""Structural checks for the independent savings currency presentation."""
from pathlib import Path
from compression import zstd
import sys
import struct
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import build_shopping_card_layout as card
from layout_io import resources, layout_sections, pane_name
from build_catalog_badge_layout import material_records
from build_layouts import PANE_TYPES

packed = (card.ROOT.parent / f'vanilla-3.0.3/Layout/{card.ASSET}.Nin_NX_NVN.zs').read_bytes()
original = resources(zstd.decompress(packed))
modified = resources(zstd.decompress(next(iter(card.build().values()))))
key = next(k for k in original if k.endswith('.bflyt'))
old, new = layout_sections(original[key]), layout_sections(modified[key])
old_panes = {pane_name(s): s for s in old if s[:4] in PANE_TYPES}
new_panes = {pane_name(s): s for s in new if s[:4] in PANE_TYPES}
assert all(new_panes[name] == pane for name, pane in old_panes.items())
assert not new_panes[card.CARD][8] & 1
parents = []; last = None; tree = {}
for section in new:
    if section[:4] in PANE_TYPES:
        last = pane_name(section)
        tree[last] = parents[-1] if parents else None
    elif section[:4] == b'pas1': parents.append(last)
    elif section[:4] == b'pae1': parents.pop()
assert not parents and tree[card.CARD] == 'N_Pos_01'
assert tree[card.NUMBER] == tree['N_CardBells_00'] == card.CARD
assert tree['T_SavingsLabel_00'] == tree['W_CardBellBase_00'] == 'N_CardBells_00'
assert all(tree[f'P_CardShape_{i:02}'] == 'N_CardIcon_00' for i in range(10))
number = new_panes[card.NUMBER]
assert struct.unpack_from('<H', number, 84)[0] == 24 # 11 characters plus terminator
assert struct.unpack_from('<I', number, 128)[0] == 0 # no MSBT lookup
assert struct.unpack_from('<2f', number, 76)[0] == 260
old_materials = material_records(next(s for s in old if s[:4] == b'mat1'))
new_materials = material_records(next(s for s in new if s[:4] == b'mat1'))
assert new_materials[:len(old_materials)] == old_materials
assert all(a == b for a,b in zip([s for s in old if s[:4] not in PANE_TYPES | {b'pas1', b'pae1', b'mat1'}],
                               [s for s in new if s[:4] not in PANE_TYPES | {b'pas1', b'pae1', b'mat1'}]))
for key in original:
    if not key.endswith('.bflyt'): assert modified[key] == original[key]
print('PASS: savings subtree hidden by default; original panes/materials/animations/textures intact, vector card and full nine-digit text capacity, no MSBT lookup')

label = new_panes['T_SavingsLabel_00']
start = struct.unpack_from('<I', label, 100)[0]
assert label[start:start + 12].decode('utf-16-le') == 'Bells\0'
assert label[92:96] == old_panes['T_Bell_00'][92:96]
assert new_materials[struct.unpack_from('<H', label, 88)[0]][40:44] == card.GREEN
for name, color in (('W_SavingsRim_00', bytes([255]*4)),
                    ('W_SavingsBase_00', card.GREEN), ('W_CardBellBase_00', bytes([255]*4))):
    for offset in (128, 168):
        index = struct.unpack_from('<H', new_panes[name], offset)[0]
        assert new_materials[index][40:44] == color
for i in range(3): assert new_panes[f'P_CardShape_{i:02}'][84:100] == bytes([255]*16)
print('PASS: white oval/card rim, white native Bells badge with matching green caption, original badge alignment')

fill = new_panes['W_SavingsBase_00']
width, height = struct.unpack_from('<2f', fill, 76)
sx, sy = struct.unpack_from('<2f', fill, 68)
assert abs(width*sx - 298) < .001 and height*sy == 120
