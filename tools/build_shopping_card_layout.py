"""Add a separate, hidden savings display to the native top-right currency part.

Uses native rounded-window textures and untextured vector panes for the green
card. Original panes, animation, fonts and textures remain byte-for-byte intact.
No item or MSBT resources are added. Requires the local hash-checked 3.0.3 dump.
"""
from pathlib import Path
from compression import zstd
import hashlib
import struct
from layout_io import archive_entries, layout_sections, pane_name, resources
from build_layouts import text_pane, PANE_TYPES
from build_catalog_badge_layout import material_records

ROOT = Path(__file__).resolve().parents[1]
ASSET = 'LBellCounterRT'
# Hash of the original compressed asset, not the generated replacement.
EXPECTED = 'b3f008cab7d267ecd7631a915e27af1dfdc8f0ca6d2fecc9d97be801604a5a92'
CARD = 'N_ShoppingCard_00'
NUMBER = 'T_SavingsNum_00'
GREEN = bytes((40, 116, 82, 255))
CREAM = bytes((255, 241, 195, 255))
INK = bytes((53, 57, 48, 255))
OVAL_CENTER = (-25, -2)
OVAL_SIZE = (306, 128)
RIM = 8
CARD_CENTER = (-25, 52)
BADGE_SHIFT = 24  # Retain the requested ~16 screen-pixel label shift.
CARD_ANGLE = 6  # Slight tilt after the native HUD parent's -3-degree rotation.
# nn::ui2d::RoundRectShape resource: type, reserved, radius, corner subdivisions.
# Verified against 3.0.3's section reader (0x268550), shape resolver (0x2649c0),
# Picture resource constructor (0x282d30), and RoundRect constructor (0x28db20).
CARD_RADII = (15, 9, 6)
CORNER_DIVISIONS = 16


def shape_section():
    records = [struct.pack('<HHII', 1, 0, radius, CORNER_DIVISIONS) for radius in CARD_RADII]
    offsets = [12 + 4 * len(records) + 12 * i for i in range(len(records))]
    body = struct.pack('<HH', len(records), 0) + struct.pack('<3I', *offsets) + b''.join(records)
    return struct.pack('<4sI', b'spi1', 8 + len(body)) + body


def rename(section, name):
    result = bytearray(section)
    result[12:36] = name.encode().ljust(24, b'\0')
    return result


def edit_layout(data):
    sections = layout_sections(data)
    panes = {pane_name(s): s for s in sections if s[:4] in PANE_TYPES}
    assert CARD not in panes
    material_slot = next(i for i, s in enumerate(sections) if s[:4] == b'mat1')
    records = material_records(sections[material_slot])
    # Two copies of the native rounded window give the green fill the same
    # white rim as Bells, without tinting away the rim in a shared material.
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
    white = bytes((255, 255, 255, 255))
    outline = window(panes['W_BaseL_00'], 'W_SavingsRim_00', *OVAL_CENTER, *OVAL_SIZE, white)
    background = bytearray(window(panes['W_BaseL_00'], 'W_SavingsBase_00', *OVAL_CENTER, *OVAL_SIZE, GREEN))
    # Scale the complete native window so its corner/frame geometry shrinks
    # with the fill, leaving an eight-unit rim on each side.
    struct.pack_into('<2f', background, 68, *((size - 2 * RIM) / size for size in OVAL_SIZE))
    badge = window(panes['W_BellBase_00'], 'W_CardBellBase_00', 0, 0, 96, 40, white)
    label = bytearray(text_pane(panes['T_Bell_00'], 'T_SavingsLabel_00', 'Bells',
                                -16, 2, 121.5, 31.95, .5, True))
    label[92:96] = panes['T_Bell_00'][92:96]  # Native Bells badge alignment.
    text_material = bytearray(records[struct.unpack_from('<H', label, 88)[0]])
    text_material[:28] = b'CardBellsText'.ljust(28, b'\0')
    text_material[40:44] = GREEN
    struct.pack_into('<H', label, 88, len(records))
    records.append(bytes(text_material))
    solid = bytearray(records[0])
    assert len(solid) == 44
    solid[:28] = b'ShoppingCardSolid'.ljust(28, b'\0')
    solid[36:40] = bytes(4)
    solid[40:44] = bytes((255, 255, 255, 255))
    solid_index = len(records)
    records.append(bytes(solid))
    material = bytearray(sections[material_slot][:12])
    struct.pack_into('<H', material, 8, len(records))
    position = 12 + 4 * len(records)
    for record in records:
        material.extend(struct.pack('<I', position))
        position += len(record)
    material.extend(b''.join(records))
    struct.pack_into('<I', material, 4, len(material))
    sections[material_slot] = bytes(material)
    sections.insert(material_slot + 1, shape_section())

    group = rename(panes['N_Pos_01'], CARD)
    group[8] = 2  # Starts hidden even if Core/module is disabled.
    icon = rename(panes['N_Pos_01'], 'N_CardIcon_00')
    struct.pack_into('<3f', icon, 44, *CARD_CENTER, 0)
    struct.pack_into('<f', icon, 64, CARD_ANGLE)
    backing = rename(icon, 'N_CardBacking_00')
    opening, closing = struct.pack('<4sI', b'pas1', 8), struct.pack('<4sI', b'pae1', 8)
    pieces, rim_pieces = [], []
    def rectangle(x, y, width, height, color, output=pieces, prefix='P_CardShape', shape=None):
        index = sum(pane_name(p).startswith(prefix + '_') for p in output)
        pane = rename(panes['P_Icon_00'][:104], f'{prefix}_{index:02}')
        pane[8:12] = bytes((3, 0, 255, 0))
        struct.pack_into('<3f', pane, 44, x, y, 0)
        struct.pack_into('<3f', pane, 56, 0, 0, 0)
        struct.pack_into('<2f', pane, 68, 1, 1)
        struct.pack_into('<2f', pane, 76, width, height)
        pane[84:100] = bytes(color) * 4
        struct.pack_into('<HBB', pane, 100, solid_index, 0, int(shape is not None))
        # PIC flag bit 0 selects native geometry. The shape index follows the
        # UV array; there are no UVs on these solid-color shapes (offset 104).
        if shape is not None:
            pane.extend(struct.pack('<I', shape))
        struct.pack_into('<I', pane, 4, len(pane))
        output.append(bytes(pane))
    # The white backing is drawn BEFORE the oval, so its lower outline is
    # occluded by the oval. The dark edge belongs to the foreground card icon.
    rectangle(0, 0, 94, 66, white, rim_pieces, 'P_CardRim', shape=0)
    rectangle(0, 0, 82, 54, INK, prefix='P_CardEdge', shape=1)
    rectangle(0, 0, 76, 48, (84, 183, 120, 255), prefix='P_CardFace', shape=2)
    rectangle(0, 10, 76, 8, CREAM)
    rectangle(-23, -7, 14, 12, (240, 204, 101, 255))
    rectangle(16, -12, 22, 3, CREAM)
    rectangle(9, -19, 36, 3, CREAM)
    number = text_pane(panes['T_Num_00'], NUMBER, '999,999,999', -25, 0, 260, 56, .76, True)
    unit = rename(panes['N_Pos_00'], 'N_CardBells_00')
    # The native badge window uses a right-hand origin: place that origin
    # eight units inside the oval's right edge, then retain the requested shift left.
    struct.pack_into('<f', unit, 44, OVAL_CENTER[0] + OVAL_SIZE[0] / 2 - RIM - BADGE_SHIFT)
    additions = [bytes(group), opening, bytes(backing), opening, *rim_pieces, closing,
                 outline, bytes(background), bytes(icon), opening,
                 *pieces, closing, number, bytes(unit), opening, badge, bytes(label), closing, closing]
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


def patch_archive(data):
    start, entries = archive_entries(data)
    output = bytearray(data[:start])
    count = 0
    for entry in sorted(entries, key=lambda entry: entry.start):
        content = entry.content
        if content.startswith(b'SARC'):
            content, nested = patch_archive(content)
            count += nested
        elif entry.name == f'blyt/{ASSET}.bflyt':
            content = edit_layout(content)
            count += 1
        alignment = min((start + entry.start) & -(start + entry.start), 4096)
        output.extend(bytes((-len(output)) % alignment))
        offset = len(output) - start
        output.extend(content)
        struct.pack_into('<2I', output, entry.node_offset + 8, offset, len(output) - start)
    struct.pack_into('<I', output, 8, len(output))
    return bytes(output), count


def build():
    packed = (ROOT.parent / f'vanilla-3.0.3/Layout/{ASSET}.Nin_NX_NVN.zs').read_bytes()
    assert hashlib.sha256(packed).hexdigest() == EXPECTED, 'Unsupported original currency layout'
    original = zstd.decompress(packed)
    modified, count = patch_archive(original)
    assert count == 1
    old, new = resources(original), resources(modified)
    assert old.keys() == new.keys()
    for name in old:
        if not name.endswith(f'/{ASSET}.bflyt'):
            assert new[name] == old[name], name
    return {f'romfs/Layout/{ASSET}.Nin_NX_NVN.zs': zstd.compress(modified)}
