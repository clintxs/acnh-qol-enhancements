"""Check final suite structure, marker versions, manifests and inherited assets."""
from pathlib import Path
from zipfile import ZipFile
import argparse
import hashlib
import json
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import package

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--revision', type=int, default=1)
args = parser.parse_args()
files = package.suite_files()
core = package.CORE_FOLDER
assert {name for name in files if '/exefs/' in name} == {
    f'{core}/exefs/subsdk6', f'{core}/exefs/main.npdm'}
assert not any(name.endswith('.ips') for name in files)
assert {name.split('/')[0] for name in files if '/' in name} == {
    core, *(f'QoL Enhancements - {label}' for label in package.LABELS)}
configs = [name for name in files if '/config/' in name]
assert len(configs) == 8
assert not any(name.endswith('/faster-dialogue.custom.ini') for name in files)
core_config = f'{core}/romfs/qol-enhancements/config/core.ini'
assert files[core_config] == (package.ROOT / 'modules/Core/romfs/qol-enhancements/config/core.ini').read_bytes()
assert b'amiibo_timeout_frames = 9000\n' in files[core_config]
assert not any('/amiibo.ini' in name or 'QoL Enhancements - Amiibo/' in name for name in files)
for name in configs:
    if name == core_config: continue
    assert files[name].decode().startswith(f'enabled = true\nversion = {package.release_version()}\n'), name
    label = name.split('/')[0].removeprefix('QoL Enhancements - ')
    relative = name.split('/',1)[1]
    assert files[name] == (package.ROOT / 'modules' / label / relative).read_bytes()
assert b'hold_to_advance_dialogue = true\n' in files['QoL Enhancements - Controls/romfs/qol-enhancements/config/controls.ini']
assert b'hold_to_advance_dialogue' not in files['QoL Enhancements - Faster Dialogue/romfs/qol-enhancements/config/faster-dialogue.ini']
pockets_config = files['QoL Enhancements - Pockets/romfs/qol-enhancements/config/pockets.ini']
assert b'enable_multiselect = true\n' in pockets_config
captions = files['QoL Enhancements - Pockets/romfs/qol-enhancements/messages/pockets.ini']
assert b'optimized_ui.' not in captions
assert captions.count(b'qol_pockets.hold_wear=') == 16
assert b'boost_ui_messages = true\n' in files['QoL Enhancements - Faster Dialogue/romfs/qol-enhancements/config/faster-dialogue.ini']
assert sum('/Layout/' in name for name in files) == 7
reference = json.loads((Path(__file__).parent / 'baseline_hashes.json').read_text())
assert hashlib.sha256(files[f'{core}/exefs/main.npdm']).hexdigest() == reference['npdm_sha256']
assert files['START HERE - CORE IS REQUIRED.txt'].startswith(b'CORE IS REQUIRED')
manifest = json.loads(files['SHA256.json'])
assert set(manifest) == set(files) - {'SHA256.json'}
for name, digest in manifest.items(): assert hashlib.sha256(files[name]).hexdigest() == digest
# Resource generation must reproduce the reviewed module baselines exactly.
for name, digest in reference['assets'].items():
    assert hashlib.sha256(files[name]).hexdigest() == digest, name
print('PASS: eight mod folders, subsdk6 only, seven versioned feature configs and default Core settings, seven layout resources, revised basket and default Island Inverted Shopping Card layout and SHA-256 manifest')
custom = package.custom_files()
personal = [name for name in custom if '/config/' in name]
assert len(personal) == 1 and personal[0].endswith('/faster-dialogue.custom.ini')
assert personal[0] == 'My Custom QoL Configuration/romfs/qol-enhancements/config/faster-dialogue.custom.ini'
assert custom[personal[0]] == (package.ROOT / 'examples/faster-dialogue.custom.ini').read_bytes()
assert 'faster-dialogue.custom.ini.example' not in files
assert b'boost_unnamed_messages = true\n' in custom[personal[0]]
assert not any('/exefs/' in name or name.endswith('/faster-dialogue.ini') for name in custom)
print('PASS: separate personal preset, no module marker or executable, suite updates cannot replace custom INI')

optional = package.optional_files()
assert optional['README.txt'] == files['README.txt'] == package.USER_GUIDE.read_bytes()
assert all(optional[name] == data for name, data in custom.items())
assert not any('/exefs/' in name or 'Island Inverted/' in name for name in optional)
assert {name.split('/')[1] for name in optional if name.startswith('Shopping Card/')} == {
    'ABD Teal', 'Bell Gold', 'Hotel Blue', 'Poki Red', 'Island'}
from build_shopping_card_layout import build as shopping_layout
for palette, label in package.COLOR_LABELS.items():
    generated = shopping_layout(palette)
    if palette == package.DEFAULT_PALETTE:
        for path, data in generated.items():
            assert files[f'QoL Enhancements - Shopping Card/{path}'] == data
        continue
    prefix = f'Shopping Card/{label}/QoL Enhancements - Shopping Card'
    for path, data in generated.items():
        assert optional[f'{prefix}/{path}'] == data
    assert optional[f'{prefix}/romfs/qol-enhancements/config/shopping-card.ini'] == files[
        'QoL Enhancements - Shopping Card/romfs/qol-enhancements/config/shopping-card.ini']
manifest = json.loads(optional['SHA256.json'])
assert set(manifest) == set(optional) - {'SHA256.json'}
for name, digest in manifest.items(): assert hashlib.sha256(optional[name]).hexdigest() == digest
print('PASS: Optional Files contains five complete color variants, exact custom settings, shared README and SHA-256 manifest; default excluded')

stem = package.archive_stem(args.revision)
archive = package.ROOT / 'dist' / f'{stem}.zip'
if archive.exists():
    with ZipFile(archive) as result:
        assert result.testzip() is None and set(result.namelist()) == set(files)
        for name, data in files.items(): assert result.read(name) == data
    with ZipFile(package.ROOT / 'dist' / f'{stem}_source.zip') as source:
        assert source.testzip() is None
        assert any(name.endswith('/source/program/core/modules.cpp') for name in source.namelist())
        assert not any(Path(name).suffix in {'.elf','.nso','.npdm','.zs','.ips','.pyc'} for name in source.namelist())
        assert not any('/.git/' in name or '/build/' in name or '/dist/' in name for name in source.namelist())
        assert any(name.endswith('/source/program/dialogue/faster_dialogue.cpp') for name in source.namelist())
        assert any(name.endswith('/source/program/controls/game/pickup_native.cpp') for name in source.namelist())
        assert any(name.endswith('/source/program/controls/game/hurry_native.cpp') for name in source.namelist())
        assert any(name.endswith('/source/program/controls/controls_module.cpp') for name in source.namelist())
        assert any(name.endswith('/modules/Controls/romfs/qol-enhancements/config/controls.ini') for name in source.namelist())
        assert not any(name.endswith('/core/patches.hpp') for name in source.namelist())
    with ZipFile(package.ROOT / 'dist' / f'{stem}_optional-files.zip') as result:
        assert result.testzip() is None and set(result.namelist()) == set(optional)
        for name, data in optional.items(): assert result.read(name) == data
    print('PASS: exact binary ZIP contents and source ZIP excludes generated binaries/game assets')

caption_path = 'QoL Enhancements - Fitting Room/romfs/qol-enhancements/messages/fitting-room.ini'
assert b'[USen]\nfitting_room.add_to_bag=Bag\n' in files[caption_path]
assert files[caption_path] == (package.ROOT/'modules/Fitting Room/romfs/qol-enhancements/messages/fitting-room.ini').read_bytes()
print('PASS: editable fitting-room caption ships with Fitting Room independently of Pockets')

assert b'fitting_room.remove_from_bag=' not in files[caption_path]

import configparser
captions = configparser.ConfigParser(interpolation=None)
captions.read_string(files[caption_path].decode('utf-8'))
assert set(captions.sections()) == {'JPja','USen','USes','USfr','USpt','EUen','EUes','EUfr','EUde','EUit','EUpt','EUnl','EUru','KRko','CNzh','TWzh'}
for locale in captions.sections():
    assert list(captions[locale]) == ['fitting_room.add_to_bag']
    assert captions[locale]['fitting_room.add_to_bag'].strip()
print('PASS: fixed Bag action caption for all 16 supported locales')
