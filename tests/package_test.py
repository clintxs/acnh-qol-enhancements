"""Check final suite structure, marker versions, manifests and inherited assets."""
from pathlib import Path
from zipfile import ZipFile
import hashlib
import json
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import package

files = package.suite_files()
core = package.CORE_FOLDER
assert {name for name in files if '/exefs/' in name} == {
    f'{core}/exefs/subsdk6', f'{core}/exefs/main.npdm'}
assert not any(name.endswith('.ips') for name in files)
assert {name.split('/')[0] for name in files if '/' in name} == {
    core, *(f'QoL Enhancements - {label}' for label in package.LABELS)}
configs = [name for name in files if '/config/' in name]
assert len(configs) == 5
assert not any(name.endswith('/faster-dialogue.custom.ini') for name in files)
for name in configs:
    assert files[name].decode() == f'enabled = true\nversion = {package.release_version()}\n'
assert sum('/Layout/' in name for name in files) == 6
reference = json.loads((Path(__file__).parent / 'baseline_hashes.json').read_text())
assert hashlib.sha256(files[f'{core}/exefs/main.npdm']).hexdigest() == reference['npdm_sha256']
assert files['START HERE - CORE IS REQUIRED.txt'].startswith(b'CORE IS REQUIRED')
manifest = json.loads(files['SHA256.json'])
assert set(manifest) == set(files) - {'SHA256.json'}
for name, digest in manifest.items(): assert hashlib.sha256(files[name]).hexdigest() == digest
# Resource generation must reproduce the current standalone modules exactly.
for name, digest in reference['assets'].items():
    assert hashlib.sha256(files[name]).hexdigest() == digest, name
print('PASS: six mod folders, subsdk6 only, five versioned configs, six unchanged resource payloads and SHA-256 manifest')
custom = package.custom_files()
personal = [name for name in custom if '/config/' in name]
assert len(personal) == 1 and personal[0].endswith('/faster-dialogue.custom.ini')
assert custom[personal[0]] == files['faster-dialogue.custom.ini.example']
assert b'faster_player = true\n' in custom[personal[0]]
assert not any('/exefs/' in name or name.endswith('/faster-dialogue.ini') for name in custom)
print('PASS: separate personal preset, no module marker or executable, suite updates cannot replace custom INI')

stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_for_acnh_v3_0_3'
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
        assert not any(name.endswith('/core/patches.hpp') for name in source.namelist())
    with ZipFile(package.ROOT / 'dist' / f'{stem}_faster-dialogue-custom-example.zip') as result:
        assert result.testzip() is None and set(result.namelist()) == set(custom)
        for name, data in custom.items(): assert result.read(name) == data
    print('PASS: exact binary ZIP contents and source ZIP excludes generated binaries/game assets')
