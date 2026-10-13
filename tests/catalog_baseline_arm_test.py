"""Replay unchanged Catalog assertions and compare native history call traces.

Each replay runs in a fresh process so imported fixtures use that ELF's own
symbols/relocations. QOL_BASELINE_ELF selects the preserved accepted build.
Synthetic UI/save views do not qualify live resource loading or rendering.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = (
    'catalog_badges_arm_test.py', 'miles_badges_arm_test.py',
    'nook_catalog_stars_arm_test.py', 'shop_catalog_stars_arm_test.py',
    'art_stars_arm_test.py', 'museum_creature_arm_test.py',
    'restaurant_recipe_dialogue_arm_test.py', 'villager_recipe_dialogue_arm_test.py',
    'dialogue_ui_sources_arm_test.py', 'catalog_variation_labels_arm_test.py',
    'miles_name_binding_arm_test.py', 'miles_and_counted_names_arm_test.py',
    'counted_names_native_test.py', 'fitting_catalog_stars_arm_test.py',
)

def replay(script, baseline):
    saved = Path(baseline).read_bytes() if baseline else None
    original_read = Path.read_bytes
    def read(path):
        if saved is not None and path == ROOT / 'project.elf': return saved
        return original_read(path)
    with patch.object(Path, 'read_bytes', read):
        from catalog_query_arm_test import NativeQuery
        from unicorn import UC_HOOK_CODE
        digest = hashlib.sha256()
        count = 0
        # Trace only native semantic calls, with item/recipe values rather than
        # stack addresses. These entries run real ARM; fixture stubs still keep
        # their inherited ABI/read-lock/history-immutability assertions.
        item_arguments = {0x254a1e0: 1, 0x254ccd0: 1, 0x1a00960: 0, 0x26bf130: 1}
        addresses = (*item_arguments, 0x19d7260, 0x251e658, 0x251288c,
                     0x2482d8c, 0x2483354, 0x24830c0, 0x200, 0x204)
        original_init = NativeQuery.__init__
        def init(machine, base):
            original_init(machine, base)
            def trace(u, pc, size, data):
                nonlocal count
                offset = pc - base
                record = [offset]
                if offset in item_arguments:
                    record.append(bytes(u.mem_read(machine.reg(item_arguments[offset]), 8)).hex())
                elif offset == 0x19d7260:
                    record.append(bytes(u.mem_read(machine.reg(0) + 8, 8)).hex())
                elif offset == 0x251e658:
                    record.extend((machine.reg(1), bytes(u.mem_read(machine.reg(2), 1)).hex()))
                elif offset == 0x251288c:
                    record.extend((bytes(u.mem_read(machine.reg(0), 2)).hex(),
                                   bytes(u.mem_read(machine.reg(1), 1)).hex()))
                digest.update((json.dumps(record) + '\n').encode())
                count += 1
            for address in addresses:
                machine.u.hook_add(UC_HOOK_CODE, trace, begin=base+address, end=base+address)
        with patch.object(NativeQuery, '__init__', init):
            runpy.run_path(str(ROOT / 'tests' / script), run_name='__main__')
        print(json.dumps({'native_calls': count, 'trace_sha256': digest.hexdigest()}))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replay', choices=SCRIPTS)
    parser.add_argument('--saved')
    args = parser.parse_args()
    if args.replay:
        replay(args.replay, args.saved)
        return
    baseline = os.environ.get('QOL_BASELINE_ELF')
    if not baseline:
        print('SKIP Catalog baseline replay (set QOL_BASELINE_ELF)')
        return
    total = 0
    for script in SCRIPTS:
        command = [sys.executable, str(Path(__file__).resolve()), '--replay', script]
        current = subprocess.check_output(command, text=True)
        previous = subprocess.check_output(command + ['--saved', str(Path(baseline).resolve())], text=True)
        assert current == previous, (script, current, previous)
        evidence = json.loads(current.splitlines()[-1])
        total += evidence['native_calls']
        print(f'PASS Catalog baseline: {script}; {evidence["native_calls"]} identical native calls', flush=True)
    print(f'PASS Catalog baseline: {len(SCRIPTS)} inherited scenario suites; {total} native calls match accepted ELF')

if __name__ == '__main__': main()
