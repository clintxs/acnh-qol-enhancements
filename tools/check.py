"""Run focused inherited regressions against the unified ELF and module paths."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GROUPS = {
    'core': ['core_arm_test.py'],
    'shopping': ['shopping_layout_test.py', 'shopping_resource_arm_test.py', 'shopping_texture_arm_test.py', 'shopping_display_arm_test.py'],
    'dialogue': ['dialogue_arm_test.py', 'dialogue_names_arm_test.py'],
    'crafting': ['crafting_arm_test.py'],
    'pockets': ['storage_hint_arm_test.py'],  # Includes full arm_stack_test.
    'fitting': ['purchase_arm_test.py', 'feedback_arm_test.py'],  # Each includes runtime coverage.
    'catalog': ['dialogue_ui_sources_arm_test.py', 'catalog_variation_labels_arm_test.py',
                'miles_name_binding_arm_test.py', 'miles_and_counted_names_arm_test.py',
                'counted_names_native_test.py', 'fitting_catalog_stars_arm_test.py'],
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('groups', nargs='*', choices=list(GROUPS))
    args = parser.parse_args()
    env = dict(os.environ)
    env['PYTHONPATH'] = str(ROOT.parent / 'Grab 5/working/python')
    with tempfile.TemporaryDirectory(prefix='qol-check-') as temporary:
        for name in ('config_test', 'core_settings_test', 'dialogue_settings_test', 'stack_menu_test', 'storage_policy_test', 'catalog_test', 'bag_policy_test'):
            exe = Path(temporary) / name
            subprocess.run(['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT / 'source'),
                            str(ROOT / 'tests' / f'{name}.cpp'), '-o', str(exe)], check=True)
            arguments = [str(exe)]
            if name == 'catalog_test':
                arguments.append(str(ROOT / 'modules/Pockets/romfs/qol-enhancements/messages/pockets.ini'))
            subprocess.run(arguments, check=True)
    for group in args.groups or GROUPS:
        for name in GROUPS[group]:
            print(f'Checking {group}: {name}', flush=True)
            subprocess.run(['python3', str(ROOT / 'tests' / name)], check=True, env=env)

if __name__ == '__main__': main()
