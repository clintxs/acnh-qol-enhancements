"""Package the player-bank-v2 diagnostic Core without replacing any release."""
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import package


def main():
    root = package.ROOT
    marker = b'DIAGNOSTIC: player-bank-v2'
    if marker not in (root / 'project.elf').read_bytes():
        raise SystemExit('Build the diagnostic Core first: sh tools/build.sh --verbose-dialogue')
    stem = f'qol-enhancements-v{package.release_version().replace(".", "_")}_player-diagnostic2_for_acnh_v3_0_3'
    output = root / 'dist'
    output.mkdir(exist_ok=True)
    binary, source = output / f'{stem}.zip', output / f'{stem}_source.zip'
    for path in (binary, source):
        if path.exists(): raise SystemExit(f'Refusing to replace existing archive: {path}')
    core = package.CORE_FOLDER
    files = {
        f'{core}/exefs/subsdk6': (root / 'deploy/subsdk6').read_bytes(),
        f'{core}/exefs/main.npdm': (root / 'deploy/main.npdm').read_bytes(),
        f'{core}/LICENSE': (root / 'LICENSE').read_bytes(),
        'README.txt': (
            'FASTER PLAYER / NPC DIALOGUE DIAGNOSTIC - ACNH 3.0.3\n\n'
            'This build adds logging; it does not fix the reported category failure.\n'
            'Fully close ACNH. Replace the existing QoL Enhancements - Core\n'
            '(REQUIRED) folder with this Core. Enable only one Core copy.\n'
            'Keep your Faster Dialogue module and personal settings mod enabled.\n'
            'In the personal file, confirm faster_player = true.\n'
            'Do not reinstall the personal preset over your edited settings.\n\n'
            'Restart ACNH, use a crafting table, catch a fish/bug, and speak to\n'
            'one listed NPC without B/R. Briefly hold B/R during a message too.\n'
            'Close ACNH, then copy the Eden log before starting another session.\n'
            'Send the log or its lines containing QoL Enhancements/Faster Dialogue.\n'
            'Include Core startup/BOOT lines if there are no dialogue lines.\n\n'
            'DIAGNOSTIC: player-bank-v2 identifies this build. READY confirms\n'
            'the settings that actually loaded. MESSAGE reports current/cached\n'
            'banks, labels and native reveal gates even if SELECT is never reached.\n'
            'SELECT reports the classifier result and requested speed tier.\n'
            'Both traces are bounded; restart if the MESSAGE limit is reached.\n\n'
            'Restore the normal 0.2.1 Core after collecting the log.\n'
            'Built with Exlaunch (GPL-2.0); see LICENSE and the matching source ZIP.\n'
        ).encode(),
    }
    assert files[f'{core}/exefs/subsdk6'][:4] == b'NSO0'
    assert files[f'{core}/exefs/main.npdm'][:4] == b'META'
    files['SHA256.json'] = (json.dumps({name: hashlib.sha256(data).hexdigest()
                                      for name, data in files.items()}, indent=2) + '\n').encode()
    with ZipFile(binary, 'x', ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()): archive.writestr(name, data)
    package.write_source_archive(source)
    for path in (binary, source):
        with ZipFile(path) as archive: assert archive.testzip() is None
        print(path)


if __name__ == '__main__': main()
