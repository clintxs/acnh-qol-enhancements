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
assert tree['P_CardIcon_00'] == 'N_CardIcon_00'
number = new_panes[card.NUMBER]
assert struct.unpack_from('<H', number, 84)[0] == 24 # 11 characters plus terminator
assert struct.unpack_from('<I', number, 128)[0] == 0 # no MSBT lookup
assert struct.unpack_from('<2f', number, 76)[0] == 260
old_materials = material_records(next(s for s in old if s[:4] == b'mat1'))
new_materials = material_records(next(s for s in new if s[:4] == b'mat1'))
assert new_materials[:len(old_materials)] == old_materials
original_sections = PANE_TYPES | {b'pas1', b'pae1', b'mat1', b'txl1'}
assert [s for s in old if s[:4] not in original_sections] == [s for s in new if s[:4] not in original_sections]
for key in original:
    if not key.endswith(('.bflyt', '.bntx')): assert modified[key] == original[key]

def texture_names(sections):
    table = next(s for s in sections if s[:4] == b'txl1')
    count = struct.unpack_from('<H', table, 8)[0]
    return [table[12+offset:table.index(0,12+offset)].decode('ascii')
            for offset in struct.unpack_from(f'<{count}I',table,12)]
assert texture_names(new) == texture_names(old) + list(card.TEXTURES)
print('PASS: hidden savings subtree; original panes/materials/fonts/animations intact, nine-digit amount, no MSBT lookup')

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
for index, (name, parent) in enumerate((('P_CardRim_00','N_CardBacking_00'),
                                        ('P_CardIcon_00','N_CardIcon_00'))):
    pane = new_panes[name]
    assert pane[84:100] == bytes([255]*16) and tree[name] == parent
    material = new_materials[struct.unpack_from('<H',pane,100)[0]]
    native_material = old_materials[struct.unpack_from('<H',old_panes['P_Icon_00'],100)[0]]
    assert material[28:44] == native_material[28:44]
    assert material[46:] == native_material[46:] # Native sampler/filter/shader settings.
    assert struct.unpack_from('<H',material,44)[0] == len(texture_names(old)) + index
    assert len(pane) == 136 and pane[102:104] == bytes((1,0)) # Ordinary textured PIC.
    assert pane[104:] == old_panes['P_Icon_00'][104:]
    assert struct.unpack_from('<2f',pane,76) == card.CARD_CANVAS
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

# A single foreground image avoids independently transformed card fragments.
assert len(new_panes) == 24 and len(new) == 63
assert len(modified[next(k for k in modified if k.endswith('.bflyt'))]) == 7100
assert not any(s[:4] == b'spi1' for s in new)
assert not any(n.startswith(('P_CardShape','P_CardEdge','P_CardFace')) for n in new_panes)
canvas = next(s for s in new if s[:4] == b'lyt1')
assert struct.unpack_from('<2f', canvas, 12) == (1920,1080)
assert struct.unpack_from('<3f', new_panes['RootPane'],44) == (0,0,0)
print('PASS: two ordinary textured pictures, 24 panes/63 sections, native 16:9 canvas/root, original cash resources and controls preserved')
