"""Package optional appearance replacements; preserve the main suite release."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import argparse
import hashlib
import json
import build_shopping_card_layout as card
from package import release_version

ROOT = Path(__file__).resolve().parents[1]
FOLDER = 'QoL Enhancements - Shopping Card'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', type=int, default=1)
    args = parser.parse_args()
    if args.revision < 1:
        parser.error('--revision must be positive')
    version = release_version().replace('.', '_')
    suite = ROOT / 'dist' / f'qol-enhancements-v{version}_for_acnh_v3_0_3.zip'
    revision = f'_r{args.revision}' if args.revision > 1 else ''
    output = ROOT / 'dist' / f'shopping-card-colors-v{version}_for_acnh_v3_0_3{revision}.zip'
    if output.exists():
        raise SystemExit(f'Refusing to replace existing color options: {output}')
    files = {'README.md': (ROOT / 'SHOPPING_CARD_COLORS.md').read_bytes()}
    for name in card.PALETTES:
        for path, data in card.build(name).items():
            files[f'{name.replace("-", " ").title()}/{FOLDER}/{path}'] = data
            if name == 'green':
                # The fallback must restore the user's known-working release,
                # rather than a newly regenerated approximation of that art.
                with ZipFile(suite) as previous:
                    assert previous.read(f'{FOLDER}/{path}') == data
    files['SHA256.json'] = (json.dumps({
        name: hashlib.sha256(data).hexdigest() for name, data in files.items()
    }, indent=2) + '\n').encode()
    with ZipFile(output, 'x', ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
    with ZipFile(output) as archive:
        assert archive.testzip() is None and set(archive.namelist()) == set(files)
        for name, data in files.items():
            assert archive.read(name) == data
    print(output)


if __name__ == '__main__':
    main()
