"""Valid alternate registration through production, with a fresh module image.

Reuse the native-world fixture only. No module/service state is copied from
it. Original command entry/update and the transition consumer still execute;
engine action completion and hook allocation remain modeled.
"""
import runpy
from pathlib import Path

for base in (0x80000000, 0x1234500000):
    result = runpy.run_path(str(Path(__file__).with_name('arm_stack_test.py')),
                           init_globals={'POCKET_TEST_BASES': (base,)})
    scope = result['reset_pocket'].__globals__
    template = scope['h']
    h = scope['Machine'](base, register_fixture=False)
    # Reuse initialized game UI objects and native table fixups. The freshly
    # loaded module keeps empty registries, empty declarations and no requests.
    h.u.mem_write(base, bytes(template.u.mem_read(base, len(scope['raw']))))
    h.u.mem_map(base + 0x59f8000, 0x1000)
    h.u.mem_write(base + 0x59f8000, bytes(template.u.mem_read(base + 0x59f8000, 0x1000)))
    h.u.mem_write(h.ram, bytes(template.u.mem_read(template.ram, 0x100000)))
    h.stubs = dict(template.stubs)
    scope['h'] = h
    symbol = scope['symbol']
    h.write(h.mod + symbol('s_ModuleInfos') + 72 + 16, base)
    h.write(h.mod + symbol('s_ModuleInfos') + 72 + 24, 0x3f07000)
    installed = []
    def install_hook():
        target = h.reg(0)
        installed.append(target)
        h.u.mem_write(target, b'\xff' * 20)
        return target
    h.stubs[h.mod + symbol('nx64', '4HookEmmb')] = install_hook
    local_rules = h.ram + 0x94000
    def register(button=0x80, action=1):
        h.write(local_rules, button, 4)
        h.write(local_rules + 4, action, 4)
        h.put(0, local_rules); h.put(1, 1)
        h.run(h.mod + symbol('InstallShortcuts'))
        return h.reg(0)
    assert register() == 1
    assert set(installed) == {base + offset for offset in (0x3c58d24, 0x308c5f8, 0x308aa80, 0x308c4c4, 0x308c590)}
    assert h.read(h.mod + symbol('InputHandlers')) != 0  # Production typed input adapter.
    assert register() == 1 and len(installed) == 5
    assert register(action=0) == 0 and len(installed) == 5
    # Identical registration must still detect foreign hook changes.
    h.write(base + 0x308c5f8, 0xdeadbeef, 4)
    assert register() == 0 and len(installed) == 5
    h.u.mem_write(base + 0x308c5f8, b'\xff' * 20)
    assert register() == 1 and len(installed) == 5
    # Caller storage disappears/changes. The service must still own Y -> Drop.
    h.u.mem_write(local_rules, b'\xcc' * 8)
    # The callback harness executes displaced original instructions directly.
    for target in installed:
        offset = target - base
        h.u.mem_write(target, scope['raw'][offset:offset + 20])
    child, pocket = scope['child'], scope['pocket']
    observed, feedback = [], []
    def menu():
        description = h.reg(1)
        h.write(description + 0x44, 1, 4)
        h.write(description + 0x58, 30, 4)
    def drop():
        observed.append(h.read(child + 0x3a0, 4))
        return int(len(observed) == 3)
    index = scope['action_map'][30]
    h.stubs[base + 0x3088df0] = menu
    h.stubs[base + 0x307d490] = lambda: index
    target = base + scope['u64'](scope['raw'], 0x4a06740 + index * 16)
    h.write(base + 0x4a06740 + index * 16, target)
    h.stubs[target] = drop
    h.stubs[base + 0x3c66950] = lambda: feedback.append(h.reg(0))
    scope['reset_pocket']()
    scope['y_input'](); scope['y_input'](0x80)
    assert h.read(pocket + 0x1f0, 4) == 7 and not observed and not feedback
    scope['transition']()
    assert not observed and not feedback
    for count in range(1, 4):
        scope['update']()
        assert observed == [30] * count and len(feedback) == 1
        assert h.read(child + 0x3a0, 4) == 99 and h.read(child + 0x26c9, 1) == 255
        assert h.read(pocket + 0x1f4, 4) == (14 if count == 3 else 0xffffffff)
    scope['update']()
    assert observed == [30] * 3 and len(feedback) == 1
    print(f'PASS {base:#x}: production alternate registration, owned caller data, identical reuse, conflict/foreign rejection and deferred multi-frame Drop with single feedback')
