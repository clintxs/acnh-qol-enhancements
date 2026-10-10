"""Verify shipping log filtering and Core overlays through the compiled loader."""
from itertools import product
from core_arm_test import Machine, DEFAULT, SLUGS, RELEASE_VERSION, expected_hook_count


def machine(base, files):
    m = Machine(base, files, verbose=False)
    m.boot()
    assert not m.logs  # RomFS settings are unavailable before mounting.
    m.activate()
    return m


def check():
    for base in (0x7100000000, 0x7200000000):
        modules = {slug: DEFAULT for slug in SLUGS}
        default = machine(base, modules)
        lines = default.text.splitlines()
        assert len(lines) == 16, default.text  # Core + six module versions/settings + two detailed settings.
        assert 'log_versions=true log_settings=true log_debug=false' in default.text
        assert 'crafting_speed=6' in default.text and 'faster_unnamed=true' in default.text
        for line in lines:
            assert 'settings:' in line or 'Core:' in line or '; enabled=' in line, line
        assert len(default.hooks) == expected_hook_count(default, (True,) * len(SLUGS))
        for versions, settings, debug in product((False, True), repeat=3):
            config = (f'log_versions={str(versions).lower()}\nlog_settings={str(settings).lower()}\n'
                      f'log_debug={str(debug).lower()}').encode()
            files = dict(modules, core=b'log_versions=false\nlog_settings=false\nlog_debug=false', **{'core.custom': config})
            m = machine(base, files)
            assert ('Core:' in m.text) == versions, m.text
            assert (f'Catalog: {RELEASE_VERSION}' in m.text) == versions, m.text
            assert ('settings:' in m.text) == settings, m.text
            assert ('BOOT_OK' in m.text) == debug, m.text
            assert ('STACKS_READY' in m.text) == debug, m.text
            assert ('DIY preview: READY' in m.text) == debug, m.text
            assert len(m.hooks) == len(default.hooks)
            if not (versions or settings or debug): assert not m.logs
        # Errors and warnings survive every ordinary logging switch being off.
        quiet = b'log_versions=false\nlog_settings=false\nlog_debug=false'
        m = Machine(base, dict(modules, core=quiet), verbose=False)
        m.write(base + 0x22fc6bc, 0xdeadbeef, 4)
        m.boot(); m.activate()
        assert 'instruction mismatch' in m.text and 'ERROR installation failed' in m.text and 'BOOT_PARTIAL' in m.text
        assert 'settings:' not in m.text and 'Core:' not in m.text and 'STACKS_READY' not in m.text
        m = machine(base, {'core': quiet, 'core.custom': b'log_debug=bad', 'faster-dialogue': DEFAULT + b'boost_speed=0'})
        assert m.text.count('WARNING:') == 2, m.text
        assert 'settings:' not in m.text and 'BOOT_OK' not in m.text
        # Atomic invalid overlay, duplicate preference rollback, disabled modules,
        # actual NPC list values, and restart-only activation behavior.
        m = machine(base, dict(modules, core=quiet, **{'core.custom': b'log_debug=true\nlog_debug=false'}))
        assert 'WARNING:' in m.text and 'BOOT_OK' not in m.text
        m = machine(base, {'core': quiet, 'core.custom': b'log_debug=true\n\xff'})
        assert 'WARNING:' in m.text and 'BOOT_OK' not in m.text
        m = machine(base, {
            'faster-dialogue': DEFAULT + b'faster_npc_list=owl,Blathers',
            'faster-dialogue.custom': b'enabled=false\nbase_speed=6',
            'crafting': DEFAULT.replace(b'true', b'false')})
        assert 'base=6 boost=6' in m.text and 'faster_npc_list[0]=owl' in m.text and 'faster_npc_list[1]=blathers' in m.text
        assert '[QoL Enhancements/Crafting] settings: diy_app_preview=true crafting_speed=6' in m.text
        assert m.text.count('settings: enabled=false') == 2
        assert 'WARNING: boost_speed below base_speed' in m.text
        assert len(m.hooks) == 2  # Only Core discovery and amiibo timeout.
        prior = m.text
        m.activate()
        assert m.text == prior
    print('PASS: quiet default startup, all logging switch combinations, regular/custom precedence, mandatory warnings/errors, invalid/duplicate overlays, disabled-module settings/NPC values, unchanged hooks and once-only logging at two bases')


if __name__ == '__main__': check()
