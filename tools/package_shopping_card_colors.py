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
from shopping_card_texture import COLOR_LABELS, DEFAULT_PALETTE



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', type=int, default=1)
    parser.add_argument('--palette', choices=tuple(card.PALETTES), help='Package just one theme.')
    args = parser.parse_args()
    if args.revision < 1:
        parser.error('--revision must be positive')
    version = release_version().replace('.', '_')
    revision = f'_r{args.revision}' if args.revision > 1 else ''
    option = args.palette or 'colors'
    output = ROOT / 'dist' / f'shopping-card-{option}-v{version}_for_acnh_v3_0_3{revision}.zip'
    if output.exists():
        raise SystemExit(f'Refusing to replace existing color options: {output}')
    files = {'README.md': (ROOT / 'SHOPPING_CARD_COLORS.md').read_bytes()}
    palettes = (args.palette,) if args.palette else tuple(
        name for name in card.PALETTES if name != DEFAULT_PALETTE)
    for name in palettes:
        prefix = f'Shopping Card/{COLOR_LABELS[name]}/{FOLDER}'
        for path in sorted((ROOT / 'modules/Shopping Card').rglob('*')):
            if path.is_file() and path.name != '.DS_Store':
                files[f'{prefix}/{path.relative_to(ROOT / "modules/Shopping Card")}'] = path.read_bytes()
        for path, data in card.build(name).items():
            files[f'{prefix}/{path}'] = data
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
