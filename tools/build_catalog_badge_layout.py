"""Build outlined catalog badges in their native text colors from the local original 3.0.3 assets.

This is a layout-only prototype, not an ownership indicator yet. No game asset is
stored in the source repository. Run with Python 3.14 (compression.zstd).

Field interpretation cross-checked against the open-source BFLYT readers:
https://github.com/exelix11/SwitchThemeInjector/tree/master/SwitchThemesCommon/Bflyt
The game-specific offsets/template are verified against the input hash below.
"""
from pathlib import Path
from compression import zstd
from dataclasses import dataclass
import argparse
import hashlib
import json
import struct
from zipfile import ZipFile, ZIP_DEFLATED

PROJECT = Path(__file__).resolve().parents[1]
ASSET_NAME = 'LCatalogBtn.Nin_NX_NVN.zs'
EXPECTED_INPUT_SHA256 = '4034706e1be6d4fec6a6c038eca19d53ea212904c225422abbb78b667d145c00'
LAYOUT_ENTRY = 'blyt/LCatalogBtn.bflyt'
BADGE_NAME = 'T_CatalogStar_00'
MOD_FOLDER = 'Catalog Aware Shops - Catalog Badge Layout Test'
RELEASE_NAME = MOD_FOLDER + ' 3.zip'
ASSETS = {
    'LCatalogBtn': EXPECTED_INPUT_SHA256,
    'LNatteeCatalogBtn': 'a6ef5b33ea1f5e453d178d95e35076a70fcd5b6e78f941b33cbd97f75e9a912b',
    'LBtnListShopSonkatsuBtn': '6193c22dab34fe23650467c81afa1ccbf35f373ab5cfd0216e877fb4344fde86',
    'LHotelCatalogBtn': '96341d9edb6a0fee74a513e4f28e5a933d83a6fcdd9bd93bb261a7b0364117af',
}
OUTLINE_CREAM = bytes.fromhex('fffbe8ff')
OUTLINE_PREFIX = 'T_StarOutline_'
# Eight slightly displaced copies form a silhouette behind the foreground glyph.
# This avoids relying on undocumented font-shadow rendering behavior.
OUTLINE_OFFSETS = ((-2, -2), (0, -2), (2, -2), (-2, 0),
                   (2, 0), (-2, 2), (0, 2), (2, 2))
PANE_TYPES = {b'pan1', b'pic1', b'txt1', b'prt1', b'wnd1', b'bnd1'}

# BFLYT section-relative offsets. Clone the local name pane to retain its font,
# text material and version-specific fields, rather than inventing a new ABI.
PANE_NAME = 12
PANE_NAME_BYTES = 24
POSITION = 44
SIZE = 76
TEXT_LENGTHS = 84
TEXT_ALIGNMENT = 92
LINE_ALIGNMENT = 93
TEXT_OFFSET = 100
FONT_SIZE = 112
CALL_NAME_OFFSET = 128
HEADER_BYTES = 20


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def pane_name(section):
    return section[PANE_NAME:PANE_NAME + PANE_NAME_BYTES].split(b'\0', 1)[0].decode('ascii')


def layout_sections(data):
    if data[:8] != b'FLYT\xff\xfe\x14\0' or u32(data, 12) != len(data):
        raise ValueError('Unexpected layout header or file length')
    sections = []
    position = HEADER_BYTES
    while position < len(data):
        size = u32(data, position + 4)
        if size < 8 or size % 4 or position + size > len(data):
            raise ValueError('Malformed layout section')
        sections.append(data[position:position + size])
        position += size
    if len(sections) != struct.unpack_from('<H', data, 16)[0]:
        raise ValueError('Layout section count does not match header')
    return sections


def icon_container_end(sections):
    """Locate the end of the verified icon parent (Miles uses N_Btn_02), so the badge draws after its existing children."""
    parents = []
    last_pane = None
    insertion = None
    for index, section in enumerate(sections):
        tag = section[:4]
        if tag in PANE_TYPES:
            last_pane = pane_name(section)
        elif tag == b'pas1':
            if last_pane is None:
                raise ValueError('Missing pane before hierarchy open')
            parents.append(last_pane)
            last_pane = None
        elif tag == b'pae1':
            if not parents:
                raise ValueError('Unbalanced pane hierarchy')
            if parents.pop() in ('N_Icon_00', 'N_Btn_02'):
                if insertion is not None:
                    raise ValueError('Duplicate icon container')
                insertion = index
    if parents or insertion is None:
        raise ValueError('Missing or unbalanced icon container')
    return insertion


def material_records(section):
    """Split the offset-indexed mat1 table; offsets are section-relative."""
    count = struct.unpack_from('<H', section, 8)[0]
    offsets = list(struct.unpack_from(f'<{count}I', section, 12))
    if offsets != sorted(offsets) or offsets[0] != 12 + count * 4:
        raise ValueError('Unexpected material table')
    return [section[start:end] for start, end in zip(offsets, offsets[1:] + [len(section)])]


def add_badge_materials(section):
    """Append independent text colors so existing text and animations stay intact.

    In these hash-verified v9 layouts, the name material has a 28-byte name,
    flags at 28, blend settings at 32, and black/white RGBA endpoints at 36/40.
    Its 44-byte record has no textures or internal offsets to relocate.
    """
    records = material_records(section)
    template = next(record for record in records if record[:28].split(b'\0')[0] == b'T_Name_00')
    if len(template) != 44 or template[:28].split(b'\0')[0] != b'T_Name_00':
        raise ValueError('Unexpected name material')
    first_new_index = len(records)
    for name, color in (('CatalogStarFill', template[40:44]), ('CatalogStarOutline', OUTLINE_CREAM)):
        material = bytearray(template)
        material[:28] = name.encode().ljust(28, b'\0')
        material[40:44] = color
        records.append(bytes(material))
    output = bytearray(section[:12])
    struct.pack_into('<H', output, 8, len(records))
    position = 12 + 4 * len(records)
    for record in records:
        output.extend(struct.pack('<I', position))
        position += len(record)
    output.extend(b''.join(records))
    struct.pack_into('<I', output, 4, len(output))
    return bytes(output), first_new_index


def is_badge(section):
    return section[:4] == b'txt1' and (pane_name(section) == BADGE_NAME or
                                      pane_name(section).startswith(OUTLINE_PREFIX))


def add_badge(layout, visible=True):
    sections = layout_sections(layout)
    panes = {pane_name(section): section for section in sections if section[:4] in PANE_TYPES}
    if BADGE_NAME in panes:
        raise ValueError('Catalog badge already exists')
    template = panes['T_Name_00']
    icon = panes['P_Icon_00']
    if template[:4] != b'txt1' or len(template) != 212 or u32(template, TEXT_OFFSET) != 168:
        raise ValueError('Unexpected native name-pane layout')
    material_section = next(i for i, section in enumerate(sections) if section[:4] == b'mat1')
    sections[material_section], foreground_material = add_badge_materials(sections[material_section])
    badge = bytearray(template)
    struct.pack_into('<H', badge, 88, foreground_material)
    badge[PANE_NAME:PANE_NAME + PANE_NAME_BYTES] = BADGE_NAME.encode().ljust(PANE_NAME_BYTES, b'\0')
    badge[8] = (badge[8] | 2) & ~1  # Inherit alpha; production badges start hidden.
    if visible:
        badge[8] |= 1  # Explicit visual-test mode only.
    badge[9] = icon[9]  # Same parent/origin convention as the item icon.
    # Mirror the palette's (-24,+19) offset to the icon's lower-right (+24,-19).
    icon_x, icon_y = struct.unpack_from('<2f', icon, POSITION)
    struct.pack_into('<2f', badge, POSITION, icon_x + 24, icon_y - 19)
    struct.pack_into('<2f', badge, SIZE, 44, 44)
    badge[TEXT_ALIGNMENT] = 0  # Center horizontally and vertically.
    badge[LINE_ALIGNMENT] = 2  # Center the single text line.
    struct.pack_into('<2f', badge, FONT_SIZE, 0.60, 0.60)
    # Both length fields count bytes, including the UTF-16 terminator. The unused
    # original tail is zeroed; no original @T_Name_00 lookup can overwrite ★.
    struct.pack_into('<2H', badge, TEXT_LENGTHS, 4, 4)
    struct.pack_into('<I', badge, CALL_NAME_OFFSET, 0)
    text_start = u32(badge, TEXT_OFFSET)
    badge[text_start:] = bytes(len(badge) - text_start)
    badge[text_start:text_start + 4] = '★\0'.encode('utf-16-le')
    outlines = []
    for index, (dx, dy) in enumerate(OUTLINE_OFFSETS):
        outline = bytearray(badge)
        name = f'{OUTLINE_PREFIX}{index:02}'
        outline[PANE_NAME:PANE_NAME + PANE_NAME_BYTES] = name.encode().ljust(PANE_NAME_BYTES, b'\0')
        struct.pack_into('<H', outline, 88, foreground_material + 1)
        struct.pack_into('<2f', outline, POSITION, icon_x + 24 + dx, icon_y - 19 + dy)
        outlines.append(bytes(outline))
    insertion = icon_container_end(sections)
    sections[insertion:insertion] = outlines + [bytes(badge)]
    header = bytearray(layout[:HEADER_BYTES])
    output = header + b''.join(sections)
    struct.pack_into('<I', output, 12, len(output))
    struct.pack_into('<H', output, 16, len(sections))
    return bytes(output)


@dataclass
class ArchiveEntry:
    name: str
    node_offset: int
    start: int
    end: int
    content: bytes


def archive_entries(data):
    """Read this little-endian SARC variant without changing its name/hash table."""
    if data[:8] != b'SARC\x14\0\xff\xfe' or u32(data, 8) != len(data):
        raise ValueError('Unexpected archive header or length')
    if data[20:24] != b'SFAT':
        raise ValueError('Missing SFAT')
    payload_start = u32(data, 12)
    count = struct.unpack_from('<H', data, 26)[0]
    names_start = 40 + count * 16
    entries = []
    for index in range(count):
        node = 32 + index * 16
        _, name_field, start, end = struct.unpack_from('<4I', data, node)
        name_start = names_start + (name_field & 0xffffff) * 4
        name = data[name_start:data.index(0, name_start)].decode('utf-8')
        if name_field >> 24 != 1 or not 0 <= start <= end <= len(data) - payload_start:
            raise ValueError('Unexpected archive node')
        entries.append(ArchiveEntry(name, node, start, end, data[payload_start + start:payload_start + end]))
    return payload_start, entries


def patch_archive(data, layout_entry=LAYOUT_ENTRY, visible=True):
    """Repack nested archives, preserving every resource except the target BFLYT."""
    payload_start, entries = archive_entries(data)
    output = bytearray(data[:payload_start])
    replacements = 0
    # Node order is by name hash; payload order is a separate concern.
    for entry in sorted(entries, key=lambda value: value.start):
        content = entry.content
        if content.startswith(b'SARC'):
            content, nested_count = patch_archive(content, layout_entry, visible)
            replacements += nested_count
        elif entry.name == layout_entry:
            content = add_badge(content, visible)
            replacements += 1
        original_absolute_start = payload_start + entry.start
        alignment = min(original_absolute_start & -original_absolute_start, 4096)
        output.extend(bytes((-len(output)) % alignment))
        start = len(output) - payload_start
        output.extend(content)
        struct.pack_into('<2I', output, entry.node_offset + 8, start, len(output) - payload_start)
    struct.pack_into('<I', output, 8, len(output))
    return bytes(output), replacements


def resources(data, prefix=''):
    result = {}
    for entry in archive_entries(data)[1]:
        name = prefix + entry.name
        if entry.content.startswith(b'SARC'):
            result.update(resources(entry.content, name + '/'))
        else:
            result[name] = entry.content
    return result


def verify(original, modified, layout_entry=LAYOUT_ENTRY):
    before, after = resources(original), resources(modified)
    assert before.keys() == after.keys()
    changed = [name for name in before if before[name] != after[name]]
    assert len(changed) == 1 and changed[0].endswith(layout_entry), changed
    old_sections = layout_sections(before[changed[0]])
    new_sections = layout_sections(after[changed[0]])
    badges = [section for section in new_sections if section[:4] == b'txt1' and pane_name(section) == BADGE_NAME]
    assert len(badges) == 1
    assert sum(is_badge(section) for section in new_sections) == 9
    assert [s for s in new_sections if not is_badge(s) and s[:4] != b'mat1'] == [s for s in old_sections if s[:4] != b'mat1']
    old_materials = material_records(next(s for s in old_sections if s[:4] == b'mat1'))
    new_materials = material_records(next(s for s in new_sections if s[:4] == b'mat1'))
    assert new_materials[:-2] == old_materials
    assert new_materials[-2][28:] == next(record for record in old_materials if record[:28].split(b'\0')[0] == b'T_Name_00')[28:]
    assert new_materials[-1][40:44] == OUTLINE_CREAM
    assert new_sections[icon_container_end(new_sections) - 1] == badges[0]
    assert badges[0][168:172] == '★\0'.encode('utf-16-le')
    return changed[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input-dir', type=Path, default=PROJECT.parent / 'vanilla-3.0.3/Layout')
    args = parser.parse_args()
    output_path = PROJECT / 'dist' / RELEASE_NAME
    if output_path.exists():
        raise SystemExit(f'Refusing to replace existing prototype: {output_path}')
    outputs, reports = {}, {}
    for name, expected_hash in ASSETS.items():
        asset_name = name + '.Nin_NX_NVN.zs'
        packed = (args.input_dir / asset_name).read_bytes()
        if hashlib.sha256(packed).hexdigest() != expected_hash:
            raise SystemExit(f'Unsupported source layout: {asset_name}; expected original ACNH 3.0.3.')
        original = zstd.decompress(packed)
        entry = f'blyt/{name}.bflyt'
        modified, count = patch_archive(original, entry)
        assert count == 1
        changed = verify(original, modified, entry)
        output = zstd.compress(modified)
        assert zstd.decompress(output) == modified
        outputs[asset_name] = output
        reports[asset_name] = dict(input_sha256=expected_hash,
            output_sha256=hashlib.sha256(output).hexdigest(), changed_resource=changed,
            added_panes=9, added_materials=2, original_panes_and_materials_preserved=True)
    report = dict(assets=reports, ownership_filter=False, runtime_tested=False,
                  fill_color='native name material per catalog', outline_rgba=OUTLINE_CREAM.hex())
    output_path.parent.mkdir(exist_ok=True)
    with ZipFile(output_path, 'w', ZIP_DEFLATED) as archive:
        for name, output in outputs.items():
            archive.writestr(f'{MOD_FOLDER}/romfs/Layout/{name}', output)
        archive.writestr(f'{MOD_FOLDER}/README.md', (PROJECT / 'CATALOG-BADGE-LAYOUT-TEST.md').read_bytes())
        archive.writestr(f'{MOD_FOLDER}/verification.json', json.dumps(report, indent=2) + '\n')
    with ZipFile(output_path) as archive:
        assert archive.testzip() is None
        assert len(archive.namelist()) == len(ASSETS) + 2 and not any('/exefs/' in n for n in archive.namelist())
    print(output_path)
    print('Verified four layouts: original panes, materials and other resources preserved.')


if __name__ == '__main__':
    main()
