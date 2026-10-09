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
assert all(tree[f'P_CardShape_{i:02}'] == 'N_CardIcon_00' for i in range(4))
number = new_panes[card.NUMBER]
assert struct.unpack_from('<H', number, 84)[0] == 24 # 11 characters plus terminator
assert struct.unpack_from('<I', number, 128)[0] == 0 # no MSBT lookup
assert struct.unpack_from('<2f', number, 76)[0] == 260
old_materials = material_records(next(s for s in old if s[:4] == b'mat1'))
new_materials = material_records(next(s for s in new if s[:4] == b'mat1'))
assert new_materials[:len(old_materials)] == old_materials
assert all(a == b for a,b in zip([s for s in old if s[:4] not in PANE_TYPES | {b'pas1', b'pae1', b'mat1'}],
                               [s for s in new if s[:4] not in PANE_TYPES | {b'pas1', b'pae1', b'mat1'}]))
for name in original:
    if not name.endswith('.bflyt'): assert modified[name] == original[name]
print('PASS: savings subtree hidden by default; original panes/materials/animations/textures intact, alpha-edged vector card and full nine-digit text capacity, no MSBT lookup')

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
# Decode the actual exported primitives: verify a filled card silhouette,
# independent of the generator's declared dimensions or window-frame fields.
# This catches 0.3.8's degenerate window approach instead of just agreeing with
# its frame edits. The native renderer only sees ordinary colored PIC quads.
for prefix, width, height, radius, color in card.LAYERS:
    layer = [pane for name, pane in new_panes.items() if name.startswith(prefix + '_')]
    assert 0 < len(layer) < 150
    assert all(pane[:4] == b'pic1' and len(pane) == 104 for pane in layer)
    assert all(pane[8:12] == bytes((3, 0, 255, 0)) for pane in layer)
    assert all(pane[102:104] == bytes(2) for pane in layer) # untextured
    parent = 'N_CardBacking_00' if prefix == 'P_CardRim' else 'N_CardIcon_00'
    assert all(tree[name] == parent for name in new_panes if name.startswith(prefix + '_'))
    rectangles = [(*struct.unpack_from('<2f', pane, 44),
                   *struct.unpack_from('<2f', pane, 76), pane[84:100]) for pane in layer]
    assert min(y - h/2 for x,y,w,h,c in rectangles) == -height/2 - 1
    assert max(y + h/2 for x,y,w,h,c in rectangles) == height/2 + 1
    assert min(x - w/2 for x,y,w,h,c in rectangles) == -width/2 - .5
    assert max(x + w/2 for x,y,w,h,c in rectangles) == width/2 + .5
    # Every interior sample must be covered; corners should be visibly rounded.
    def coverage(px, py):
        result = 0
        for x,y,w,h,c in rectangles:
            if abs(px-x) > w/2 or abs(py-y) > h/2: continue
            u = (px-x)/w + .5; v = .5-(py-y)/h
            alpha = ((1-u)*(1-v)*c[3] + u*(1-v)*c[7] +
                     (1-u)*v*c[11] + u*v*c[15]) / 255
            result = alpha + result*(1-alpha)
        return result
    assert coverage(0,0) == 1 and coverage(0,height/2-2) == 1
    assert coverage(width/2-1, height/2-1) == 0
    assert .4 < coverage(width/2,0) < .6 # alpha feather at the contour
    for y in range(-int(height/2)+2, int(height/2)-1):
        for x in range(-int(width/2)+2, int(width/2)-1):
            dx=max(abs(x)-(width/2-radius),0)
            dy=max(abs(y)-(height/2-radius),0)
            if dx*dx + dy*dy < (radius-1)**2:
                assert coverage(x,y) > .99, (prefix,x,y)
    assert any(0 in c[3::4] and 255 in c[3::4] for x,y,w,h,c in rectangles)
assert not any(name.startswith('W_Card') and 'BellBase' not in name for name in new_panes)
print('PASS: full card silhouettes, round corner cutouts, feathered contours and filled interiors using ordinary PIC quads; no card window stretching')

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
print('PASS: thicker eight-unit oval rim, centered card, backing occluded by oval, Bells badge shifted 24 layout units left')

# Main package keeps the native 16:9 canvas and all original transforms.
canvas = next(section for section in new if section[:4] == b'lyt1')
assert struct.unpack_from('<2f', canvas, 12) == (1920,1080)
assert struct.unpack_from('<3f', new_panes['RootPane'],44) == (0,0,0)
print('PASS: native 1920x1080 canvas, root and existing anchor transforms preserved; no 16:10 placement adjustment in the default package')
