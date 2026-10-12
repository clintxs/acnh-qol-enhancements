"""Core-only hold-A selection feedback playtest, preserving existing releases."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package


def main():
    root = package.ROOT
    assert b'HOLD_SELECT_READY select/deselect with Decide feedback' in (root / 'project.elf').read_bytes()
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_hold-select-sound1_for_acnh_v3_0_3'
    binary, source = (root / 'dist' / (stem + suffix) for suffix in ('.zip', '_source.zip'))
    for path in (binary, source):
        if path.exists(): raise SystemExit(f'Refusing to replace existing archive: {path}')
    core = package.CORE_FOLDER
    files = {
        f'{core}/exefs/subsdk6': (root / 'deploy/subsdk6').read_bytes(),
        f'{core}/exefs/main.npdm': (root / 'deploy/main.npdm').read_bytes(),
        f'{core}/LICENSE': (root / 'LICENSE').read_bytes(),
        'README.txt': (
            'HOLD-A MULTISELECT SOUND TEST - ACNH 3.0.3\n\n'
            'Close ACNH. Merge the included Core folder into your existing\n'
            'QoL Enhancements - Core (REQUIRED) folder, replacing matching\n'
            'files. Keep just one Core enabled, then restart. Pockets must\n'
            'remain enabled. No configuration or caption files are replaced.\n\n'
            'Holding A and moving to select/deselect items now replaces the\n'
            'item cursor cue with the normal A/confirm cue. Ordinary movement,\n'
            'backtracking over items already in the desired state, and normal\n'
            'A clicks are unchanged. Native limits/capacity rules still apply.\n\n'
            'Test selling and shed storage: select, deselect, backtrack, release\n'
            'A, and try an empty/disabled slot or a selection-capacity limit.\n'
            'Automated tests passed; audible Eden confirmation is pending.\n\n'
            'Built with Exlaunch (GPL-2.0); see LICENSE and matching source ZIP.\n'
        ).encode(),
    }
    files['SHA256.json'] = (json.dumps({name: hashlib.sha256(data).hexdigest()
                                      for name, data in files.items()}, indent=2) + '\n').encode()
    with ZipFile(binary, 'x', ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()): archive.writestr(name, data)
    package.write_source_archive(source)
    with ZipFile(binary) as archive:
        assert archive.testzip() is None
        for name, data in files.items(): assert archive.read(name) == data
    with ZipFile(source) as archive: assert archive.testzip() is None
    for path in (binary, source): print(path)


if __name__ == '__main__': main()
