"""Compiled hint callbacks plus original footer builder/append at two bases.

Imports the existing ARM harness (and runs its regressions). Engine type checks,
home location, glyph-priority lookup and memcpy are substituted; native record
allocation/copying and footer construction execute directly. Rendering remains
an Eden test, not something this fixture claims to simulate.
"""
from arm_stack_test import Machine, symbol, raw, u64

KEY = b'qol_pockets.put_in_storage'
WITHDRAWAL_KEY = b'qol_pockets.move_to_pockets'
cases = 0
for base in (0x80000000, 0x1234500000):
    h = Machine(base, withdrawal_hint=True)
    pocket, page, back, unit = [h.ram + o for o in (0x1000, 0x20000, 0x21000, 0x22000)]
    app, footer, entries, pool = [h.ram + o for o in (0x24000, 0x25000, 0x26000, 0x27000)]
    type_guard, type_object, footer_vt = [h.ram + o for o in (0x28000, 0x28100, 0x28200)]
    is_type, prepare = h.ram + 0x80010, h.ram + 0x80020
    def cstr(p):
        return bytes(h.u.mem_read(p, 64)).split(b'\0', 1)[0]
    def copy():
        h.u.mem_write(h.reg(0), bytes(h.u.mem_read(h.reg(1), h.reg(2))))
        return h.reg(0)
    at_home, right_type = True, True
    def home():
        assert h.reg(0) == pocket + 0x1f8
        return int(at_home)
    h.stubs = {is_type: lambda: int(right_type), prepare: lambda: None,
               base + 0x2815598: home, base + 0x3f04470: copy,
               base + 0x2de2de0: lambda: h.reg(0)}
    # Relocations read by the original builder and copied string descriptor.
    application_global = h.ram + 0x28300  # Native pointer lives in BSS, outside main.raw.
    h.write(base + 0x4ab2230, application_global)
    h.write(application_global, app)
    h.write(app + 0x58, footer)
    h.write(base + 0x4ac7280, type_guard)
    h.write(base + 0x4ac7278, type_object)
    h.write(type_guard, 1, 1)
    h.write(footer, footer_vt)
    h.write(footer_vt, is_type)
    for got in (0x4ab2bc0, 0x4ab2020):
        h.write(base + got, base + u64(raw, got))
    string_vt = base + u64(raw, 0x4ab2bc0) + 16
    h.write(string_vt + 24, prepare)
    h.write(pocket, base + 0x4a06e28)
    h.write(pocket + 0x210, page)
    h.write(page + 0x570, back)
    h.write(back + 0xa0, unit)
    h.write(unit + 0x14, 5, 4)
    h.write(pocket + 0x1f8, 2, 1)
    h.write(pocket + 0x1c51d, 1, 1)
    h.write(footer + 0x1b0, 2, 4)
    h.write(footer + 0x1f0, 0xffffffff, 4)
    h.write(footer + 0x1f4, 0xffffffff, 4)
    h.write(footer + 0x220, entries)
    def reset(capacity=5):
        h.write(footer + 0x218, 0, 4)
        h.write(footer + 0x21c, capacity, 4)
        h.write(footer + 0x228, pool)
        h.write(footer + 0x239, 0, 1)
        for i in range(5):
            h.u.mem_write(pool + i * 0x88, bytes(0x88))
            h.write(pool + i * 0x88, pool + (i+1) * 0x88 if i < 4 else 0)
    def build(kind=0):
        h.put(0, pocket); h.put(1, kind)
        h.run(h.callback('BuildPocketStorageHint'))
        return [h.read(entries + i*8) for i in range(h.read(footer + 0x218, 4))]
    for at_home in (False, True):
        for kind in (0, 1, 2, 3):
            for mode in (0, 1, 7):
                for suppressed in (0, 1):
                    reset()
                    h.write(pocket + 0x1c4fc, mode, 4)
                    h.write(pocket + 0x1c51e, suppressed, 1)
                    rows = build(kind)
                    own = [p for p in rows if cstr(h.read(p + 0x18)) == KEY]
                    expected = at_home and kind == 0 and mode == 0 and not suppressed
                    assert len(own) == int(expected), (at_home, kind, mode, suppressed, rows)
                    if expected:
                        p = own[0]
                        assert h.read(p, 4) == 7 and h.read(p + 4, 4) == 0xffffffff
                        assert h.read(p + 0x68, 1) == 2
                        assert h.read(p + 0x18) == p + 0x24  # Owned native copy, not stack pointer.
                        assert [cstr(h.read(r + 0x18)) for r in rows] == [b'1101', b'1102', KEY,
                            b'qol_pockets.drop_release', b'qol_pockets.hold_wear']
                    shortcuts = [p for p in rows if h.read(p, 4) in (10, 11)]
                    assert len(shortcuts) == (2 if kind == 0 and mode == 0 and not suppressed else 0)
                    cases += 1
    at_home = True
    h.write(pocket + 0x1c4fc, 0, 4); h.write(pocket + 0x1c51e, 0, 1)
    for capacity in (0, 1, 2, 3, 5):
        reset(capacity)
        rows = build()
        assert len(rows) == min(capacity, 5)
        cases += 1
    reset(); right_type = False
    assert build() == []
    right_type = True; cases += 1
    reset(); h.write(pocket + 0x1c51d, 0, 1)
    assert len(build()) == 1
    h.write(pocket + 0x1c51d, 1, 1); cases += 1
    # Keep the existing batch for this case: calling the native builder
    # twice without its state-machine reset would also duplicate A/B.
    reset(); rows = build()
    h.stubs[base + 0x3094fb4] = lambda: None
    build()
    assert h.read(footer + 0x218, 4) == 5; cases += 1
    del h.stubs[base + 0x3094fb4]

    # Initialize static caption references as the installer does, then execute
    # the actual inline callback. Native/unknown labels must be untouched.
    bank, key, regs, source = [h.ram + o for o in (0x30000, 0x30100, 0x31000, 0x32000)]
    for name, text in (('StorageBank', b'CmnSubWin'), ('StorageCaption', b'0121'),
                       ('WithdrawalCaption', b'0202')):
        target = h.mod + symbol(name)
        h.write(target, base + u64(raw, 0x4ab2020) + 16)
        h.write(target + 8, source)
        h.u.mem_write(source, text + b'\0'); source += 0x100
    h.write(key + 8, source)
    for text in (KEY, WITHDRAWAL_KEY, b'1101', b'1102', b'1201', KEY + b'_other', b''):
        h.u.mem_write(source, text + b'\0')
        h.write(regs + 16, bank); h.write(regs + 24, key)
        h.put(0, regs); h.run(h.callback('ResolveStorageHintCaption'))
        if text in (KEY, WITHDRAWAL_KEY):
            assert cstr(h.read(h.read(regs + 16) + 8)) == b'CmnSubWin'
            assert cstr(h.read(h.read(regs + 24) + 8)) == (b'0121' if text == KEY else b'0202')
        else:
            assert h.read(regs + 16) == bank and h.read(regs + 24) == key
        cases += 1
    # Run the real storage footer builder through its patched A append call.
    # B reuses the descriptor, so this also checks that our longer token does
    # not replace its writable pointer or leak into the subsequent Close row.
    storage = h.ram + 0x40000
    h.stubs[base + 0x3359d70] = lambda: None
    for correct_type in (False, True):
        for mode in (0, 1, 2, 7):
            for flags in (0, 1, 2, 3):
                reset()
                h.write(storage, base + (0x4a16028 if correct_type else 0x4a06e28))
                h.write(storage + 0x220, mode, 4)
                h.put(0, storage); h.put(1, flags)
                h.run(base + 0x316f388)
                rows = [h.read(entries + i*8) for i in range(h.read(footer + 0x218, 4))]
                labels = [cstr(h.read(p + 0x18)) for p in rows]
                expected = []
                if flags & 1:
                    expected.append(WITHDRAWAL_KEY if correct_type and mode == 0 else b'1201')
                if flags & 2:
                    expected.append(b'1202')
                assert labels == expected, (correct_type, mode, flags, labels)
                assert [h.read(p, 4) for p in rows] == ([4] if flags & 1 else []) + ([5] if flags & 2 else [])
                cases += 1
print(f'PASS {cases} storage-hint cases at two bases; native builder and record copying, caption routing. Live rendering pending.')
