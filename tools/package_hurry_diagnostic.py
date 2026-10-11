"""Package a Core-only hurry-v1 trace; preserve normal release archives/settings."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package


def diagnostic_files():
    root = package.ROOT
    if b'DIAGNOSTIC: hurry-v1' not in (root / 'project.elf').read_bytes():
        raise ValueError('Build first: sh tools/build.sh --hurry-diagnostic')
    core = package.CORE_FOLDER
    files = {
        f'{core}/exefs/subsdk6': (root / 'deploy/subsdk6').read_bytes(),
        f'{core}/exefs/main.npdm': (root / 'deploy/main.npdm').read_bytes(),
        f'{core}/LICENSE': (root / 'LICENSE').read_bytes(),
        'README.txt': (
            'Y TO HURRY - DIAGNOSTIC 1 - ACNH 3.0.3\n\n'
            'Fully close ACNH. Copy the exefs files from this Core folder into\n'
            'your existing QoL Enhancements - Core (REQUIRED) folder, replacing\n'
            'subsdk6 and main.npdm. Enable only one Core copy. Keep Controls\n'
            'enabled with hold_to_advance_dialogue=true. Faster Dialogue is\n'
            'optional. Keep personal settings and other feature folders.\n'
            'This archive supplies no config files.\n\n'
            'Restart, talk to an NPC, hold Y while text appears and once the\n'
            'message is waiting to advance, then release it. Try B/R too.\n'
            'Close ACNH and save the Eden log before starting another session.\n'
            'Send the log, or lines containing QoL Enhancements/Controls.\n'
            'Include Core startup or errors if there are no dialogue lines.\n\n'
            'DIAGNOSTIC: hurry-v1 identifies this build. Bounded HURRY traces\n'
            'report input, speed and advance stages, raw held buttons, Y,\n'
            'ownership, native states and the exact rejecting guard. They log\n'
            'even with log_debug=false. Startup/unheld messages have a separate\n'
            'budget from held Y. Includes the corrected input, waiting-page and\n'
            'mid-message input-pause handling, retaining choice protection.\n\n'
            f'Restore the normal {package.release_version()} Core after collecting the log.\n'
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
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_hurry-diagnostic1_for_acnh_v3_0_3'
    output = root / 'dist'
    output.mkdir(exist_ok=True)
    binary, source = output / f'{stem}.zip', output / f'{stem}_source.zip'
    for path in (binary, source):
        if path.exists(): raise SystemExit(f'Refusing to replace existing archive: {path}')
    files = diagnostic_files()
    with ZipFile(binary, 'x', ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()): archive.writestr(name, data)
    package.write_source_archive(source)
    with ZipFile(binary) as archive:
        assert archive.testzip() is None and set(archive.namelist()) == set(files)
        for name, data in files.items(): assert archive.read(name) == data
        assert not any('/config/' in name or '/Layout/' in name for name in archive.namelist())
    with ZipFile(source) as archive: assert archive.testzip() is None
    for path in (binary, source): print(path)


if __name__ == '__main__': main()
