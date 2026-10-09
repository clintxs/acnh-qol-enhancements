"""Add a separate, hidden savings display to the native top-right currency part.

Uses native rounded windows and ordinary textured picture panes for the card.
Original panes, animation and fonts remain intact; native texture pixels/settings
are preserved while two card pictures are appended to the embedded BNTX.
No item or MSBT resources are added. Requires the local hash-checked 3.0.3 dump.
"""
from pathlib import Path
from compression import zstd
import hashlib
import re
import struct
from layout_io import archive_entries, layout_sections, pane_name, resources
from build_layouts import text_pane, PANE_TYPES
from build_catalog_badge_layout import material_records
from shopping_bntx import append_card, textures
from shopping_card_texture import NAMES as TEXTURES, CANVAS as CARD_CANVAS, PALETTES

ROOT = Path(__file__).resolve().parents[1]
ASSET = 'LBellCounterRT'
# Hash of the original compressed asset, not the generated replacement.
EXPECTED = 'b3f008cab7d267ecd7631a915e27af1dfdc8f0ca6d2fecc9d97be801604a5a92'
CARD = 'N_ShoppingCard_00'
NUMBER = 'T_SavingsNum_00'
OVAL_CENTER = (-25, -2)
OVAL_SIZE = (306, 128)
RIM = 8
CARD_CENTER = (-25, 52)
BADGE_SHIFT = 24  # Retain the requested ~16 screen-pixel label shift.
CARD_ANGLE = 6  # Slight tilt after the native HUD parent's -3-degree rotation.

def rename(section, name):
    result = bytearray(section)
    result[12:36] = name.encode().ljust(24, b'\0')
    return result


def edit_layout(data, palette_name='green'):
    palette = PALETTES[palette_name]
    counter = bytes(palette.counter)
    sections = layout_sections(data)
    panes = {pane_name(s): s for s in sections if s[:4] in PANE_TYPES}
    assert CARD not in panes
    texture_slot = next(i for i,s in enumerate(sections) if s[:4] == b'txl1')
    old_table = sections[texture_slot]
    texture_count = struct.unpack_from('<H', old_table, 8)[0]
    names = []
    for offset in struct.unpack_from(f'<{texture_count}I', old_table, 12):
        start = 12 + offset
        names.append(old_table[start:old_table.index(0,start)].decode('ascii'))
    assert not set(TEXTURES).intersection(names)
    names.extend(TEXTURES)
    texture_table = bytearray(struct.pack('<4sIHH', b'txl1',0,len(names),0))
    strings = b''.join(name.encode() + b'\0' for name in names)
    position = 4 * len(names)
    for name in names:
        texture_table.extend(struct.pack('<I', position))
        position += len(name) + 1
    texture_table.extend(strings)
    texture_table.extend(bytes(-len(texture_table) % 4))
    struct.pack_into('<I', texture_table, 4, len(texture_table))
    sections[texture_slot] = bytes(texture_table)
    material_slot = next(i for i, s in enumerate(sections) if s[:4] == b'mat1')
    records = material_records(sections[material_slot])
    # Two copies of the native rounded window give the colored fill the same
    # light rim as Bells, without tinting away the rim in a shared material.
    def window(template, name, x, y, width, height, color):
        pane = rename(template, name)
        struct.pack_into('<2f', pane, 44, x, y)
        struct.pack_into('<2f', pane, 76, width, height)
        for offset in (128, 168):
            old_index = struct.unpack_from('<H', pane, offset)[0]
            material = bytearray(records[old_index])
            material[:28] = f'{name}_{offset}'.encode().ljust(28, b'\0')
            material[40:44] = color
            struct.pack_into('<H', pane, offset, len(records))
            records.append(bytes(material))
        return bytes(pane)
    rim = bytes(palette.rim)
    outline = window(panes['W_BaseL_00'], 'W_SavingsRim_00', *OVAL_CENTER, *OVAL_SIZE, rim)
    background = bytearray(window(panes['W_BaseL_00'], 'W_SavingsBase_00', *OVAL_CENTER, *OVAL_SIZE,
                                  bytes(palette.background or palette.counter)))
    # Scale the complete native window so its corner/frame geometry shrinks
    # with the fill, leaving an eight-unit rim on each side.
    struct.pack_into('<2f', background, 68, *((size - 2 * RIM) / size for size in OVAL_SIZE))
    badge = window(panes['W_BellBase_00'], 'W_CardBellBase_00', 0, 0, 96, 40, rim)
    label = bytearray(text_pane(panes['T_Bell_00'], 'T_SavingsLabel_00', 'Bells',
                                -16, 2, 121.5, 31.95, .5, True))
    label[92:96] = panes['T_Bell_00'][92:96]  # Native Bells badge alignment.
    text_material = bytearray(records[struct.unpack_from('<H', label, 88)[0]])
    text_material[:28] = b'CardBellsText'.ljust(28, b'\0')
    text_material[40:44] = counter
    struct.pack_into('<H', label, 88, len(records))
    records.append(bytes(text_material))
    number = bytearray(text_pane(panes['T_Num_00'], NUMBER, '999,999,999', -25, 0, 260, 56, .76, True))
    if palette.balance is not None:
        # Island puts blue digits on white. Clone the number material so cash
        # and every existing palette retain their native cream text exactly.
        balance_material = bytearray(records[struct.unpack_from('<H', number, 88)[0]])
        balance_material[:28] = b'CardBalanceText'.ljust(28, b'\0')
        balance_material[40:44] = bytes(palette.balance)
        struct.pack_into('<H', number, 88, len(records))
        records.append(bytes(balance_material))
    picture_materials = []
    native_icon_material = records[struct.unpack_from('<H', panes['P_Icon_00'],100)[0]]
    for index in range(2):
        picture_material = bytearray(native_icon_material)
        picture_material[:28] = f'CardPicture{index}'.encode().ljust(28, b'\0')
        struct.pack_into('<H', picture_material,44,texture_count + index)
        picture_materials.append(len(records))
        records.append(bytes(picture_material))
    material = bytearray(sections[material_slot][:12])
    struct.pack_into('<H', material, 8, len(records))
    position = 12 + 4 * len(records)
    for record in records:
        material.extend(struct.pack('<I', position))
        position += len(record)
    material.extend(b''.join(records))
    struct.pack_into('<I', material, 4, len(material))
    sections[material_slot] = bytes(material)

    group = rename(panes['N_Pos_01'], CARD)
    group[8] = 2  # Starts hidden even if Core/module is disabled.
    icon = rename(panes['N_Pos_01'], 'N_CardIcon_00')
    struct.pack_into('<3f', icon, 44, *CARD_CENTER, 0)
    struct.pack_into('<f', icon, 64, CARD_ANGLE)
    backing = rename(icon, 'N_CardBacking_00')
    opening, closing = struct.pack('<4sI', b'pas1', 8), struct.pack('<4sI', b'pae1', 8)
    def picture(name, material_index):
        pane = rename(panes['P_Icon_00'], name)
        assert len(pane) == 136 and pane[102:104] == bytes((1,0))
        pane[8:12] = bytes((3,0,255,0))
        struct.pack_into('<3f', pane,44,0,0,0)
        struct.pack_into('<3f', pane,56,0,0,0)
        struct.pack_into('<2f', pane,68,1,1)
        struct.pack_into('<2f', pane,76,*CARD_CANVAS)
        pane[84:100] = bytes((255,255,255,255)) * 4
        struct.pack_into('<H', pane,100,material_index)
        return bytes(pane)
    # One normal picture holds the complete colored card, including its edge
    # and details. Its light backing draws before the oval and stays occluded.
    rim_pieces = [picture('P_CardRim_00',picture_materials[0])]
    pieces = [picture('P_CardIcon_00',picture_materials[1])]
    unit = rename(panes['N_Pos_00'], 'N_CardBells_00')
    # The native badge window uses a right-hand origin: place that origin
    # eight units inside the oval's right edge, then retain the requested shift left.
    struct.pack_into('<f', unit, 44, OVAL_CENTER[0] + OVAL_SIZE[0] / 2 - RIM - BADGE_SHIFT)
    additions = [bytes(group), opening, bytes(backing), opening, *rim_pieces, closing,
                 outline, bytes(background), bytes(icon), opening,
                 *pieces, closing, bytes(number), bytes(unit), opening, badge, bytes(label), closing, closing]
    parents = []
    last = None
    insertion = None
    for i, section in enumerate(sections):
        if section[:4] in PANE_TYPES:
            last = pane_name(section)
        elif section[:4] == b'pas1':
            parents.append(last)
        elif section[:4] == b'pae1':
            if parents.pop() == 'N_Pos_01':
                insertion = i
    assert insertion is not None and not parents
    sections[insertion:insertion] = additions
    # Stay below the pane/section budget of the live-confirmed 0.3.10 recovery.
    # This is a conservative project guard, not a claimed native engine limit.
    assert sum(s[:4] in PANE_TYPES for s in sections) <= 35
    assert len(sections) <= 74
    output = bytearray(data[:20]) + b''.join(sections)
    struct.pack_into('<I', output, 12, len(output))
    struct.pack_into('<H', output, 16, len(sections))
    return bytes(output)


def patch_archive(data, palette_name='green'):
    start, entries = archive_entries(data)
    output = bytearray(data[:start])
    count = 0
    for entry in sorted(entries, key=lambda entry: entry.start):
        content = entry.content
        if content.startswith(b'SARC'):
            content, nested = patch_archive(content, palette_name)
            count += nested
        elif entry.name == 'timg/__Combined.bntx':
            content = append_card(content, palette_name)
        elif entry.name == f'blyt/{ASSET}.bflyt':
            content = edit_layout(content, palette_name)
            count += 1
        alignment = min((start + entry.start) & -(start + entry.start), 4096)
        output.extend(bytes((-len(output)) % alignment))
        offset = len(output) - start
        output.extend(content)
        struct.pack_into('<2I', output, entry.node_offset + 8, offset, len(output) - start)
    struct.pack_into('<I', output, 8, len(output))
    return bytes(output), count


def build(palette_name='green'):
    if palette_name not in PALETTES:
        raise ValueError(f'Unknown Shopping Card palette: {palette_name}')
    packed = (ROOT.parent / f'vanilla-3.0.3/Layout/{ASSET}.Nin_NX_NVN.zs').read_bytes()
    assert hashlib.sha256(packed).hexdigest() == EXPECTED, 'Unsupported original currency layout'
    original = zstd.decompress(packed)
    modified, count = patch_archive(original, palette_name)
    assert count == 1
    header = (ROOT / 'source/program/shopping/resource_budget.hpp').read_text()
    floor = int(re.search(r'CounterResourceMinimumBytes = (\d+)', header)[1])
    # Vanilla's RSTC allowance includes 8 KiB beyond its expanded archive size.
    # Keep at least that overhead; current 1 MiB floor also leaves ample room
    # for added panes/materials. Runtime hook and exact target are ARM-tested.
    assert len(modified) + 8192 <= floor, 'Counter archive exceeds scoped runtime resource budget'
    old, new = resources(original), resources(modified)
    assert old.keys() == new.keys()
    for name in old:
        if name.endswith('/timg/__Combined.bntx'):
            before,after = textures(old[name]),textures(new[name])
            assert len(after) == len(before) + 2
            for a,b in zip(before,after):
                assert a.name == b.name and a.data == b.data
                assert a.info[16:96] == b.info[16:96]  # Formats, dimensions, channels and tiling.
        elif not name.endswith(f'/{ASSET}.bflyt'):
            assert new[name] == old[name], name
    return {f'romfs/Layout/{ASSET}.Nin_NX_NVN.zs': zstd.compress(modified)}
