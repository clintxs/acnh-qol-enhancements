"""Core-only storage-payment playtest, retaining installed card colors/settings."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package


def main():
    root = package.ROOT
    elf = (root / 'project.elf').read_bytes()
    assert package.release_version().encode()+b'\0' in elf
    for command in (b'EventFlowActionBellCountDown', b'EventFlowQueryHasBellCount',
                    b'EventFlowActionUIMoneyAppear', b'EventFlowActionUIMoneyDisappear'):
        assert command+b'\0' in elf
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_shopping-storage_for_acnh_v3_0_3'
    binary, source = (root / 'dist' / (stem + suffix) for suffix in ('.zip', '_source.zip'))
    for path in (binary, source):
        if path.exists(): raise SystemExit(f'Refusing to replace existing archive: {path}')
    binary.parent.mkdir(exist_ok=True)
    core = package.CORE_FOLDER
    files = {
        f'{core}/exefs/subsdk6': (root / 'deploy/subsdk6').read_bytes(),
        f'{core}/exefs/main.npdm': (root / 'deploy/main.npdm').read_bytes(),
        f'{core}/LICENSE': (root / 'LICENSE').read_bytes(),
        'README.txt': (
            'SHOPPING CARD: TOM NOOK STORAGE PAYMENTS - ACNH 3.0.3\n\n'
            'Close ACNH. Merge the included Core folder into your existing\n'
            'QoL Enhancements - Core (REQUIRED) folder, replacing matching\n'
            'files. Keep one Core enabled and restart. Shopping Card must\n'
            'remain enabled. Your existing layouts, card colors and settings\n'
            'are retained.\n\n'
            'All six storage-expansion fees accept combined funds: savings,\n'
            'then pocket Bell bags, then wallet. The combined card balance\n'
            'appears during the storage payment dialogue. Other Tom Nook\n'
            'fees retain their original handling. No new items or save format.\n\n'
            'Try savings-only and mixed-funds payments, cancellation and\n'
            'insufficient funds. Confirm the counter returns to pocket Bells\n'
            'after leaving the storage dialogue. Automated checks pass;\n'
            'in-game confirmation is pending.\n\n'
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
