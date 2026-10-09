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
for i in range(3):
    assert new_panes[f'P_CardShape_{i:02}'][84:100] == card.INK * 4
    assert new_panes[f'P_CardRim_{i:02}'][84:100] == bytes([255]*16)
    assert tree[f'P_CardRim_{i:02}'] == 'N_CardBacking_00'
print('PASS: dark card edge inside white backing; white native Bells badge with matching green caption')

fill = new_panes['W_SavingsBase_00']
width, height = struct.unpack_from('<2f', fill, 76)
sx, sy = struct.unpack_from('<2f', fill, 68)
assert abs(width*sx - 290) < .001 and height*sy == 112
order = list(new_panes)
assert (order.index('N_CardBacking_00') < order.index('W_SavingsRim_00') <
        order.index('W_SavingsBase_00') < order.index('N_CardIcon_00'))
icon, backing = new_panes['N_CardIcon_00'], new_panes['N_CardBacking_00']
assert icon[44:76] == backing[44:76]
assert struct.unpack_from('<f', icon, 44)[0] == struct.unpack_from('<f', fill, 44)[0]
assert struct.unpack_from('<f', new_panes['N_CardBells_00'], 44)[0] == 96
print('PASS: thicker eight-unit oval rim, centered card, backing occluded by oval, Bells badge retains the requested shift left')

# Regression contract: the recovery BFLYT is byte-identical to the previously
# displayed 0.3.7 layout after normalizing the sole intentional badge shift.
# Compare exported bytes to a recorded hash; a geometry-only preview cannot
# establish native layout loading/rendering (the 0.3.9 regression).
import hashlib
baseline = bytearray(modified[next(k for k in modified if k.endswith('.bflyt'))])
position = 20
while position < len(baseline):
    size = struct.unpack_from('<I', baseline, position + 4)[0]
    if baseline[position:position+4] == b'pan1' and pane_name(baseline[position:position+size]) == 'N_CardBells_00':
        struct.pack_into('<f', baseline, position + 44, 120)
    position += size
assert hashlib.sha256(baseline).hexdigest() == '6a7cf50ae9bb8cc32f1188d5933d5ed5679e7211f20b80e6d01cbc33b55c4eaf'
assert len(new_panes) == 35 and len(new) == 74
assert len(baseline) == 7996
canvas = next(s for s in new if s[:4] == b'lyt1')
assert struct.unpack_from('<2f', canvas, 12) == (1920,1080)
assert struct.unpack_from('<3f', new_panes['RootPane'],44) == (0,0,0)
print('PASS: recovery matches displayed 0.3.7 layout bytes except badge X; 35 panes/74 sections, native 16:9 canvas/root, original cash resources and controls preserved')
