"""Package Core, Controls and Swimming for a focused auto-swim playtest; preserve releases."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package


def main():
    root = package.ROOT
    assert b'auto swim: READY; prototype 8; analog speed; delayed 1s 100 percent boosts; interruptible strokes' in (root / 'project.elf').read_bytes(), 'Build first: sh tools/build.sh'
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_swimming-prototype8_for_acnh_v3_0_3'
    binary, source = (root / 'dist' / (stem + suffix) for suffix in ('.zip', '_source.zip'))
    for path in (binary, source):
        if path.exists(): raise SystemExit(f'Refusing to replace existing archive: {path}')
    core = package.CORE_FOLDER
    key = 'QoL Enhancements - Swimming/romfs/qol-enhancements/config/swimming.ini'
    files = {
        f'{core}/exefs/subsdk6': (root / 'deploy/subsdk6').read_bytes(),
        f'{core}/exefs/main.npdm': (root / 'deploy/main.npdm').read_bytes(),
        f'{core}/LICENSE': (root / 'LICENSE').read_bytes(),
        key: (root / 'modules/Swimming/romfs/qol-enhancements/config/swimming.ini').read_bytes(),
        'QoL Enhancements - Controls/romfs/qol-enhancements/config/controls.ini':
            (root / 'modules/Controls/romfs/qol-enhancements/config/controls.ini').read_bytes(),
        'README.txt': (
            'AUTO SWIM - PROTOTYPE 8 - DELAYED ONE-SECOND BOOST - ACNH 3.0.3\n\n'
            'Fully close ACNH. Merge the included Core, Controls and Swimming folders into\n'
            'your existing QoL Enhancements mod folders, replacing matching files.\n'
            'Enable Core and Swimming, then restart. Controls is independent. Use one Core copy.\n'
            'Other modules and personal custom settings can stay installed.\n'
            'Move any auto_swim setting from controls.custom.ini to\n'
            'swimming.custom.ini. Controls no longer owns this setting.\n'
            'This updates an existing installation; no core.ini is included.\n\n'
            'auto_swim=true is enabled by default at the surface and underwater.\n'
            'Stick travel controls speed beyond a small neutral dead zone.\n'
            'Starting movement requests one opening arm stroke. After that,\n'
            'press A for a boost or hold A to repeat when its 2s cooldown ends.\n'
            'Boosts start half a second into the arm stroke, adding 100% of\n'
            'full cruise speed and then fading over one full second.\n'
            'Ready strokes interrupt the current kick animation; the cooldown\n'
            'starts when an arm stroke begins.\n'
            'Quiet foot kicking continues between strokes. Underwater speed\n'
            'stays continuous across animation transitions. Stop/start and\n'
            'diving/surfacing do not bypass the cooldown. Y keeps priority.\n'
            'Set auto_swim=false in swimming.ini or swimming.custom.ini and\n'
            'restart to restore ordinary swimming. Custom values take priority.\n'
            'Disabling/removing the Swimming folder also restores vanilla swimming.\n\n'
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
    assert b'auto_swim' not in files['QoL Enhancements - Controls/romfs/qol-enhancements/config/controls.ini']
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
        assert any(name.endswith('/source/program/swimming/auto_swim.cpp') for name in archive.namelist())
    print(binary)
    print(source)

if __name__ == '__main__': main()
