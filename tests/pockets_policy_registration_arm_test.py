"""Production Pockets callback registration and behavior at two load addresses.

Only hook allocation and engine services are modeled. No private callback slot
is assigned here: each new handler must be installed by its feature entry point.
Inherited suites separately execute original Split/transfer/capacity paths.
"""
import runpy
from pathlib import Path

for base in (0x80000000, 0x1234500000):
    result = runpy.run_path(str(Path(__file__).with_name('arm_stack_test.py')),
                           init_globals={'POCKET_TEST_BASES': (base,)})
    scope = result['reset_pocket'].__globals__
    template = scope['h']
    h = scope['Machine'](base, register_fixture=False)
    h.u.mem_write(base, bytes(template.u.mem_read(base, len(scope['raw']))))
    h.u.mem_map(base + 0x59f8000, 0x1000)
    h.u.mem_write(base + 0x59f8000, bytes(template.u.mem_read(base + 0x59f8000, 0x1000)))
    h.u.mem_write(h.ram, bytes(template.u.mem_read(template.ram, 0x100000)))
    h.stubs = dict(template.stubs)
    scope['h'] = h
    symbol = scope['symbol']
    h.write(h.mod + symbol('s_ModuleInfos') + 72 + 16, base)
    h.write(h.mod + symbol('s_ModuleInfos') + 72 + 24, 0x3f07000)
    # Remove fixture-generated inline adapters at native fingerprint sites.
    for offset in (0x3076480, 0x30764f0, 0x308c764, 0x1a9bfa0):
        h.u.mem_write(base + offset, scope['raw'][offset:offset + 20])
    installed = []
    def install_hook():
        target = h.reg(0)
        installed.append(target)
        h.u.mem_write(target, b'\xff' * 20)
        return target
    h.stubs[h.mod + symbol('nx64', '4HookEmmb')] = install_hook
    h.stubs[h.mod + symbol('nx64', 'HookInline')] = install_hook
    def function(name):
        found = [value for key, value in scope['symbols'].items()
                 if f'{len(name)}{name}Ev' in key and scope['symbol_types'][key] == 2]
        assert len(found) == 1, (name, found)
        return h.mod + found[0]
    for name in ('InstallStackSplitting', 'InstallStorageWithdrawal', 'InstallRecyclingClaim',
                 'InstallQuickStore', 'InstallHoldSelect', 'InstallStorageHint', 'InstallStorageWithdrawalHint'):
        try:
            h.run(function(name))
        except Exception as error:
            raise AssertionError(f"{name} failed at {base:#x}") from error
        assert h.reg(0) == 1, name
    # Restore original instructions: callbacks use the fixture's direct Orig
    # pointers. Hook publication and declaration copying have already run.
    for target in installed:
        offset = target - base
        h.u.mem_write(target, scope['raw'][offset:offset + 20])
    menu, context = h.ram + 0x100, h.ram + 0x200
    built = []
    def stack_menu():
        built.append(1)
        h.write(menu, 46, 4); h.write(menu + 28, 1, 4)
    h.stubs[base + 0x307e7e0] = stack_menu
    h.stubs[base + 0x19f02d0] = lambda: 30
    h.stubs[base + 0x2572fb0] = lambda: 1
    h.put(0, menu); h.put(1, context); h.run(h.callback('BuildStackMenu'))
    assert built == [1] and [h.read(menu + i * 4, 4) for i in range(3)] == [46, scope['GRAB_FIVE'], scope['GRAB_TEN']]
    # Real feature-installed storage policy: ENTRY defers, UPDATE confirms
    # exactly once; a new extra option between those phases restores the menu.
    owner, child = h.ram + 0x1000, h.ram + 0xb1000
    h.write(owner, base + 0x4a16028); h.write(owner + 0x220, 0, 4)
    h.write(owner + 0x1b0, 5, 4); h.write(owner + 0x200, 0, 1)
    h.write(owner + 0x1f0, 0xffffffffffffffff)
    h.write(owner + 0xaa9e9, 0, 1); h.write(owner + 0xaa9d0, child)
    h.write(child + 0x1b0, 0, 4); h.write(child + 0x1f0, 0xffffffffffffffff)
    h.write(child + 0x218, base + 0x4a164e8); h.write(child + 0x220, owner)
    h.write(child + 0x228, base + 0x316b2dc); h.write(child + 0x230, 0)
    bank, key = h.ram + 0x94000, h.ram + 0x94020
    h.u.mem_write(bank, b'CmnSubWin\0'); h.u.mem_write(key, b'0202\0')
    count, events = 1, []
    def storage_menu():
        description = h.reg(1)
        h.write(description + 0x44, count, 4)
        h.write(description + 0x48, bank); h.write(description + 0x50, key)
        h.write(description + 0x58, 0, 4)
    def update():
        if h.read(child + 0x26c9, 1) == 0:
            assert h.read(child + 0x1b0, 4) == 1 and h.read(child + 0x26d8, 4) == 1
            assert h.read(child + 0x3a0, 4) == 0
            events.append('transfer')
    h.stubs[base + 0x316b2dc] = storage_menu
    h.stubs[base + 0x316dd58] = lambda: events.append('menu')
    h.stubs[base + 0x316deec] = update
    h.put(0, owner); h.run(h.callback('EnterStorageCommand')); assert not events
    h.put(0, owner); h.run(h.callback('UpdateStorageCommand'))
    h.put(0, owner); h.run(h.callback('UpdateStorageCommand'))
    assert events == ['transfer'] and h.read(child + 0x26c9, 1) == 255 and h.read(child + 0x1b0, 4) == 0
    events.clear()
    h.put(0, owner); h.run(h.callback('EnterStorageCommand')); assert not events
    count = 2
    h.put(0, owner); h.run(h.callback('UpdateStorageCommand'))
    assert events == ['menu']
    # Reject fabricated/out-of-range results even from a registered chooser.
    # Exercise the real adapter against a callback-scoped menu snapshot.
    h.u.mem_write(h.ram, bytes(template.u.mem_read(template.ram, 0x100000)))
    h.stubs = dict(template.stubs)
    pocket = scope['pocket']
    output, chooser = h.ram + 0x96000, h.ram + 0x97000
    row_action, flags, choice = 30, 0, 0
    def probe_menu():
        description = h.reg(1)
        h.write(description + 0x44, 1, 4)
        h.write(description + 0x58, row_action, 4)
        h.write(description + 0x5c, flags, 4)
    h.stubs[base + 0x3088df0] = probe_menu
    h.stubs[chooser] = lambda: choice
    for row_action, flags, choice, expected in ((30, 0, 0, True), (30, 0, 7, False),
                                               (30, 1, 0, False), (999, 0, 0, False)):
        scope['reset_pocket']()
        h.put(0, pocket); h.put(1, scope['selected_slot']); h.put(2, 0); h.put(3, 1)
        h.put(4, chooser); h.put(5, 0); h.put(6, 0); h.put(8, output)
        h.run(h.mod + symbol('pocket_commands_native', '5Probe'))
        assert bool(h.read(output + 16, 1)) == expected, (row_action, flags, choice)

    # Command policy declarations are immutable after publication.
    rules, policies = h.ram + 0x95000, h.ram + 0x95100
    h.write(rules, 0x80, 4); h.write(rules + 4, 0, 4)
    for i, name in enumerate(('StoreAtHome', 'DropOrRelease', 'EquipOrConsume', 'OpenContainerDirectly')):
        h.write(policies + i * 16, h.mod + symbol(name))
        h.write(policies + i * 16 + 8, (2, 1, 1, 0)[i], 4)
        h.write(policies + i * 16 + 12, int(i != 3), 1)
    h.write(policies + 8, 1, 4)  # Alter Store feedback.
    before = len(installed)
    h.put(0, rules); h.put(1, 1); h.put(2, policies)
    h.run(h.mod + symbol('InstallCommandHandlers'))
    assert h.reg(0) == 0 and len(installed) == before
    print(f'PASS {base:#x}: production Pockets handler installation, stack insertion, deferred storage once-only execution/cancellation and incompatible policy rejection')
