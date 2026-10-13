"""Run focused inherited regressions against the unified ELF and module paths."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GROUPS = {
    'core': ['page_input_arm_test.py', 'feature_activation_arm_test.py', 'native_bindings_arm_test.py', 'logging_arm_test.py', 'core_arm_test.py', 'recipe_preview_arm_test.py'],
    'shopping': ['shopping_layout_test.py', 'shopping_resource_arm_test.py', 'shopping_texture_arm_test.py', 'shopping_display_arm_test.py', 'shopping_cash_arm_test.py', 'shopping_storage_arm_test.py'],
    'dialogue': ['dialogue_arm_test.py', 'dialogue_names_arm_test.py'],
    'crafting': ['crafting_arm_test.py', 'quantity_shortcut_arm_test.py'],
    'controls': ['auto_swim_arm_test.py', 'swimming_camera_arm_test.py', 'auto_vault_arm_test.py', 'auto_ladder_arm_test.py', 'auto_placed_ladder_arm_test.py', 'auto_traversal_swap_arm_test.py', 'hold_y_arm_test.py', 'controls_dialogue_arm_test.py', 'dialogue_hurry_arm_test.py', 'dialogue_catch_hurry_arm_test.py'],
    'pockets': ['pocket_registration_arm_test.py', 'pocket_lifecycle_arm_test.py', 'pocket_commands_arm_test.py', 'storage_hint_arm_test.py', 'recycling_claim_arm_test.py', 'hold_select_arm_test.py'],  # Includes inherited arm_stack_test regressions.
    'fitting': ['caption_contract_arm_test.py', 'purchase_arm_test.py', 'feedback_arm_test.py', 'fitting_hint_arm_test.py'],
    'catalog': ['art_stars_arm_test.py', 'museum_creature_arm_test.py', 'restaurant_recipe_dialogue_arm_test.py', 'villager_recipe_dialogue_arm_test.py', 'dialogue_ui_sources_arm_test.py', 'catalog_variation_labels_arm_test.py',
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
        for name in ('preview_session_test', 'shared_ui_test', 'pocket_commands_test', 'settings_schema_test', 'hook_registry_test', 'native_bindings_test', 'catalog_settings_test', 'config_test', 'core_settings_test', 'controls_settings_test', 'pockets_settings_test', 'dialogue_settings_test', 'crafting_settings_test', 'stack_menu_test', 'storage_policy_test', 'catalog_test', 'bag_policy_test', 'shopping_payment_test'):
            exe = Path(temporary) / name
            subprocess.run(['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT / 'source'),
                            str(ROOT / 'tests' / f'{name}.cpp'), '-o', str(exe)], check=True)
            arguments = [str(exe)]
            if name == 'dialogue_settings_test':
                arguments.append(str(ROOT / 'examples/faster-dialogue.custom.ini'))
            if name == 'catalog_test':
                arguments.append(str(ROOT / 'modules/Pockets/romfs/qol-enhancements/messages/pockets.ini'))
            subprocess.run(arguments, check=True)
    for group in args.groups or GROUPS:
        for name in GROUPS[group]:
            print(f'Checking {group}: {name}', flush=True)
            subprocess.run(['python3', str(ROOT / 'tests' / name)], check=True, env=env)

if __name__ == '__main__': main()
