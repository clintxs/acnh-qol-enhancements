"""Package bounded auto-vault traces with the existing prototype enabled."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package
from package_auto_vault import prototype_files


def main():
    root = package.ROOT
    if b'DIAGNOSTIC: auto-vault-v4' not in (root / 'project.elf').read_bytes():
        raise ValueError('Build first: sh tools/build.sh --auto-vault-diagnostic')
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_auto-vault-diagnostic4_for_acnh_v3_0_3'
    output = root / 'dist'
    output.mkdir(exist_ok=True)
    binary, source = output / f'{stem}.zip', output / f'{stem}_source.zip'
    for path in (binary, source):
        if path.exists(): raise SystemExit(f'Refusing to replace existing archive: {path}')
    files = prototype_files()
    files['README.txt'] = (
        'AUTO VAULT - DIAGNOSTIC 4 - ACNH 3.0.3\n\n'
        f'Based on QoL Enhancements {package.release_version()}. This transfers stable bank validation to movement.\n'
        'Prototype 3 retains native validation and timing; live confirmation is pending.\n\n'
        'Fully close ACNH. Copy the included Core and Controls folders into\n'
        'your existing mod folders, replacing their matching files. Enable\n'
        'one Core copy and Controls. Keep other modules and personal settings.\n'
        'If controls.custom.ini sets auto_vault=false, change it to true.\n'
        'No core.ini is included; this updates an existing installation.\n\n'
        'Restart and hold a vaulting pole. Push firmly against a river that\n'
        'you can vault across, keep pushing for 3 seconds, then release.\n'
        'At the same spot, press A to confirm a manual vault works. If handy,\n'
        'also try a narrow river where walking triggers an ordinary hop.\n'
        'Close ACNH and preserve the Eden log before the next session.\n'
        'Share the updated log for diagnosis.\n\n'
        'The log identifies DIAGNOSTIC: auto-vault-v4 and records bounded\n'
        'VAULT movement/entry/search messages even with log_debug=false. These show\n'
        'movement state, action, equipped tool kind, input, hold timer, hop\n'
        'geometry eligibility and native vault probe/commit results. Search lines\n'
        'identify rejected candidate queries, raw native return vs preparation\n'
        'guard, one-use transfer, current/alternate positions and facing.\n'
        'Normal releases do not include the extra observation hooks or traces.\n'
        'No auto equipping, unequipping or ladder use is included.\n\n'
        f'To restore, reinstall the normal {package.release_version()} Core and set\n'
        'auto_vault=false in Controls.\n\n'
        'Built with Exlaunch (GPL-2.0); see LICENSE and matching source ZIP.\n'
    ).encode()
    del files['SHA256.json']
    files['SHA256.json'] = (json.dumps({name: hashlib.sha256(data).hexdigest()
                                      for name, data in files.items()}, indent=2) + '\n').encode()
    with ZipFile(binary, 'x', ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()): archive.writestr(name, data)
    package.write_source_archive(source)
    with ZipFile(binary) as archive:
        assert archive.testzip() is None and set(archive.namelist()) == set(files)
        for name, data in files.items(): assert archive.read(name) == data
        manifest = json.loads(archive.read('SHA256.json'))
        assert all(hashlib.sha256(archive.read(name)).hexdigest() == digest for name, digest in manifest.items())
    with ZipFile(source) as archive: assert archive.testzip() is None
    for path in (binary, source): print(path)


if __name__ == '__main__': main()
