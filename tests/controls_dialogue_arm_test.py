"""Independent Controls activation, native-speed parity and layered settings."""
import itertools
import struct
from core_arm_test import Machine, CONTROLS_MANUAL as DEFAULT
from dialogue_hurry_arm_test import HurryMachine

REVEAL = (0x1afa00c, 0x1afdec0, 0x1afded4, 0x1afdefc, 0x1a969e0)
ADVANCE = 0x1afa530
NAMES = (0x1b04824, 0x1b04b80)
PICKUP = (0x2461f30, 0x2222a34, 0x2326e2c, 0x21baf28, 0x23282d0)


def main():
    count = 0
    for base in (0x7100000000, 0x7200000000):
        m = HurryMachine(base, faster_dialogue=False)
        native = HurryMachine(base, enabled=False)
        assert len(m.hooks) == 14
        assert all(base + a in m.hooks for a in (*REVEAL, ADVANCE, *PICKUP))
        assert all(base + a not in m.hooks for a in NAMES)
        # No Faster Dialogue: ordinary and B/L/R-held text must be byte-for-byte
        # native through both reveal paths, all native gates and overrides.
        for state, mode, forced, disabled, override, held in itertools.product(
                (0, 1, 2, 3, 4, 5), range(3), (0, 1), (0, 1), (-1., 0., .75), (0, 1)):
            m.prepare(buttons=0, state=3); native.prepare(buttons=0, state=3)
            case = (state, mode, forced, disabled, 9, 1, 1, override)
            assert m.run(case, held) == native.run(case, held), case
            count += 1
        for buttons, previous, getter, fallback in itertools.product(
                (0, 1, 2, 0x10, 0x2000, 0x4000, 0x4010), (0, 1), (False, True), (False, True)):
            m.prepare(state=3); native.prepare(state=3)
            m.input(buttons, previous, getter, fallback)
            native.input(buttons, previous, getter, fallback)
            assert m.read(m.obj + 0x21b4, 1) == native.read(native.obj + 0x21b4, 1)
            count += 1
        # Controls alone: hurry and resume native speed immediately on release.
        for buttons, flag in ((0x10, 5), (0, 0), (0x10, 5), (0, 0)):
            m.prepare(state=3, buttons=buttons)
            result, _ = m.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
            assert result[0x21b7] == flag
            if flag == 5:
                assert abs(struct.unpack_from('<f', result, 0x2194)[0] - 34.) < 1e-5
                m.renderer(5, -1., 2.4, .5, False)
        for reveal, choice in itertools.product((2, 3, 4), (False, True)):
            m.prepare(reveal=reveal, choice=choice)
            assert m.advance() == (3 if reveal != 3 else 6 if choice else 7)
            assert m.request == 1
        for bank in ('SYS_Get_Fish', 'SYS_Get_Insect', 'SYS_Get_DiveFish'):
            m.prepare(window=2); m.message_id(bank, '001')
            assert m.advance() == 5 and m.request == 0
            m.message_id(bank, 'Fish_02233')
            assert m.advance() == 7 and m.request == 1

        # Setting parity and rollback use the actual shared INI loader.
        cases = (
            (DEFAULT, None, True),
            (DEFAULT + b'hold_to_advance_dialogue=false', None, False),
            (DEFAULT, b'hold_to_advance_dialogue=false', False),
            (DEFAULT + b'hold_to_advance_dialogue=false', b'hold_to_advance_dialogue=true', True),
            (DEFAULT + b'hold_to_advance_dialogue=false', b'# inherit', False),
            (DEFAULT + b'hold_to_advance_dialogue=false', b'hold_to_advance_dialogue=bad', False),
            (DEFAULT + b'hold_to_advance_dialogue=false', b'hold_to_advance_dialogue=true\nhold_to_advance_dialogue=false', False),
            (DEFAULT + b'hold_to_advance_dialogue=false', b'hold_to_advance_dialogue=true\n\xff', False),
            (DEFAULT + b'hold_to_advance_dialogue=false', b'hold_to_advance_dialogue=true\nversion=bad', False),
            (DEFAULT, b'enabled=false', False),
            (DEFAULT.replace(b'true', b'false'), b'enabled=true', True),
            (None, b'hold_to_advance_dialogue=true', False),
        )
        for speed_enabled, (regular, custom, hurry) in itertools.product((False, True), cases):
            x = HurryMachine(base, faster_dialogue=speed_enabled, controls=regular, controls_custom=custom,
                             dialogue_custom=b'base_speed=6\nboost_speed=8\nboost_ui_messages=false\nboost_unnamed_messages=false')
            assert (base + ADVANCE in x.hooks) == hurry
            assert all((base + a in x.hooks) == (hurry or speed_enabled) for a in REVEAL)
            assert all((base + a in x.hooks) == speed_enabled for a in NAMES)
            x.prepare()
            assert x.advance() == (7 if hurry else 5)
            x.prepare(state=3)
            result, _ = x.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
            assert result[0x21b7] == (5 if hurry else 1 if speed_enabled else 0)
            if speed_enabled and not hurry:
                assert abs(struct.unpack_from('<f', result, 0x2194)[0] - 5.2) < 1e-5
            count += 1
        print(f'PASS {base:#x}: Controls-only native speed/input parity, 30x hold/release, independent settings and first-catch stops')

    # Conflicts stay within each policy's actual dependencies.
    for offset in (*NAMES, 0x1d9dc3c):
        m = Machine(0x7100000000, {'controls': DEFAULT, 'faster-dialogue': DEFAULT})
        m.write(m.base + offset, 0xdeadbeef, 4); m.boot(); m.activate()
        assert 'Faster Dialogue: ERROR' in m.text and 'Controls: ENABLED' in m.text
        assert m.base + ADVANCE in m.hooks and all(m.base + a not in m.hooks for a in NAMES)
    for offset in (ADVANCE, 0x2435188):
        m = Machine(0x7100000000, {'controls': DEFAULT, 'faster-dialogue': DEFAULT})
        m.write(m.base + offset, 0xdeadbeef, 4); m.boot(); m.activate()
        assert 'Controls: ERROR' in m.text and 'Faster Dialogue: ENABLED' in m.text
        assert m.base + ADVANCE not in m.hooks and all(m.base + a in m.hooks for a in (*NAMES, *REVEAL, *PICKUP))
        # Disabled hurry never validates or installs its advance/controller path.
        m = Machine(0x7100000000, {'controls': DEFAULT + b'hold_to_advance_dialogue=false'})
        m.write(m.base + offset, 0xdeadbeef, 4); m.boot(); m.activate()
        assert 'BOOT_OK' in m.text and len(m.hooks) == 8
    for offset in REVEAL:
        m = Machine(0x7100000000, {'controls': DEFAULT, 'faster-dialogue': DEFAULT})
        m.write(m.base + offset, 0xdeadbeef, 4); m.boot(); m.activate()
        assert len(m.hooks) == 8 and m.base + ADVANCE not in m.hooks
    m = Machine(0x7100000000, {'controls': DEFAULT})
    m.write(m.base + PICKUP[0], 0xdeadbeef, 4); m.boot(); m.activate()
    assert m.base + ADVANCE in m.hooks and all(m.base + a in m.hooks for a in REVEAL)
    print(f'PASS: {count} independent Controls dialogue cases; shared sites install once and conflicts remain isolated')


if __name__ == '__main__': main()
