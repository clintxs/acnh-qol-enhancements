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
    # Native rounded window has separate content and corner materials. Clone
    # both so no existing currency or RegionType animation is recolored.
    background = rename(panes['W_BaseL_00'], 'W_SavingsBase_00')
    struct.pack_into('<2f', background, 44, -25, -2)
    struct.pack_into('<2f', background, 76, 306, 128)
    for offset in (128, 168):
        old_index = struct.unpack_from('<H', background, offset)[0]
        material = bytearray(records[old_index])
        material[:28] = f'SavingsWindow{offset}'.encode().ljust(28, b'\0')
        material[40:44] = GREEN
        struct.pack_into('<H', background, offset, len(records))
        records.append(bytes(material))
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

    group = rename(panes['N_Pos_01'], CARD)
    group[8] = 2  # Starts hidden even if Core/module is disabled.
    icon = rename(panes['N_Pos_01'], 'N_CardIcon_00')
    struct.pack_into('<3f', icon, 44, 2, 52, 0)
    struct.pack_into('<f', icon, 64, -8)
    opening, closing = struct.pack('<4sI', b'pas1', 8), struct.pack('<4sI', b'pae1', 8)
    pieces = []
    def rectangle(x, y, width, height, color):
        pane = rename(panes['P_Icon_00'][:104], f'P_CardShape_{len(pieces):02}')
        pane[8:12] = bytes((3, 0, 255, 0))
        struct.pack_into('<3f', pane, 44, x, y, 0)
        struct.pack_into('<3f', pane, 56, 0, 0, 0)
        struct.pack_into('<2f', pane, 68, 1, 1)
        struct.pack_into('<2f', pane, 76, width, height)
        pane[84:100] = bytes(color) * 4
        struct.pack_into('<HBB', pane, 100, solid_index, 0, 0)
        struct.pack_into('<I', pane, 4, len(pane))
        pieces.append(bytes(pane))
    # Stepped silhouettes form rounded corners in native layout coordinates.
    for width, height in ((82, 42), (78, 50), (70, 54)):
        rectangle(0, 0, width, height, (20, 83, 53, 255))
    for width, height in ((76, 38), (72, 44), (66, 48)):
        rectangle(0, 0, width, height, (84, 183, 120, 255))
    rectangle(0, 10, 76, 8, CREAM)
    rectangle(-23, -7, 14, 12, (240, 204, 101, 255))
    rectangle(16, -12, 22, 3, CREAM)
    rectangle(9, -19, 36, 3, CREAM)
    number = text_pane(panes['T_Num_00'], NUMBER, '999,999,999', -25, 0, 260, 56, .76, True)
    label = text_pane(panes['T_Num_00'], 'T_SavingsLabel_00', 'Savings', -25, -45, 190, 30, .43, True)
    additions = [bytes(group), opening, bytes(background), bytes(icon), opening,
                 *pieces, closing, number, label, closing]
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
