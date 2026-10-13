"""Stage 2 ownership, preparation and partial activation through production code."""
from core_arm_test import Machine, RAW, DEFAULT
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1

def call(m, name, *args):
    for index, value in enumerate(args):
        m.u.reg_write(UC_ARM64_REG_X0 + index, value)
    if name == 'RegisterPageInput':
        m.u.reg_write(UC_ARM64_REG_X1, 60)
        m.u.reg_write(UC_ARM64_REG_X0 + 2, 0)
        name = 'RegisterInputAdapter'
    elif name == 'RegisterPageInputObserver':
        m.u.reg_write(UC_ARM64_REG_X0 + 2, 60)
        m.u.reg_write(UC_ARM64_REG_X0 + 3, 0)
        name = 'RegisterObserverAdapter'
    m.call(m.module.find(name))
    return int(m.reg(0) in (0, 1)) if name in ('RegisterInputAdapter', 'RegisterObserverAdapter') else m.reg(0)

def outcome(m, name):
    # Read the bounded production result table: an AArch64 entry has an id
    # pointer followed by FeatureResult (state, published, reason pointer).
    table = m.module.find('ActivationResults')
    for index in range(64):
        entry = table + index * 24
        identifier = m.read(entry)
        if identifier and m.string(identifier) == name:
            return m.read(entry + 8, 4), bool(m.read(entry + 12, 1)), m.string(m.read(entry + 16))
    raise AssertionError(('missing result', name, m.text))

for base in (0x7100000000, 0x7200000000):
    # Shared input: duplicate registration verifies existing code, capacity does
    # not publish a new handler, and foreign removal cannot masquerade as pristine.
    m = Machine(base, {})
    for i in range(5): assert call(m, 'RegisterPageInput', m.ram + 0x1000 + i * 4) == 1
    assert len(m.hooks) == 1
    assert call(m, 'RegisterPageInput', m.ram + 0x1000) == 1
    before = call(m, 'MutationCount')
    assert call(m, 'RegisterPageInput', m.ram + 0x2000) == 0
    assert call(m, 'MutationCount') == before
    m.u.mem_write(base + 0x3c58d24, RAW[0x3c58d24:0x3c58d24 + 20])
    assert call(m, 'RegisterPageInput', m.ram + 0x1000) == 0
    assert 'owned/reserved instructions changed' in m.text

    # Observer pairs are idempotent, but reusing only one half is incompatible.
    m = Machine(base, {})
    before_cb, after_cb = m.ram + 0x1000, m.ram + 0x1100
    assert call(m, 'RegisterPageInputObserver', before_cb, after_cb) == 1
    assert call(m, 'RegisterPageInputObserver', before_cb, after_cb) == 1
    assert call(m, 'RegisterPageInputObserver', before_cb, after_cb + 4) == 0
    for i in range(1, 5):
        assert call(m, 'RegisterPageInputObserver', before_cb + i * 4, after_cb + i * 4) == 1
    assert call(m, 'RegisterPageInputObserver', before_cb + 20, after_cb + 20) == 0

    # Caption capacity and reuse also validate the shared native owner.
    m = Machine(base, {})
    for i in range(4): assert call(m, 'RegisterHintText', m.ram + 0x1000 + i * 4) == 1
    assert call(m, 'RegisterHintText', m.ram + 0x1000) == 1
    assert call(m, 'RegisterHintText', m.ram + 0x2000) == 0
    m.write(base + 0x1aea210, 0xdeadbeef, 4)
    assert call(m, 'RegisterHintText', m.ram + 0x1000) == 0

    for order in (('InstallStorageHint', 'InstallFittingHint'), ('InstallFittingHint', 'InstallStorageHint')):
        m = Machine(base, {})
        for name in order: assert call(m, name) == 1, (order, name, m.text)
        assert base + 0x1aea210 in m.hooks and len(m.hooks) == 4

    # Both resource registration orders retain one hook and separate allowances.
    for order in (('PrepareFittingResources', 'PrepareShoppingResources'),
                  ('PrepareShoppingResources', 'PrepareFittingResources')):
        m = Machine(base, {})
        for name in order: assert call(m, name) == 1
        assert len(m.hooks) == 1
        assert call(m, order[0]) == 1
        m.write(base + 0x3e3cad0, 0xdeadbeef, 4)
        assert call(m, order[1]) == 0

    # Actual shared consumers in both orders, including workbench before DIY.
    for order in (('InstallHoldToAdvanceDialogue', '_ZN3qol8dialogue7InstallEv'),
                  ('_ZN3qol8dialogue7InstallEv', 'InstallHoldToAdvanceDialogue'),
                  ('InstallWorkbenchPreviewTilt', 'InstallRecipePreview'),
                  ('InstallRecipePreview', 'InstallWorkbenchPreviewTilt'),
                  ('InstallQuickStore', 'InstallRecipePreview'),
                  ('InstallRecipePreview', 'InstallQuickStore')):
        m = Machine(base, {})
        for name in order: assert call(m, name) == 1, (order, name, m.text)
        if 'InstallHoldToAdvanceDialogue' in order:
            assert call(m, 'InstallRevealHooks') == 1
            m.write(base + 0x1afa00c, 0xdeadbeef, 4)
            assert call(m, 'InstallRevealHooks') == 0
        if 'InstallWorkbenchPreviewTilt' in order:
            m.u.mem_write(base + 0x6021a4, RAW[0x6021a4:0x6021a4 + 20])
            assert call(m, 'InstallWorkbenchPreviewTilt') == 0

    # A missing shared service leaves private hooks unpublished, records actual
    # dependencies, and preserves independently supported Pockets behavior.
    m = Machine(base, {'pockets': DEFAULT})
    handlers = m.module.find('InputHandlers')
    for i in range(5): m.write(handlers + i * 8, m.ram + 0x1000 + i * 4)
    m.boot(); m.activate()
    assert outcome(m, 'pockets.quick_store')[:2] == (3, False)
    assert outcome(m, 'pockets.hints')[:2] == (3, False)
    assert outcome(m, 'pockets.recycling')[:2] == (1, True)
    assert outcome(m, 'pockets')[:2] == (4, True)
    assert base + 0x308aa80 not in m.hooks and base + 0x308c5f8 not in m.hooks

    m = Machine(base, {'fitting-room': DEFAULT})
    providers = m.module.find('HintProviders')
    for i in range(4): m.write(providers + i * 8, m.ram + 0x1000 + i * 4)
    m.boot(); m.activate()
    assert outcome(m, 'fitting.bag')[:2] == (3, False)
    assert outcome(m, 'resources.fitting')[:2] == (1, True)
    assert 'Fitting Room: ERROR installation failed or partial; published=true' in m.text
    assert m.read(base + 0x4a10398) == base + 0x3c58d24
    assert m.read(base + 0x49cd3f8) == base + 0x3c58d24

    # Unsupported traversal extensions remain explicit while the pole and
    # handheld ladder paths, plus unrelated controls, stay active.
    m = Machine(base, {'controls': DEFAULT})
    m.write(base + 0x22d9bd8, 0xdeadbeef, 4)
    m.boot(); m.activate()
    assert outcome(m, 'controls.placed_ladder')[:2] == (2, False)
    assert outcome(m, 'controls.auto_vault')[:2] == (1, True)
    assert outcome(m, 'controls.auto_ladder')[:2] == (1, True)
    assert outcome(m, 'controls.traversal')[:2] == (2, True)
    assert outcome(m, 'controls.swimming')[:2] == (1, True)
    assert 'BOOT_PARTIAL' in m.text

    m = Machine(base, {'crafting': DEFAULT + b'crafting_speed=0\ndiy_app_preview=false\nworkbench_preview_tilt=false\ncrafting_quantity_wrap=false'})
    m.boot(); m.activate()
    for feature in ('crafting.speed', 'crafting.diy_preview', 'crafting.workbench_tilt', 'crafting.quantity_wrap'):
        assert outcome(m, feature)[:2] == (0, False)
    assert outcome(m, 'crafting.quantity_shortcut')[:2] == (1, True)

    # Inject failure at the real pointer publication/readback boundary. The
    # already-published input service is reported; other Crafting features run.
    m = Machine(base, {'crafting': DEFAULT})
    original = base + 0x2ea0754
    injected = [False]
    def reject_mode_write(u, pc, size, _):
        if not injected[0] and m.read(base + 0x49e8378) != original:
            m.write(base + 0x49e8378, original)
            injected[0] = True
    m.u.hook_add(UC_HOOK_CODE, reject_mode_write)
    m.boot(); m.activate()
    assert injected[0]
    assert outcome(m, 'crafting.diy_preview')[:2] == (4, True)
    assert outcome(m, 'crafting.quantity_shortcut')[:2] == (1, True)
    assert 'renderer mode slot write failed' in outcome(m, 'crafting.diy_preview')[2]

    # RomFS presence and enabled behavior have separate, explicit results.
    m = Machine(base, {'fitting-room': b'enabled=false\nversion=0.4.32'})
    m.boot(); m.activate()
    assert outcome(m, 'resources.fitting')[:2] == (1, True)
    assert outcome(m, 'fitting-room')[:2] == (0, True)
    assert base + 0x3e3cad0 in m.hooks
    print(f'PASS {base:#x}: preparation, service capacity, repeated ownership checks, foreign changes, both consumer orders, dependencies, partial publication and disabled-package resources')
