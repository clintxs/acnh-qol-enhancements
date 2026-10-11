"""Package automatic pocket-tool borrowing without replacing earlier prototypes."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package
from package_auto_vault import prototype_files


def main():
    root = package.ROOT
    elf = (root / 'project.elf').read_bytes()
    for marker in (b'auto equip: READY; prototype 2;', b'auto ladder: READY; prototype 2;', b'DIAGNOSTIC: auto-vault-v4'):
        if marker not in elf: raise ValueError('Build first: sh tools/build.sh --auto-vault-diagnostic')
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_auto-traversal-prototype2_for_acnh_v3_0_3'
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
        'AUTO TRAVERSAL - SILENT AUTO EQUIP PROTOTYPE 2 - ACNH 3.0.3\n\n'
        f'Based on QoL Enhancements {package.release_version()}. Includes the working pole\n'
        'and uphill/downhill ladder traversal features. This adds silent\n'
        'equipping from pockets and restoration after the crossing.\n\n'
        'Fully close ACNH. Merge the included Core and Controls folders into\n'
        'your existing mod folders, replacing matching files, then restart.\n'
        'Use one Core copy. Keep your other modules and personal settings.\n'
        'No core.ini is included; this updates an existing installation.\n'
        'The supplied Controls config enables auto_vault, auto_ladder and\n'
        'auto_equip_traversal. If controls.custom.ini overrides any of these\n'
        'keys to false, update that override to true.\n\n'
        'Keep a pole and ladder in your pockets. With empty hands, push firmly\n'
        'against a vaultable river, then try a cliff face and a cliff edge.\n'
        'After the normal hop delay, the needed tool should equip, traverse,\n'
        'and put itself away. Repeat while holding a net, shovel or other tool:\n'
        'the previous tool should return after landing. Equipping and putting\n'
        'away should be immediate, without turning to face the camera.\n'
        'Test rivers to the north, south, east and west. Also try holding the\n'
        'ladder at a river and the pole at a cliff. Already holding the needed\n'
        'tool keeps the previous equipped-tool behavior.\n\n'
        'Check narrow ordinary hops, missing tools, blocked landings, releasing\n'
        'or turning during equip, and manually changing tools. Manual tool\n'
        'selection cancels restoration. A failed/cancelled climb should restore\n'
        'the previous tool once normal movement resumes. Start in single-player.\n'
        'Compiled checks pass; live animation timing and multiplayer are unverified.\n\n'
        'This diagnostic build logs bounded VAULT movement/entry/search lines\n'
        'and SWAP events even when log_debug=false. Startup should say:\n'
        'auto equip: READY; prototype 2. Preserve the Eden log if it fails.\n\n'
        'To disable only automatic equipping, set auto_equip_traversal=false\n'
        'and restart; equipped-tool traversal continues. Reinstall the preserved\n'
        'auto-ladder-prototype2 Core to roll back the executable.\n\n'
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
