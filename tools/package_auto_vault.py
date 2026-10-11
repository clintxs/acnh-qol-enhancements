"""Package the opt-in equipped-pole prototype without replacing release ZIPs."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package


def prototype_files():
    root = package.ROOT
    if b'auto vault: READY; prototype 3;' not in (root / 'project.elf').read_bytes():
        raise ValueError('Build the auto-vault prototype first: sh tools/build.sh')
    core = package.CORE_FOLDER
    controls = 'QoL Enhancements - Controls'
    config = (root / 'modules/Controls/romfs/qol-enhancements/config/controls.ini').read_bytes()
    assert config.count(b'auto_vault = false\n') == 1
    files = {
        f'{core}/exefs/subsdk6': (root / 'deploy/subsdk6').read_bytes(),
        f'{core}/exefs/main.npdm': (root / 'deploy/main.npdm').read_bytes(),
        f'{core}/LICENSE': (root / 'LICENSE').read_bytes(),
        f'{controls}/romfs/qol-enhancements/config/controls.ini':
            config.replace(b'auto_vault = false\n', b'auto_vault = true\n'),
        'README.txt': (
            'AUTO VAULT - EQUIPPED POLE PROTOTYPE 3 - ACNH 3.0.3\n\n'
            f'Based on QoL Enhancements {package.release_version()}. Live Eden validation is pending.\n'
            'Fully close ACNH. Copy this Core folder into your existing\n'
            'QoL Enhancements - Core (REQUIRED) folder, replacing subsdk6 and\n'
            'main.npdm. Copy the Controls folder into your existing Controls\n'
            'folder. Enable Core and Controls, then restart. Use one Core copy.\n'
            'Keep other feature folders and personal custom settings. This ZIP\n'
            'does not include core.ini; it updates an existing installation.\n'
            'If controls.custom.ini sets auto_vault=false, change it to true\n'
            'or remove that key so the supplied regular true setting applies.\n\n'
            'Hold a vaulting pole. Push the stick firmly toward a vaultable\n'
            'crossing and keep pushing, as for an ordinary narrow-river hop.\n'
            'After the native hop delay, the game should vault without A.\n'
            'Narrow hops retain priority; manual A still uses the native path.\n'
            'No automatic equipping, unequipping or ladder use is included.\n\n'
            'Test a normal vaultable river, a narrow hop, an intermediate\n'
            'island, diagonal banks, a short pond crossing, wide water, ocean,\n'
            'river mouths and occupied landings. Try another tool and empty\n'
            'hands, release/change direction during the hold, and manual A.\n'
            'Start with single-player; multiplayer behavior is unverified.\n'
            'Report whether it vaults, does nothing, repeats, or misfires.\n\n'
            'To disable, set auto_vault=false in controls.ini or your personal\n'
            'controls.custom.ini and restart. The normal module default is\n'
            'false; this test ZIP explicitly enables the prototype. To restore\n'
            f'the original executable, reinstall the normal {package.release_version()} Core.\n\n'
            'Built with Exlaunch (GPL-2.0); see LICENSE and matching source ZIP.\n'
        ).encode(),
    }
    assert files[f'{core}/exefs/subsdk6'][:4] == b'NSO0'
    assert files[f'{core}/exefs/main.npdm'][:4] == b'META'
    files['SHA256.json'] = (json.dumps({name: hashlib.sha256(data).hexdigest()
                                      for name, data in files.items()}, indent=2) + '\n').encode()
    return files


def main():
    root = package.ROOT
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_auto-vault-prototype3_for_acnh_v3_0_3'
    output = root / 'dist'
    output.mkdir(exist_ok=True)
    binary, source = output / f'{stem}.zip', output / f'{stem}_source.zip'
    for path in (binary, source):
        if path.exists(): raise SystemExit(f'Refusing to replace existing archive: {path}')
    files = prototype_files()
    with ZipFile(binary, 'x', ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()): archive.writestr(name, data)
    package.write_source_archive(source)
    with ZipFile(binary) as archive:
        assert archive.testzip() is None and set(archive.namelist()) == set(files)
        for name, data in files.items(): assert archive.read(name) == data
        manifest = json.loads(archive.read('SHA256.json'))
        assert all(hashlib.sha256(archive.read(name)).hexdigest() == digest for name, digest in manifest.items())
        assert b'auto_vault = true\n' in archive.read(
            'QoL Enhancements - Controls/romfs/qol-enhancements/config/controls.ini')
    with ZipFile(source) as archive: assert archive.testzip() is None
    for path in (binary, source): print(path)


if __name__ == '__main__': main()
