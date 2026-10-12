"""Package Core and Controls for a focused auto-swim playtest; preserve releases."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package


def main():
    root = package.ROOT
    assert b'auto swim: READY; prototype 4; strong stick; 2s strokes; smooth diving' in (root / 'project.elf').read_bytes(), 'Build first: sh tools/build.sh'
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_auto-swim-prototype4_for_acnh_v3_0_3'
    binary, source = (root / 'dist' / (stem + suffix) for suffix in ('.zip', '_source.zip'))
    for path in (binary, source):
        if path.exists(): raise SystemExit(f'Refusing to replace existing archive: {path}')
    core = package.CORE_FOLDER
    key = 'QoL Enhancements - Controls/romfs/qol-enhancements/config/controls.ini'
    files = {
        f'{core}/exefs/subsdk6': (root / 'deploy/subsdk6').read_bytes(),
        f'{core}/exefs/main.npdm': (root / 'deploy/main.npdm').read_bytes(),
        f'{core}/LICENSE': (root / 'LICENSE').read_bytes(),
        key: (root / 'modules/Controls/romfs/qol-enhancements/config/controls.ini').read_bytes(),
        'README.txt': (
            'AUTO SWIM - PROTOTYPE 4 - RECURRING ARM STROKES - ACNH 3.0.3\n\n'
            'Fully close ACNH. Merge the included Core and Controls folders into\n'
            'your existing QoL Enhancements mod folders, replacing matching files.\n'
            'Enable Core and Controls, then restart. Use one Core copy.\n'
            'Other modules and personal custom settings can stay installed.\n'
            'This updates an existing installation; no core.ini is included.\n\n'
            'auto_swim=true is enabled by default at the surface and underwater.\n'
            'Push about 85% of the left stick travel to propel yourself; lighter\n'
            'tilt turns in place. Uses held A with an arm stroke about every two\n'
            'seconds and quieter foot kicking between strokes. Stroke requests\n'
            'wait for native animation entry instead of expiring after 200ms.\n'
            'Underwater cruise speed is retained between pulses to smooth motion.\n'
            'Physical A retains normal behavior. Y still dives/surfaces normally.\n'
            'Set auto_swim=false in controls.ini or controls.custom.ini and\n'
            'restart to restore ordinary swimming. Custom values take priority.\n\n'
            'Test all directions, gentle stick tilt, neutral/release, repeated\n'
            'strokes, diving, catching creatures, surfacing, water entry and\n'
            'shore exits. Check ordinary land A/Y actions and traversal too.\n'
            'Compiled regression checks pass;\n'
            'live Eden animation/cadence and multiplayer remain unverified.\n\n'
            'Built with Exlaunch (GPL-2.0); see LICENSE and matching source ZIP.\n'
        ).encode(),
    }
    assert files[f'{core}/exefs/subsdk6'][:4] == b'NSO0'
    assert files[f'{core}/exefs/main.npdm'][:4] == b'META'
    assert b'auto_swim = true\n' in files[key]
    files['SHA256.json'] = (json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in files.items()}, indent=2) + '\n').encode()
    with ZipFile(binary, 'x', ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()): archive.writestr(name, data)
    package.write_source_archive(source)
    with ZipFile(binary) as archive:
        assert archive.testzip() is None and set(archive.namelist()) == set(files)
        for name, data in files.items(): assert archive.read(name) == data
        manifest = json.loads(archive.read('SHA256.json'))
        assert all(hashlib.sha256(archive.read(name)).hexdigest() == digest for name, digest in manifest.items())
    with ZipFile(source) as archive:
        assert archive.testzip() is None
        assert any(name.endswith('/source/program/controls/auto_swim.cpp') for name in archive.namelist())
    print(binary)
    print(source)

if __name__ == '__main__': main()
