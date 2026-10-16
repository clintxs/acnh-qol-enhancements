"""Exercise production installers' bounds, diagnostics and baseline hook sites.

Set QOL_BASELINE_ELF to a preserved ELF to compare installed main hook sites.
Native services are modeled; this does not qualify live game/render behavior.
"""
import os
import contextlib
import io
import runpy
from pathlib import Path
from unittest.mock import patch
from core_arm_test import Machine, DEFAULT
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2

BASES = (0x7100000000, 0x7200000000)
for base in BASES:
    for installer in ('InstallQuickStore', 'InstallAutoTraversal', 'InstallRecipePreview', 'InstallWorkbenchPreviewTilt'):
        m = Machine(base, {})
        # The segment is genuinely unreadable, not merely reported as short.
        # Installers must reject it without probing any main instructions.
        m.write(m.module.find('s_ModuleInfos') + 72 + 24, 0)
        m.u.mem_unmap(base, 0x6000000)
        for reg in (UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2):
            m.u.reg_write(reg, 1)
        m.call(m.module.find(installer))
        assert m.reg(0) == 0 and not m.hooks, (installer, m.text)
        assert 'outside executable text' in m.text and 'actual=unreadable' in m.text, m.text
    # A changed instruction is readable and must report exact evidence without
    # installing any of the affected quick-store hooks.
    for dependency, name, installer in (
        (0x2572e00, 'inventory.Capacity', 'InstallQuickStore'),
        (0x2577ea0, 'inventory.ItemAt', 'InstallQuickStore'),
        (0x2e99d90, 'crafting.preview.RecipeLeave', 'InstallRecipePreview'),
    ):
        m = Machine(base, {})
        m.write(base + dependency, 0xdeadbeef, 4)
        m.call(m.module.find(installer))
        assert m.reg(0) == 0 and not m.hooks
        assert f'native binding={name}' in m.text and 'actual=deadbeef' in m.text
    # A truncated pointer slot must decline DIY publication before changing its
    # renderer vtable or hooks. This is distinct from instruction mismatch.
    m = Machine(base, {})
    m.write(m.module.find('s_ModuleInfos') + 72 + 56, 0)
    m.call(m.module.find('InstallRecipePreview'))
    assert m.reg(0) == 0 and not m.hooks and 'outside mapped data' in m.text
    assert m.read(base + 0x49e8378) == base + 0x2ea0754
print('PASS: production installers reject unmapped/truncated text, report exact mismatches, and guard data slots at two bases')

baseline = os.environ.get('QOL_BASELINE_ELF')
if baseline:
    preserved = Path(baseline).resolve().read_bytes()
    original_read = Path.read_bytes

    def baseline_read(path):
        if path.name == 'project.elf' and path.parent == Path(__file__).resolve().parents[1]:
            return preserved
        return original_read(path)

    def installed(base, files):
        m = Machine(base, files)
        m.boot()
        m.activate()
        assert 'BOOT_OK' in m.text and 'errors=0' in m.text, m.text
        return {address - base for address in m.hooks}, m.text

    for base in BASES:
        for slugs in (('pockets',), ('controls',), ('crafting',), ('pockets', 'controls', 'crafting'), ('catalog',), ('pockets', 'controls', 'crafting', 'catalog'), ('shopping-card',), ('fitting-room', 'shopping-card'), ('fitting-room',), ('pockets', 'controls', 'crafting', 'catalog', 'fitting-room'), ('pockets', 'controls', 'crafting', 'catalog', 'fitting-room', 'shopping-card')):
            files = dict.fromkeys(slugs, DEFAULT)
            current_sites, current_log = installed(base, files)
            with patch.object(Path, 'read_bytes', baseline_read):
                baseline_sites, baseline_log = installed(base, files)
            assert current_sites == baseline_sites, (slugs, current_sites ^ baseline_sites)
            for marker in ('QUICK_STORE_READY', 'DIY preview: READY', 'auto vault: READY', 'auto ladder: READY', 'auto equip: READY', 'CATALOG_STARS_READY', 'CATALOG_BADGES_READY', 'CATALOG_VARIATIONS_READY', 'DIALOGUE_ITEM_STARS_READY'):
                assert (marker in current_log) == (marker in baseline_log), (slugs, marker)
    print('PASS: migrated feature and combined hook sites/readiness match the preserved stage 0 ELF at two bases')
    # Run the same behavioral assertions against both ELFs, including full item
    # identity, eligibility, deferred dispatch, cancellation and feedback timing.
    def pocket_callbacks():
        with contextlib.redirect_stdout(io.StringIO()) as output:
            result = runpy.run_path(str(Path(__file__).with_name('arm_stack_test.py')))
        return result['cases'], output.getvalue()

    current_callbacks = pocket_callbacks()
    with patch.object(Path, 'read_bytes', baseline_read):
        baseline_callbacks = pocket_callbacks()
    assert current_callbacks == baseline_callbacks
    print(f'PASS: identical {current_callbacks[0]} pocket callback scenarios on current and preserved baseline ELFs')
else:
    print('SKIP: baseline ELF comparison (set QOL_BASELINE_ELF to enable)')
