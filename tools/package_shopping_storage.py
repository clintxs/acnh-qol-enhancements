"""Core-only Shopping Card playtest, retaining installed card colors/settings."""
import argparse
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shop-entry', action='store_true', help='Package the silent shop-entry fix.')
    args = parser.parse_args()
    root = package.ROOT
    elf = (root / 'project.elf').read_bytes()
    assert package.release_version().encode()+b'\0' in elf
    for command in (b'EventFlowActionBellCountDown', b'EventFlowQueryHasBellCount',
                    b'EventFlowActionUIMoneyAppear', b'EventFlowActionUIMoneyDisappear'):
        assert command+b'\0' in elf
    focus = 'shopping-entry' if args.shop_entry else 'shopping-storage'
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_{focus}_for_acnh_v3_0_3'
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
            ('SHOPPING CARD: SILENT SHOP ENTRY - ACNH 3.0.3\n\n' if args.shop_entry else
             'SHOPPING CARD: TOM NOOK STORAGE PAYMENTS - ACNH 3.0.3\n\n') +
            'Close ACNH. Merge the included Core folder into your existing\n'
            'QoL Enhancements - Core (REQUIRED) folder, replacing matching\n'
            'files. Keep one Core enabled and restart. Shopping Card must\n'
            'remain enabled. Your existing layouts, card colors and settings\n'
            'are retained.\n\n' +
            ('Shop entry initializes the card balance silently, including when\n'
             'funds changed while away or a previous animation was interrupted.\n'
             'Purchase and sale animations/sounds remain enabled. The confirmed\n'
             'storage-upgrade counter dismissal fix is retained.\n\n'
             'Check entry after changing funds outdoors and re-entering shops.\n'
             'Confirm purchases/sales still animate and play their usual sound.\n' if args.shop_entry else
            'All six storage-expansion fees accept combined funds: savings,\n'
            'then pocket Bell bags, then wallet. The combined card balance\n'
            'appears during the storage payment dialogue. Other Tom Nook\n'
            'fees retain their original handling. No new items or save format.\n\n'
            'Try savings-only and mixed-funds payments, cancellation and\n'
            'insufficient funds. Confirm the card finishes hiding without a\n'
            'flash of pocket Bells; ordinary counters still work afterward.\n') +
            'Automated checks pass;\n'
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
