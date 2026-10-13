"""Replay hold-A gestures through compiled shared input, then native selection.

Navigation/button generation are modeled. Original 3.0.3 selection code runs
through its mask, limit, capacity and refresh handling; drawing is stubbed.
The imported harness also runs existing Pockets regressions.
"""
from arm_stack_test import Machine, symbol
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_PC

cases = 0
for base in (0x80000000, 0x1234500000):
    h = Machine(base)
    pocket, page, snapshot, rows = [h.ram + o for o in (0x1000, 0x20000, 0x21000, 0x22000)]
    ctx, focus, controllers, controller = [h.ram + o for o in (0x23000, 0x24000, 0x25000, 0x26000)]
    binding, hint, unit, app, masks = [h.ram + o for o in (0x27000, 0x28000, 0x29000, 0x2a000, 0x2b000)]
    widgets, panes, marker, marker_vt, impl = [h.ram + o for o in (0x30000, 0x40000, 0x41000, 0x42000, 0x43000)]
    marker_ptr = h.ram + 0x44000
    prepare, animate, suppress = [h.ram + o for o in (0x85000, 0x85010, 0x85020)]
    h.u.mem_write(h.mod + symbol('InputHandlers'), bytes(5 * 8))
    h.stubs[h.mod + symbol('SvcLogger', 'LogRaw')] = lambda: None
    # Supply the real text segment metadata required by bounded preparation;
    # the inherited callback-only fixture previously needed just the module base.
    main_info = h.mod + symbol('s_ModuleInfos') + 72
    h.write(main_info + 16, base)
    h.write(main_info + 24, 0x3f07000)
    # Model Exlaunch publication and register through the real ownership path.
    # Native input/feedback calls are still intercepted by the service stubs below.
    def install_hook():
        target = h.reg(0)
        h.u.mem_write(target, b'\xff' * 20)
        return target
    h.stubs[h.mod + symbol('nx64', '4HookEmmb')] = install_hook
    h.run(h.mod + symbol('InstallHoldSelect'))
    assert h.reg(0) == 1
    h.write(pocket, base + 0x4a06e28)
    h.write(pocket + 0x210, page)
    h.write(page, base + 0x4a06b00)
    h.write(page + 0x468, 1, 4); h.write(page + 0x46c, 1, 4)
    h.write(page + 0x470, binding)
    for offset, value in ((0, base + 0x4a070d8), (8, pocket), (16, base + 0x3088724), (24, 0)):
        h.write(binding + offset, value)
    h.write(page + 0x570, hint); h.write(hint + 0xa0, unit)
    h.write(unit + 0x14, 5, 4); h.write(unit + 0x10, 1, 4)
    h.write(page + 0x210, ctx); h.write(ctx + 0x48, focus)
    h.write(focus + 0x18, 2, 4); h.write(focus + 0x20, controllers)
    h.write(controllers + 8, controller)
    h.write(snapshot + 8, 2, 4); h.write(snapshot + 0x10, rows)
    h.u.mem_map(base + 0x59f8000, 0x1000)  # Application pointer is native BSS.
    h.write(base + 0x59f83f0, app); h.write(app + 0x418, masks)
    h.write(marker_ptr, marker); h.write(marker, marker_vt)
    h.write(marker_vt + 0x18, prepare); h.write(marker + 0x18, impl)
    h.write(impl + 0x158, animate)
    for slot in range(40):
        widget = widgets + slot * 0x400
        h.write(page + 0x590 + slot * 8, widget)
        h.write(widget + 0x218, panes + slot * 16)
        h.write(widget + 0x2c3, 1, 1)
        h.write(widget + 0x315, 1, 1)
        parameter = h.ram + 0x88000 + slot * 0x100
        h.write(widget + 0x2a0, parameter)
        h.write(widget + 0x2a8, parameter + 0x50)
        h.write(widget + 0x2f0, marker_ptr)
    target, native_click, native_calls, free_storage = 0, None, [], 40
    feedback_events = []
    sound_string, sound_text = h.ram + 0x87000, h.ram + 0x87100
    h.write(sound_string, base + 0x47f9770)
    h.write(sound_string + 8, sound_text)
    h.u.mem_write(sound_text, b'Select\0')
    feedback_pending = False
    def native_input():
        global feedback_pending
        native_calls.append(1)
        old_pane = h.read(controller + 0x10)
        h.write(controller + 0x10, panes + target * 16 if target >= 0 else 0)
        if native_click is not None:
            h.write(pocket + 0x1c518, native_click, 1)
            h.write(pocket + 0x1c51a, 4, 2)
        # Native pass consumes A after navigation; this must not block the observer.
        for row in range(2): h.write(rows + row * 0x48 + 0x3c, 0xffffffff, 4)
        feedback_pending = target >= 0 and old_pane != panes + target * 16
        if feedback_pending:
            h.put(0, widgets + target * 0x400); h.put(1, sound_string); h.put(2, 123)
    def redirect_feedback(u, pc, size, data):
        # The modeled navigation ends with the actual compiled feedback hook;
        # retain its caller's LR so it returns to the shared after-observer.
        if pc == base + 0x3c58d24 and feedback_pending:
            u.reg_write(UC_ARM64_REG_PC, h.callback('HoldSelectFeedback'))
    h.u.hook_add(UC_HOOK_CODE, redirect_feedback)
    def feedback():
        name = bytes(h.u.mem_read(h.read(h.reg(1)+8), 16)).split(b'\0')[0]
        feedback_events.append((h.reg(0), name, h.reg(2)))
    refreshes = []
    h.stubs = {base + 0x3c58d24: native_input, prepare: lambda: None,
               animate: lambda: None, suppress: lambda: 1,
               h.mod + symbol('SvcLogger', 'LogRaw'): lambda: None,
               base + 0x257c230: lambda: free_storage,
               base + 0x3c66950: feedback,
               base + 0x3093d24: lambda: refreshes.append(h.read(pocket + 0x1c4e8))}
    def frame(buttons=0, move=None, blocked=0, consumed=0, click=None, other_row=0):
        global target, native_click
        if move is not None: target = move
        native_click = click
        h.write(rows, other_row, 4); h.write(rows + 0x3c, 0, 4)
        # Live Eden UI rows include bit 16 alongside A and navigation. This is
        # not a shoulder/trigger action and must not cancel sweep selection.
        live_buttons = buttons | (0x10000 if buttons else 0)
        h.write(rows + 0x48, live_buttons, 4); h.write(rows + 0x48 + 0x3c, consumed, 4)
        h.put(0, page); h.put(1, snapshot); h.put(2, blocked)
        before = len(native_calls)
        h.run(h.callback('SharedPageInput'))
        assert len(native_calls) == before + 1
    def native_select():
        h.put(0, pocket)
        # Stop after native mask/limit/capacity handling and refresh, before counters.
        h.run(base + 0x308b3f0, base + 0x308b780)
        h.put(0, pocket); h.run(base + 0x3090cec)
    def reset(mode=1, limit=-1):
        h.write(pocket + 0x1b0, 3, 4); h.write(pocket + 0x1f0, 0xffffffffffffffff)
        h.write(pocket + 0x1c4fc, mode, 4)
        h.write(pocket + 0x1c4f8, limit & 0xffffffff, 4)
        h.write(pocket + 0x1c4e8, 0); h.write(pocket + 0x1c4f0, 0)
        h.write(pocket + 0x1c51a, 0, 2); h.write(pocket + 0x1c518, 255, 1)
        h.write(ctx + 0x54, 0, 1); h.write(ctx + 0x58, 1, 4); h.write(masks + 0x10, 0, 4)
        frame(0, 0)
    for mode in (1, 9):
        reset(mode)
        # First item uses the normal A click, then four cursor moves add four more.
        frame(16, click=0); native_select()
        for slot in range(1, 5):
            feedback_events.clear()
            frame(16, slot)
            assert feedback_events == [(widgets + slot * 0x400, b'Decide', 0)]
            assert h.read(h.ram + 0x88000 + slot * 0x100 + 0x40, 4) == 1
            assert h.read(h.ram + 0x88000 + slot * 0x100 + 0x90, 4) == 1
            assert h.read(pocket + 0x1c518, 1) == slot
            assert h.read(pocket + 0x1c51a, 2) == 4
            native_select()
        assert h.read(pocket + 0x1c4e8) == 31
        feedback_events.clear()
        frame(16, 3); assert h.read(pocket + 0x1c51a, 2) == 0  # Backtracking.
        assert feedback_events == [(widgets + 3 * 0x400, b'Select', 123)]
        feedback_events.clear()
        frame(16); assert h.read(pocket + 0x1c51a, 2) == 0  # Stationary hold.
        assert feedback_events == []
        frame(0); frame(16, click=3); native_select()  # Separate tap still deselects.
        assert h.read(pocket + 0x1c4e8) == 23
        frame(0, 39); frame(16, click=39); native_select()
        assert h.read(pocket + 0x1c4e8) == 23 | (1 << 39)
        cases += 1
        # Starting on selected items locks this hold to deselection.
        reset(mode); h.write(pocket + 0x1c4e8, 31)
        frame(16, click=0); native_select()
        for slot in range(1, 5):
            feedback_events.clear()
            frame(16 | 8, slot); native_select()
            assert feedback_events == [(widgets + slot * 0x400, b'Decide', 0)]
        assert h.read(pocket + 0x1c4e8) == 0
        frame(16, 3); assert h.read(pocket + 0x1c51a, 2) == 0
        frame(16); assert h.read(pocket + 0x1c51a, 2) == 0
        frame(0); frame(16, click=3); native_select()
        frame(16, 4); native_select()
        assert h.read(pocket + 0x1c4e8) == 24  # New hold selects again.
        cases += 1
        # Unselected gaps do not switch a deselecting hold back to selecting.
        reset(mode); h.write(pocket + 0x1c4e8, 21)
        frame(16, click=0); native_select()
        frame(16, 1); assert h.read(pocket + 0x1c51a, 2) == 0
        frame(16, 2); native_select()
        frame(16, 3); assert h.read(pocket + 0x1c51a, 2) == 0
        frame(16, 4); native_select()
        assert h.read(pocket + 0x1c4e8) == 0
        cases += 1
        # A+movement in one frame follows the native first-click destination.
        reset(mode); h.write(pocket + 0x1c4e8, 6)
        frame(16 | 8, 1, click=1); native_select()
        frame(16 | 8, 2); native_select()
        assert h.read(pocket + 0x1c4e8) == 0
        cases += 1
        # A blocked deselecting gesture requires release before it can restart.
        reset(mode); h.write(pocket + 0x1c4e8, 7)
        frame(16, click=0); native_select()
        frame(16, 1, blocked=1)
        frame(16, 2); assert h.read(pocket + 0x1c51a, 2) == 0
        frame(0); frame(16, click=2); native_select()
        frame(16, 1); native_select()
        assert h.read(pocket + 0x1c4e8) == 0
        cases += 1
    # Navigation plus a real A click retains Select -> Decide ordering, rather
    # than replaying the deferred cursor cue after the real confirm.
    reset(); feedback_events.clear()
    h.write(rows + 0x48, 16, 4); h.write(rows + 0x48 + 0x3c, 0, 4)
    h.put(0, page); h.put(1, snapshot); h.put(2, 0)
    h.run(h.mod + symbol('BeforeHoldSelectInput'))
    for event in (b'Select', b'Decide'):
        h.u.mem_write(sound_text, event + b'\0')
        h.put(0, widgets + 0x400); h.put(1, sound_string); h.put(2, 123)
        h.run(h.callback('HoldSelectFeedback'))
        if event == b'Select': assert feedback_events == []
    assert [event[1] for event in feedback_events] == [b'Select', b'Decide']
    h.write(controller + 0x10, panes + 16)
    h.write(pocket + 0x1c518, 1, 1); h.write(pocket + 0x1c51a, 4, 2)
    h.put(0, page); h.put(1, snapshot); h.put(2, 0)
    h.run(h.mod + symbol('AfterHoldSelectInput'))
    assert len(feedback_events) == 2
    cases += 1
    # Outside an eligible held-input pass, every event passes through unchanged.
    for event in (b'Select', b'Decide', b'Unselect'):
        feedback_events.clear(); h.u.mem_write(sound_text, event + b'\0')
        h.put(0, widgets); h.put(1, sound_string); h.put(2, 123)
        h.run(h.callback('HoldSelectFeedback'))
        assert feedback_events == [(widgets, event, 123)]
        cases += 1
    h.u.mem_write(sound_text, b'Select\0')
    for setting in ('parent', 'local', 'global', 'consumed', 'B', 'selected', 'blocked-slot', 'disabled',
                    'pending-click', 'outside-grid', 'wrong-row', 'transition', 'state', 'single-pick', 'wrong-mode'):
        reset(); frame(16)
        if setting == 'local': h.write(ctx + 0x54, 1, 1)
        if setting == 'global': h.write(masks + 0x10, 1, 4)
        if setting == 'selected': h.write(pocket + 0x1c4e8, 2)
        if setting == 'blocked-slot': h.write(pocket + 0x1c4f0, 2)
        if setting == 'disabled': h.write(widgets + 0x400 + 0x2c3, 0, 1)
        if setting == 'transition': h.write(pocket + 0x1f4, 4, 4)
        if setting == 'state': h.write(pocket + 0x1b0, 8, 4)
        if setting == 'single-pick': h.write(pocket + 0x1c4f8, 1, 4)
        if setting == 'wrong-mode': h.write(pocket + 0x1c4fc, 0, 4)
        frame(0 if setting == 'wrong-row' else 16 | (32 if setting == 'B' else 0),
              -1 if setting == 'outside-grid' else 1,
              blocked=int(setting == 'parent'), consumed=16 if setting == 'consumed' else 0,
              click=7 if setting == 'pending-click' else None, other_row=16)
        assert h.read(pocket + 0x1c51a, 2) == (4 if setting == 'pending-click' else 0), setting
        if setting == 'pending-click': assert h.read(pocket + 0x1c518, 1) == 7
        h.write(widgets + 0x400 + 0x2c3, 1, 1)
        cases += 1
    # Held input cannot resume after a block, nor carry into another mode/page.
    for change in ('block', 'mode', 'page'):
        reset(); frame(16)
        if change == 'block': frame(16, 1, blocked=1)
        if change == 'mode': h.write(pocket + 0x1c4fc, 9, 4)
        if change == 'page':
            h.put(0, pocket); h.run(h.mod + symbol('ResetHoldSelect'))
        frame(16, 2)
        assert h.read(pocket + 0x1c51a, 2) == 0
        frame(0); frame(16); frame(16, 3)
        assert h.read(pocket + 0x1c51a, 2) == 4
        cases += 1
    reset(); frame(16)
    h.write(h.mod + symbol('InputHandlers'), suppress)
    h.put(0, page); h.put(1, snapshot); h.put(2, 0)
    before = len(native_calls); h.run(h.callback('SharedPageInput'))
    assert len(native_calls) == before and h.read(pocket + 0x1c51a, 2) == 0
    h.write(h.mod + symbol('InputHandlers'), 0)
    frame(16, 2); assert h.read(pocket + 0x1c51a, 2) == 0
    cases += 1
    # Unrelated page dispatches in the same frame do not cancel the gesture.
    reset(); frame(16)
    h.write(page, 0); frame(16); h.write(page, base + 0x4a06b00)
    frame(16, 1); assert h.read(pocket + 0x1c51a, 2) == 4
    cases += 1
    # Native enforcement still applies to synthetic clicks.
    reset(limit=2); h.write(pocket + 0x1c4e8, 3)
    frame(0, 3); frame(16); frame(16, 2); native_select()
    assert h.read(pocket + 0x1c4e8) == 3; cases += 1
    reset(mode=9); free_storage = 1; h.write(pocket + 0x1c4e8, 1)
    frame(0, 2); frame(16); frame(16, 1); native_select()
    assert h.read(pocket + 0x1c4e8) == 1
    assert h.read(pocket + 0x1c50c, 4) == 0x69 and h.read(pocket + 0x1f0, 4) == 12
    cases += 1
    # Reproduce the observed shed trace: A=10010, A+right=10018, slots 10..14.
    free_storage = 40
    reset(mode=9); frame(0, 10); frame(0x10010, click=10); native_select()
    for slot in range(11, 15):
        frame(0x10018, slot); native_select()
    assert h.read(pocket + 0x1c4e8) == 0x7c00
    frame(0x10014, 13); assert h.read(pocket + 0x1c51a, 2) == 0
    frame(0); frame(0x10010, click=12); native_select()
    assert h.read(pocket + 0x1c4e8) == 0x6c00
    cases += 1
    for direction in range(4):
        reset(); frame(16); frame(16 | (1 << direction), 1)
        assert h.read(pocket + 0x1c51a, 2) == 4
        cases += 1
    # UI actions occupy bits 5..11, including actual ZL/ZR at 10/11.
    for button in range(5, 12):
        reset(); frame(16); frame(16 | (1 << button), 1)
        assert h.read(pocket + 0x1c51a, 2) == 0
        frame(16, 2); assert h.read(pocket + 0x1c51a, 2) == 0
        frame(0); frame(16); frame(16, 3)
        assert h.read(pocket + 0x1c51a, 2) == 4
        cases += 1
print(f'hold select: {cases} compiled/native ARM scenarios passed at two bases')
