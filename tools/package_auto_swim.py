"""Package Core and Controls for a focused auto-swim playtest; preserve releases."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package


def main():
    root = package.ROOT
    assert b'auto swim: READY; analog speed; delayed 1.5s 62.5 percent boosts=%s; interruptible strokes' in (root / 'project.elf').read_bytes(), 'Build first: sh tools/build.sh'
    assert b'camera tilt: READY; native tilt positions while swimming forward' in (root / 'project.elf').read_bytes(), 'Build camera support first'
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_swimming-prototype13_for_acnh_v3_0_3'
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
            'AUTO SWIM - PROTOTYPE 13 - CONTROLS INTEGRATION AND OPTIONAL BOOST - ACNH 3.0.3\n\n'
            'Fully close ACNH. Merge the included Core and Controls folders into\n'
            'your existing QoL Enhancements mod folders, replacing matching files.\n'
            'Enable Core and Controls, then restart. Use one Core copy.\n'
            'Other modules and personal custom settings can stay installed.\n'
            'Disable/remove the old Swimming folder. Move personal auto_swim\n'
            'preferences from swimming.custom.ini into controls.custom.ini.\n'
            'This updates an existing installation; no core.ini is included.\n\n'
            'auto_swim=true is enabled by default at the surface and underwater.\n'
            'Stick travel controls speed beyond a small neutral dead zone.\n'
            'Starting movement requests one opening arm stroke. After that,\n'
            'press A for a boost or hold A to repeat when its 2s cooldown ends.\n'
            'Boosts start half a second into the arm stroke, adding 62.5% of\n'
            'full cruise speed and then fading over 1.5 seconds.\n'
            'Ready strokes interrupt the current kick animation; the cooldown\n'
            'starts when an arm stroke begins.\n'
            'Quiet foot kicking continues between strokes. Underwater speed\n'
            'stays continuous across animation transitions. Stop/start and\n'
            'diving/surfacing do not bypass the cooldown. Y keeps priority.\n'
            'Both auto_swim and swim_stroke_boost default to true in Controls.\n'
            'Set swim_stroke_boost=false for cosmetic strokes without propulsion.\n'
            'Set auto_swim=false for vanilla swimming movement.\n'
            'Use controls.ini or controls.custom.ini and restart after changes.\n'
            'The normal three camera tilt positions always work while swimming\n'
            'forward with Controls enabled, even with auto_swim=false.\n'
            'Other native camera locks remain active; there is no camera toggle.\n\n'
            'Test all directions, gentle stick tilt, neutral/release, repeated\n'
            'strokes, diving, catching creatures, surfacing, water entry and\n'
            'shore exits. Check all three camera tilt positions while swimming forward.\n'
            'Check ordinary land A/Y actions and traversal too.\n'
            'Compiled regression checks pass;\n'
            'live Eden animation/cadence and multiplayer remain unverified.\n\n'
            'Built with Exlaunch (GPL-2.0); see LICENSE and matching source ZIP.\n'
        ).encode(),
    }
    assert files[f'{core}/exefs/subsdk6'][:4] == b'NSO0'
    assert files[f'{core}/exefs/main.npdm'][:4] == b'META'
    assert b'auto_swim = true\n' in files[key]
    assert b'swim_stroke_boost = true\n' in files[key]
    assert b'camera_tilt_while_swimming' not in files[key]
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
        for source_file in ('auto_swim.cpp', 'camera_tilt.cpp', 'settings.hpp'):
            assert any(name.endswith('/source/program/controls/' + source_file) for name in archive.namelist())
    print(binary)
    print(source)

if __name__ == '__main__': main()
