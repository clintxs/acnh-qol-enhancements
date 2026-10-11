"""Package the equipped-ladder test with the confirmed equipped-pole feature."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package
from package_auto_vault import prototype_files


def main():
    root = package.ROOT
    elf = (root / 'project.elf').read_bytes()
    if b'auto ladder: READY; prototype 1;' not in elf or b'DIAGNOSTIC: auto-vault-v4' not in elf:
        raise ValueError('Build first: sh tools/build.sh --auto-vault-diagnostic')
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_auto-ladder-prototype1_for_acnh_v3_0_3'
    output = root / 'dist'
    output.mkdir(exist_ok=True)
    binary, source = output / f'{stem}.zip', output / f'{stem}_source.zip'
    for path in (binary, source):
        if path.exists(): raise SystemExit(f'Refusing to replace existing archive: {path}')
    files = prototype_files()
    key = 'QoL Enhancements - Controls/romfs/qol-enhancements/config/controls.ini'
    assert files[key].count(b'auto_ladder = false\n') == 1
    files[key] = files[key].replace(b'auto_ladder = false\n', b'auto_ladder = true\n')
    files['README.txt'] = (
        'AUTO LADDER - EQUIPPED LADDER PROTOTYPE 1 - ACNH 3.0.3\n\n'
        f'Based on QoL Enhancements {package.release_version()}. Includes the working pole prototype 3.\n'
        'The ladder extension still needs live uphill and downhill testing.\n\n'
        'Fully close ACNH. Copy the included Core and Controls folders into\n'
        'your existing mod folders, replacing their matching files. Enable\n'
        'one Core copy and Controls, then restart. Keep other modules and\n'
        'personal settings. No core.ini is included; this updates an existing\n'
        'installation. The supplied Controls file enables auto_vault and\n'
        'auto_ladder. If controls.custom.ini overrides either key to false,\n'
        'change that override to true to test that feature.\n\n'
        'Hold a ladder. Push firmly into a cliff face for a second or two.\n'
        'Also try walking against a cliff edge from above. The game should\n'
        'perform its normal ladder climb after the ordinary hop hold delay.\n'
        'If either direction does nothing, try manual A at the same spot.\n'
        'Try a blocked landing, a corner/diagonal cliff, release mid-hold,\n'
        'another tool, and the working pole vault. No auto equip/unequip.\n\n'
        'This diagnostic build records bounded VAULT movement/entry/search\n'
        'lines for both tools, even with log_debug=false. Tool 0x11 is a\n'
        'ladder; 0x28 is the pole. The startup marker is auto ladder: READY;\n'
        'prototype 1. Preserve the Eden log after testing if it fails.\n'
        'The normal build excludes these extra diagnostic observers.\n\n'
        'To disable only the new feature, set auto_ladder=false and restart.\n'
        f'To restore the previous executable, reinstall the normal {package.release_version()} Core\n'
        'or the working auto-vault diagnostic4 Core. Older ZIPs are preserved.\n\n'
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
        assert b'auto_ladder = true\n' in archive.read(key)
        assert b'auto_vault = true\n' in archive.read(key)
    with ZipFile(source) as archive: assert archive.testzip() is None
    for path in (binary, source): print(path)


if __name__ == '__main__': main()
