"""Package placed-world ladder climbing without replacing earlier prototypes."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package
from package_auto_vault import prototype_files


def main():
    root = package.ROOT
    elf = (root / 'project.elf').read_bytes()
    for marker in (b'auto placed ladder: READY; prototype 1;', b'auto equip: READY; prototype 2;', b'DIAGNOSTIC: auto-vault-v4'):
        if marker not in elf: raise ValueError('Build first: sh tools/build.sh --auto-vault-diagnostic')
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_placed-ladder-prototype1_for_acnh_v3_0_3'
    output = root / 'dist'
    output.mkdir(exist_ok=True)
    binary, source = output / f'{stem}.zip', output / f'{stem}_source.zip'
    for path in (binary, source):
        if path.exists(): raise SystemExit(f'Refusing to replace existing archive: {path}')
    files = prototype_files()
    key = 'QoL Enhancements - Controls/romfs/qol-enhancements/config/controls.ini'
    for setting in (b'auto_ladder', b'auto_equip_traversal'):
        old, new = setting + b' = false\n', setting + b' = true\n'
        assert files[key].count(old) == 1
        files[key] = files[key].replace(old, new)
    files['README.txt'] = (
        'AUTO PLACED LADDER PROTOTYPE 1 - ACNH 3.0.3\n\n'
        f'Based on QoL Enhancements {package.release_version()}. Includes the working pole,\n'
        'handheld ladder and silent pocket-tool borrowing/restoration features.\n'
        'Adds automatic climbing of placed ladders through native object search.\n\n'
        'Fully close ACNH. Merge the included Core and Controls folders into\n'
        'your existing mod folders, replacing matching files, then restart.\n'
        'Use one Core copy. Keep your other modules and personal settings.\n'
        'No core.ini is included; this updates an existing installation.\n'
        'The supplied Controls config enables auto_vault, auto_ladder and\n'
        'auto_equip_traversal. Check controls.custom.ini for false overrides.\n\n'
        'Push firmly against a placed ladder or vine, facing it. After the\n'
        'normal hop delay it should climb using the placed object, without\n'
        'borrowing a pocket ladder. Try both uphill and downhill, empty hands\n'
        'and holding another tool. Try vines and different ladder variants.\n'
        'Placed climbing uses auto_ladder and does not require auto equipping\n'
        'or a handheld ladder in your pockets. The normal native climb action\n'
        'handles held-tool visibility. Check that your held tool returns normally.\n\n'
        'Check turning/releasing during the delay, a blocked landing, ordinary\n'
        'narrow hops and the existing handheld pole/ladder behavior. Start in\n'
        'single-player. Compiled checks pass; live variants remain unverified.\n'
        'If it fails, preserve the Eden log and describe object/direction/tool.\n'
        'Startup should say: auto placed ladder: READY; prototype 1.\n\n'
        'Set auto_ladder=false and restart to disable all automatic climbing.\n'
        'Reinstall the preserved auto-traversal-prototype2 Core to roll back\n'
        'only this new extension. Older archives are preserved locally.\n\n'
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
        for setting in (b'auto_vault', b'auto_ladder', b'auto_equip_traversal'):
            assert setting + b' = true\n' in archive.read(key)
    with ZipFile(source) as archive: assert archive.testzip() is None
    for path in (binary, source): print(path)


if __name__ == '__main__': main()
