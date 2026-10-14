"""Package one suite ZIP and corresponding source; never replace a release."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from compression import zstd
import argparse
import hashlib
import json
import re
import build_catalog_badge_layout as badges
from build_layouts import build as build_fitting
from build_shopping_card_layout import build as build_shopping
from shopping_card_texture import COLOR_LABELS, DEFAULT_PALETTE, PALETTES

ROOT = Path(__file__).resolve().parents[1]
CORE_FOLDER = 'QoL Enhancements - Core (REQUIRED)'
USER_GUIDE = ROOT / 'docs/user-guide.txt'
LABELS = ('Catalog', 'Pockets', 'Faster Dialogue', 'Fitting Room', 'Shopping Card', 'Crafting', 'Controls')

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
        'README.txt': USER_GUIDE.read_bytes(),
        'START HERE - CORE IS REQUIRED.txt': (
            'CORE IS REQUIRED FOR ALL FEATURE MODULES TO WORK.\n\n'
            'Install Core plus the feature modules you want.\n'
            'On Eden, copy the selected QoL Enhancements folders into the game mods folder.\n'
            'On Atmosphere, merge their exefs/romfs folders as described in README.txt.\n'
            'Disable older standalone versions and restart the game.\n'
            'See README.txt for details.\n'
        ).encode(),
        f'{CORE_FOLDER}/exefs/subsdk6': (ROOT / 'deploy/subsdk6').read_bytes(),
        f'{CORE_FOLDER}/exefs/main.npdm': (ROOT / 'deploy/main.npdm').read_bytes(),
        f'{CORE_FOLDER}/LICENSE': (ROOT / 'LICENSE').read_bytes(),
        f'{CORE_FOLDER}/romfs/qol-enhancements/config/core.ini':
            (ROOT / 'modules/Core/romfs/qol-enhancements/config/core.ini').read_bytes(),
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
    for label, assets in (('Catalog', catalog_layouts()), ('Fitting Room', build_fitting()), ('Shopping Card', build_shopping(DEFAULT_PALETTE))):
        for name, data in assets.items():
            files[f'QoL Enhancements - {label}/{name}'] = data
    manifest = {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}
    files['SHA256.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    return files

def custom_files():
    # Personal settings stay separate so replacing suite modules preserves them.
    return {
        'My Custom QoL Configuration/romfs/qol-enhancements/config/faster-dialogue.custom.ini':
            (ROOT / 'examples/faster-dialogue.custom.ini').read_bytes(),
    }


def optional_files():
    files = {'README.txt': USER_GUIDE.read_bytes(), **custom_files()}
    module = ROOT / 'modules/Shopping Card'
    for palette in PALETTES:
        if palette == DEFAULT_PALETTE:
            continue
        prefix = f'Shopping Card/{COLOR_LABELS[palette]}/QoL Enhancements - Shopping Card'
        # Include the regular marker so each variant is a complete replacement module.
        for path in sorted(module.rglob('*')):
            if path.is_file() and path.name != '.DS_Store':
                files[f'{prefix}/{path.relative_to(module)}'] = path.read_bytes()
        for path, data in build_shopping(palette).items():
            files[f'{prefix}/{path}'] = data
    files['SHA256.json'] = (json.dumps({
        name: hashlib.sha256(data).hexdigest() for name, data in files.items()
    }, indent=2) + '\n').encode()
    return files


def archive_stem(revision=1):
    suffix = f'_r{revision}' if revision > 1 else ''
    return f'qol-enhancements-v{release_version().replace(".", "_")}_for_acnh_v3_0_3{suffix}'


def write_source_archive(path):
    excluded = {'.git', '.vscode', '__pycache__', 'build', 'deploy', 'dist', 'out'}
    with ZipFile(path, 'x', ZIP_DEFLATED) as archive:
        for source in sorted(ROOT.rglob('*')):
            relative = source.relative_to(ROOT)
            if source.is_file() and not excluded.intersection(relative.parts):
                if source.suffix not in {'.elf', '.nso', '.npdm', '.pyc', '.map'} and source.name != '.DS_Store':
                    archive.write(source, f'qol-enhancements/{relative}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', type=int, default=1,
                        help='Prepare a new packaging revision without replacing previous archives.')
    args = parser.parse_args()
    if args.revision < 1:
        parser.error('--revision must be positive')
    output = ROOT / 'dist'
    output.mkdir(exist_ok=True)
    stem = archive_stem(args.revision)
    binary, source = output / f'{stem}.zip', output / f'{stem}_source.zip'
    optional = output / f'{stem}_optional-files.zip'
    for path in (binary, source, optional):
        if path.exists(): raise SystemExit(f'Refusing to replace existing release: {path}')
    for path, files in ((binary, suite_files()), (optional, optional_files())):
        with ZipFile(path, 'x', ZIP_DEFLATED) as archive:
            for name, data in sorted(files.items()): archive.writestr(name, data)
    write_source_archive(source)
    for path in (binary, source, optional):
        with ZipFile(path) as archive: assert archive.testzip() is None
        print(path)

if __name__ == '__main__': main()
