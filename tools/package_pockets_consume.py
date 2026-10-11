"""Package a focused Core/Pockets update without replacing full releases."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package


def main():
    root = package.ROOT
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_pockets-consume1_for_acnh_v3_0_3'
    output = root / 'dist'
    output.mkdir(exist_ok=True)
    binary, source = output / f'{stem}.zip', output / f'{stem}_source.zip'
    for path in (binary, source):
        if path.exists(): raise SystemExit(f'Refusing to replace existing archive: {path}')
    files = {
        'QoL Enhancements - Core (REQUIRED)/exefs/subsdk6': (root / 'deploy/subsdk6').read_bytes(),
        'QoL Enhancements - Core (REQUIRED)/exefs/main.npdm': (root / 'deploy/main.npdm').read_bytes(),
        'README.txt': (
            'POCKETS CONSUME SHORTCUT - ACNH 3.0.3\n\n'
            'Close ACNH and merge these Core/Pockets folders into the existing\n'
            'QoL Enhancements installation. Keep just one Core enabled.\n'
            'Other module folders and personal custom INIs are unchanged.\n'
            'Back up customized Pockets messages before replacing that file.\n\n'
            'ZR now selects Eat, Eat 1 or Drink when available and enabled.\n'
            'Hold/Wear/Put Away take priority as the less destructive choice;\n'
            'consume is the fallback. Eat 10 Turnips is excluded.\n'
            'The hint is Use Item. Edit qol_pockets.hold_wear in the Pockets\n'
            'messages INI to customize/localize it; restart after editing.\n'
            'No native message archive is replaced.\n\n'
            'Automated tests pass; live Eden testing is still required.\n'
            'Try food, stacked fruit, a drink, full food energy, a tool,\n'
            'clothing and an empty slot. One press should perform one action.\n'
            'This normal Core build retains current traversal code, but does\n'
            'not change Controls settings or include diagnostic logging.\n\n'
            'Built with Exlaunch (GPL-2.0); see LICENSE and matching source ZIP.\n'
        ).encode(),
        'LICENSE': (root / 'LICENSE').read_bytes(),
    }
    for path in (root / 'modules/Pockets').rglob('*'):
        if path.is_file() and path.name != '.DS_Store':
            files['QoL Enhancements - Pockets/' + path.relative_to(root / 'modules/Pockets').as_posix()] = path.read_bytes()
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
