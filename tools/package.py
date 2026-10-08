"""Package one suite ZIP and corresponding source; never replace a release."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from compression import zstd
import hashlib
import json
import re
import build_catalog_badge_layout as badges
from build_layouts import build as build_fitting

ROOT = Path(__file__).resolve().parents[1]
CORE_FOLDER = 'QoL Enhancements - Core (REQUIRED)'
LABELS = ('Catalog', 'Pockets', 'Faster Dialogue', 'Fitting Room', 'Amiibo')

def release_version():
    text = (ROOT / 'source/program/core/release.hpp').read_text()
    return re.search(r'ReleaseVersion\[\] = "(\d+\.\d+\.\d+)"', text)[1]

def catalog_layouts():
    layouts = {}
    for name, expected_hash in badges.ASSETS.items():
        asset = name + '.Nin_NX_NVN.zs'
        packed = (ROOT.parent / 'vanilla-3.0.3/Layout' / asset).read_bytes()
        if hashlib.sha256(packed).hexdigest() != expected_hash:
            raise ValueError(f'Unsupported original layout: {asset}')
        original = zstd.decompress(packed)
        entry = f'blyt/{name}.bflyt'
        modified, count = badges.patch_archive(original, entry, visible=False)
        assert count == 1
        key = badges.verify(original, modified, entry)
        panes = [section for section in badges.layout_sections(badges.resources(modified)[key]) if badges.is_badge(section)]
        assert len(panes) == 9 and all(not (pane[8] & 1) for pane in panes)
        layouts[f'romfs/Layout/{asset}'] = zstd.compress(modified)
        assert zstd.decompress(layouts[f'romfs/Layout/{asset}']) == modified
    return layouts

def suite_files():
    version = release_version()
    files = {
        'README.txt': (ROOT / 'README.txt').read_bytes(),
        'START HERE - CORE IS REQUIRED.txt': (
            'CORE IS REQUIRED FOR ALL FEATURE MODULES TO WORK.\n\n'
            'Install and enable QoL Enhancements - Core (REQUIRED).\n'
            'For the full suite, copy all six QoL Enhancements folders into the\n'
            'game mods folder. For selected features, copy Core plus those folders.\n'
            'Disable older standalone versions and restart the game.\n'
            'See README.txt for details. This is an integration test build.\n'
        ).encode(),
        f'{CORE_FOLDER}/exefs/subsdk6': (ROOT / 'deploy/subsdk6').read_bytes(),
        f'{CORE_FOLDER}/exefs/main.npdm': (ROOT / 'deploy/main.npdm').read_bytes(),
        f'{CORE_FOLDER}/LICENSE': (ROOT / 'LICENSE').read_bytes(),
        'faster-dialogue.custom.ini.example': (ROOT / 'examples/faster-dialogue.custom.ini').read_bytes(),
    }
    assert files[f'{CORE_FOLDER}/exefs/subsdk6'][:4] == b'NSO0'
    assert files[f'{CORE_FOLDER}/exefs/main.npdm'][:4] == b'META'
    for label in LABELS:
        folder = ROOT / 'modules' / label
        configs = list(folder.rglob('config/*.ini'))
        assert len(configs) == 1
        config = configs[0].read_text()
        assert re.search(r'^version\s*=\s*' + re.escape(version) + r'\s*$', config, re.M), label
        for path in sorted(folder.rglob('*')):
            if path.is_file() and path.name != '.DS_Store':
                files[f'QoL Enhancements - {label}/{path.relative_to(folder)}'] = path.read_bytes()
    for label, assets in (('Catalog', catalog_layouts()), ('Fitting Room', build_fitting())):
        for name, data in assets.items():
            files[f'QoL Enhancements - {label}/{name}'] = data
    manifest = {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}
    files['SHA256.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    return files

def custom_files():
    # This separate, optional personal mod is intentionally absent from suite
    # module folders, so replacing those folders cannot overwrite preferences.
    return {
        'README.txt': (
            'OPTIONAL PERSONAL FASTER DIALOGUE SETTINGS\n\n'
            'Install the My Custom Faster Dialogue Settings folder alongside\n'
            'QoL Enhancements - Core and QoL Enhancements - Faster Dialogue.\n'
            'Enable all three in Eden and fully restart ACNH.\n\n'
            'This preset uses base 2x, held B/R 4x, and automatic 4x for\n'
            "Timmy, Tommy, Orville, Wilbur, Kapp'n, recognized UI and player/self messages.\n"
            'Speeds are nominal tiers based on the native fast-text rate.\n'
            'Edit romfs/qol-enhancements/config/faster-dialogue.custom.ini\n'
            'inside the personal mod. Keep this folder when updating the suite.\n'
            'Do not reinstall this preset over your edited personal settings.\n'
            'Values override faster-dialogue.ini; omitted keys inherit.\n'
            'The same settings work in either file.\n'
            'The personal file alone cannot enable an absent Faster Dialogue module.\n'
            'Use only one mod supplying this custom file.\n'
        ).encode(),
        'My Custom Faster Dialogue Settings/romfs/qol-enhancements/config/faster-dialogue.custom.ini':
            (ROOT / 'examples/faster-dialogue.custom.ini').read_bytes(),
    }

def write_source_archive(path):
    excluded = {'.git', '.vscode', '__pycache__', 'build', 'deploy', 'dist', 'out'}
    with ZipFile(path, 'x', ZIP_DEFLATED) as archive:
        for source in sorted(ROOT.rglob('*')):
            relative = source.relative_to(ROOT)
            if source.is_file() and not excluded.intersection(relative.parts):
                if source.suffix not in {'.elf', '.nso', '.npdm', '.pyc', '.map'} and source.name != '.DS_Store':
                    archive.write(source, f'qol-enhancements/{relative}')

def main():
    output = ROOT / 'dist'
    output.mkdir(exist_ok=True)
    stem = f'qol-enhancements-v{release_version().replace(".", "_")}_for_acnh_v3_0_3'
    binary, source = output / f'{stem}.zip', output / f'{stem}_source.zip'
    custom = output / f'{stem}_faster-dialogue-custom-example.zip'
    for path in (binary, source, custom):
        if path.exists(): raise SystemExit(f'Refusing to replace existing release: {path}')
    files = suite_files()
    with ZipFile(binary, 'w', ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()): archive.writestr(name, data)
    with ZipFile(custom, 'w', ZIP_DEFLATED) as archive:
        for name, data in sorted(custom_files().items()): archive.writestr(name, data)
    write_source_archive(source)
    for path in (binary, source, custom):
        with ZipFile(path) as archive: assert archive.testzip() is None
        print(path)

if __name__ == '__main__': main()
