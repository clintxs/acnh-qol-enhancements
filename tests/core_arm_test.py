"""Execute compiled Core entrypoint and post-mount activation at two ASLR bases.

FS, hook installation, writable aliases and cache services are modeled. Every
module installer and its validation run from the final ELF. No game/save writes.
"""
from itertools import product
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm64_const import *
from elf_loader import Module, ROOT
RAW = (ROOT / 'Grab 5/working/main.raw').read_bytes()
SLUGS = ('catalog', 'pockets', 'faster-dialogue', 'fitting-room', 'shopping-card', 'faster-crafting')
COUNTS = (16, 16, 7, 11, 10, 2)
DEFAULT = b'enabled=true\nversion=0.3.19\n'

def expected_hook_count(machine, enabled):
    # The existing verbose-dialogue build adds a Catalog message trace hook.
    # Keep normal/diagnostic counts explicit; never infer them from installed hooks.
    trace_enabled = any('NookMessageTrace' in name for name in machine.module.symbols)
    resources = 'content:/qol-enhancements/config/shopping-card.ini' in machine.files
    return 2 + int(resources) + sum(count for count, active in zip(COUNTS, enabled) if active) + (trace_enabled and enabled[0])

class Machine:
    def __init__(self, base, files):
        self.base = base
        self.u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        self.services, self.logs, self.hooks = {}, [], {}
        self.files = {f'content:/qol-enhancements/config/{slug}.ini': data for slug, data in files.items()}
        self.handles, self.opened, self.closed = {}, [], []
        self.open_error = self.read_error = self.size_error = False
        self.custom_fault = None
        self.u.mem_map(base, 0x6000000)
        self.u.mem_write(base, RAW)
        self.ram = base + 0x10000000
        self.u.mem_map(self.ram, 0x200000)
        self.write(base + 0x4a06740, base + 0x30742d0)
        self.write(base + 0x4a10398, base + 0x3c58d24)
        self.module = Module(self, 'qol-enhancements', base + 0x7000000)
        self.alias = base + 0x20000000
        self.u.mem_map(self.alias, 0x4725000)
        storage = self.module.find('patch', 's_Storage')
        self.write(storage, base)
        self.write(storage + 8, self.alias)
        for name in ('armDCacheFlush', 'armICacheInvalidate'):
            self.services[self.module.find(name)] = ('cache', name)
        self.u.hook_add(UC_HOOK_CODE, self.service)
        self.u.hook_add(UC_HOOK_MEM_WRITE, self.alias_write, begin=self.alias, end=self.alias + 0x4725000 - 1)
    def write(self, address, value, size=8):
        self.u.mem_write(address, value.to_bytes(size, 'little'))
    def read(self, address, size=8):
        return int.from_bytes(self.u.mem_read(address, size), 'little')
    def reg(self, index):
        return self.u.reg_read(UC_ARM64_REG_X0 + index)
    def string(self, address):
        out = bytearray()
        while (byte := self.u.mem_read(address + len(out), 1)[0]):
            out.append(byte)
            assert len(out) < 4096
        return out.decode()
    def alias_write(self, u, access, address, size, value, _):
        self.write(self.base + address - self.alias, value, size)
    def service(self, u, pc, size, _):
        if pc not in self.services: return
        kind, value = self.services[pc]
        result = 0
        if kind == 'log':
            self.logs.append(bytes(u.mem_read(self.reg(2), self.reg(1))).decode())
        elif kind == 'install':
            target = self.reg(0)
            assert all(abs(target - old) >= 20 for old in self.hooks), (hex(target), self.hooks)
            self.hooks[target] = self.reg(1)
            u.mem_write(target, b'\xff' * 20)
            result = self.ram + 0x200
        elif kind == 'import':
            if 'OpenFile' in value:
                path = self.string(self.reg(1))
                self.opened.append(path)
                if self.open_error or (self.custom_fault == 'open' and path.endswith('.custom.ini')): result = 0x402
                elif path not in self.files: result = 0x202
                else:
                    handle = len(self.opened)
                    self.handles[handle] = self.files[path]
                    self.write(self.reg(0), handle)
            elif 'GetFileSize' in value:
                if self.size_error or (self.custom_fault == 'size' and self.opened[-1].endswith('.custom.ini')): result = 0x402
                else: self.write(self.reg(0), len(self.handles[self.reg(1)]))
            elif 'ReadFile' in value:
                if self.read_error or (self.custom_fault == 'read' and self.opened[-1].endswith('.custom.ini')): result = 0x402
                else:
                    content = self.handles[self.reg(0)]
                    assert self.reg(1) == 0 and self.reg(3) == len(content)
                    u.mem_write(self.reg(2), content)
            elif 'CloseFile' in value:
                self.closed.append(self.reg(0))
                del self.handles[self.reg(0)]
            else: raise AssertionError(('unexpected import', value))
        elif kind not in ('initialize', 'cache'): raise AssertionError((kind, value))
        u.reg_write(UC_ARM64_REG_X0, result)
        u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
    def call(self, address):
        sp, stop = self.ram + 0x1f0000, self.ram + 0x100
        self.u.reg_write(UC_ARM64_REG_SP, sp)
        self.u.reg_write(UC_ARM64_REG_LR, stop)
        self.u.emu_start(address, stop, count=1500000)
        assert self.u.reg_read(UC_ARM64_REG_PC) == stop
        assert self.u.reg_read(UC_ARM64_REG_SP) == sp
    def boot(self):
        self.call(self.module.find('exl_main'))
    def activate(self):
        self.call(self.module.find('AfterContentMount', 'Callback'))
        assert not self.handles
    @property
    def text(self): return ''.join(self.logs)

def check_all_subsets():
    for base in (0x7100000000, 0x7200000000):
        for flags in product((False, True), repeat=len(SLUGS)):
            m = Machine(base, {slug: DEFAULT for slug, enabled in zip(SLUGS, flags) if enabled})
            m.boot()
            assert len(m.hooks) == 1 and not m.opened and 'WAITING_FOR_CONTENT' in m.text
            m.activate()
            assert len(m.hooks) == expected_hook_count(m, flags), m.text
            assert 'BOOT_OK' in m.text and f'enabled_modules={sum(flags)} errors=0' in m.text, m.text
            assert len(m.opened) == 2 + len(SLUGS) + sum(flags) and len(m.closed) == sum(flags)
            assert m.read(base + 0xbc4140, 4) == 0x52803848
            assert base + 0xbc4144 in m.hooks
            assert m.read(base + 0x1af9ff0, 4) == 0x528c0049
            assert (m.read(base + 0x4a10398) != base + 0x3c58d24) == flags[3]
            if flags[2]:
                for offset in (0x1afa00c, 0x1afdec0, 0x1afded4, 0x1afdefc, 0x1a969e0):
                    assert base + offset in m.hooks
                # No code-cave helper or legacy instruction replacements remain.
                assert bytes(m.u.mem_read(base + 0x3f06fb0, 48)) == RAW[0x3f06fb0:0x3f06fe0]
            before = (len(m.hooks), len(m.opened), len(m.logs))
            m.activate()
            assert before == (len(m.hooks), len(m.opened), len(m.logs))
    print('PASS: all 64 module subsets at two bases; deferred activation, isolated C++ hooks, C++ dialogue hooks, once-only reads')

def check_errors():
    for slug in SLUGS:
        for data, expected in ((b'enabled=true\nversion=0.0.9', 'below minimum'),
                               (b'enabled=true\nversion=0.4.0', 'newer than Core'),
                               (b'enabled=false\nversion=0.0.1', 'enabled=false'),
                               (b'enabled=true', 'config invalid'),
                               (b'x' * 4097, 'config invalid'),
                               (b'', 'config invalid')):
            m = Machine(0x7100000000, {slug: data})
            m.boot(); m.activate()
            assert expected in m.text, m.text
            if b'version=0.4.0' in data:
                assert 'BOOT_OK' in m.text and 'enabled_modules=1' in m.text
            else:
                assert len(m.hooks) == 2 + (slug == 'shopping-card') and m.read(m.base + 0xbc4140, 4) == 0x52803848
                assert m.read(m.base + 0x1af9ff0, 4) == 0x528c0049
    for fault in ('open_error', 'read_error', 'size_error'):
        m = Machine(0x7100000000, {slug: DEFAULT for slug in SLUGS})
        setattr(m, fault, True)
        m.boot(); m.activate()
        assert len(m.hooks) == 3 and 'BOOT_PARTIAL' in m.text
    # A startup conflict prevents even the Core discovery hook.
    m = Machine(0x7100000000, {})
    m.write(m.base + 0x37b5460, 0, 4)
    m.boot()
    assert not m.hooks and not m.opened and 'PROFILE_MISMATCH' in m.text
    # Dialogue hook sites and legacy paths must remain pristine; validate before installing.
    for offset in (0x1af9ff0, 0x1afdd20, 0x1afdebc, 0x1afded4,
                   0x1d9dc3c, 0x1d9e11c, 0x1d9e224, 0x1d9e244,
                   0x1b04824, 0x1b04b80, 0x1b048ec, 0x1b04b5c, 0x5f5b28):
        m = Machine(0x7100000000, {'faster-dialogue': DEFAULT, 'amiibo': DEFAULT})
        m.write(m.base + offset, 0xdeadbeef, 4)
        m.boot(); m.activate()
        assert 'Faster Dialogue] DISABLED' in m.text and 'Amiibo timeout: READY' in m.text
        assert not any(m.base + offset in m.hooks for offset in (0x1afa00c, 0x1afdec0))
    m = Machine(0x7100000000, {slug: b'enabled=true\nversion=0.1.0' for slug in SLUGS[:4]})
    m.boot(); m.activate()
    assert len(m.hooks) == expected_hook_count(m, (True,) * 4) and 'BOOT_OK' in m.text and 'enabled_modules=4 errors=0' in m.text
    m = Machine(0x7100000000, {'amiibo': DEFAULT, 'faster-dialogue': DEFAULT})
    m.write(m.base + 0xbc4140, 0x52846508, 4) # Older standalone Amiibo payload.
    m.boot(); m.activate()
    assert 'Amiibo timeout: DISABLED' in m.text and 'Faster Dialogue: ENABLED' in m.text
    assert m.base + 0xbc4144 not in m.hooks
    print('PASS: minimum/future/disabled/malformed/oversize versions, file failures, startup and patch conflicts')

def check_amiibo_hook():
    configurations = (
        ({}, 9000),
        ({'core': b'amiibo_timeout_frames=1200'}, 1200),
        ({'core': b'amiibo_timeout_frames=0'}, 0),
        ({'core.custom': b'amiibo_timeout_frames=0'}, 0),
        ({'core': b'amiibo_timeout_frames=1200', 'core.custom': b'amiibo_timeout_frames=1'}, 1),
        ({'core': b'amiibo_timeout_frames=0', 'core.custom': b'amiibo_timeout_frames=9000'}, 9000),
        ({'core': b'amiibo_timeout_frames=450', 'core.custom': b'# inherit'}, 450),
        ({'core': b'amiibo_timeout_frames=65535'}, 65535),
        ({'core': b'amiibo_timeout_frames=65536'}, 9000),
        ({'core': b'amiibo_timeout_frames=-1'}, 9000),
        ({'core': b'amiibo_timeout_frames=9,000'}, 9000),
        ({'core': b'amiibo_timeout_frames=0\namiibo_timeout_frames=1'}, 9000),
        ({'core': b'amiibo_timeout_frames=1200', 'core.custom': b'amiibo_timeout_frames=0\namiibo_timeout_frames=0'}, 1200),
        ({'core': b'amiibo_timeout_frames=450', 'core.custom': b'amiibo_timeout_frames=bad'}, 450),
        ({'core': b'amiibo_timeout_frames=0\n\xff'}, 9000),
        ({'core': b'amiibo_timeout_frames=450', 'core.custom': b'amiibo_timeout_frames=0\n\xff'}, 450),
        ({'core': b'x' * 4097, 'core.custom': b'amiibo_timeout_frames=600'}, 600),
        ({'core': b'amiibo_timeout_frames=600', 'core.custom': b'x' * 4097}, 600),
        ({'amiibo': b'enabled=false\nversion=0.3.18', 'amiibo.custom': b'enabled=false'}, 9000),
    )
    for base in (0x7100000000, 0x7200000000):
        for files, frames in configurations:
            m = Machine(base, files)
            m.boot()
            assert len(m.hooks) == 1 and not m.opened
            m.activate()
            assert len(m.hooks) == 1 + bool(frames) and 'BOOT_OK' in m.text, m.text
            assert m.opened[:2] == ['content:/qol-enhancements/config/core.ini', 'content:/qol-enhancements/config/core.custom.ini']
            assert not any('/amiibo.' in name for name in m.opened)
            assert (base + 0xbc4144 in m.hooks) == bool(frames)
            if frames:
                assert f'frames={frames}' in m.text
                registers = m.ram + 0x1000
                m.write(registers + 8 * 8, 450)
                m.u.reg_write(UC_ARM64_REG_X0, registers)
                m.call(m.hooks[base + 0xbc4144])
                assert m.read(registers + 8 * 8) == frames
                m.u.mem_write(base + 0xbc4144, RAW[0xbc4144:0xbc4158])
                m.u.reg_write(UC_ARM64_REG_X8, m.read(registers + 8 * 8))
            else:
                # Zero installs no hook: execute the original MOV 450 as well.
                assert bytes(m.u.mem_read(base + 0xbc4140, 24)) == RAW[0xbc4140:0xbc4158]
                m.u.emu_start(base + 0xbc4140, base + 0xbc4144, count=1)
            actor = m.ram + 0x2000
            m.u.reg_write(UC_ARM64_REG_X19, actor)
            m.u.emu_start(base + 0xbc4144, base + 0xbc414c, count=10)
            actual = frames or 450
            assert m.read(actor + 0xf8, 2) == actual and m.read(actor + 0xf4, 4) == 0
            # Execute unsigned native decrement and its expiry branch, including
            # 65535 (above signed-halfword range) and the last remaining frame.
            for remaining in (actual, 1):
                m.write(actor + 0xf8, remaining, 2)
                m.u.emu_start(base + 0xbc44b0, 0, count=5)
                assert m.read(actor + 0xf8, 2) == remaining - 1
                assert m.u.reg_read(UC_ARM64_REG_PC) == base + (0xbc44c4 if remaining == 1 else 0xbc4744)
            before = (len(m.hooks), len(m.opened), len(m.logs))
            m.activate()
            assert before == (len(m.hooks), len(m.opened), len(m.logs))
        # File failures retain defaults/prior settings, without disabling Core.
        for fault in ('open', 'size', 'read'):
            m = Machine(base, {'core': b'amiibo_timeout_frames=450', 'core.custom': b'amiibo_timeout_frames=0'})
            m.custom_fault = fault
            m.boot(); m.activate()
            assert 'frames=450' in m.text and 'BOOT_OK' in m.text and 'WARNING:' in m.text
        # Disabled means no fingerprint check or writes, even with a conflict.
        m = Machine(base, {'core.custom': b'amiibo_timeout_frames=0', 'faster-dialogue': DEFAULT})
        m.write(base + 0xbc4140, 0xdeadbeef, 4)
        m.boot(); m.activate()
        assert 'BOOT_OK' in m.text and 'Faster Dialogue: ENABLED' in m.text
        assert m.read(base + 0xbc4140, 4) == 0xdeadbeef and base + 0xbc4144 not in m.hooks
    print('PASS: Core-only default amiibo hook; regular/custom precedence, absent Core config, 0 untouched native behavior, 1/450/9000/65535 native stores/countdown/expiry, invalid-file fallback, ignored legacy Amiibo files and once-only activation at two bases')

def check_custom_preferences():
    for custom, summary in ((b'', 'base=2 boost=4 faster_ui=false faster_player=false npc_count=0'),
                            (b'base_speed=4\nboost_speed=2', 'base=4 boost=4'),
                            (b'base_speed=2\nboost_speed=2', 'base=2 boost=2'),
                            (b'base_speed=0\nfaster_ui=yes', 'base=2 boost=4 faster_ui=false'),
                            (b'x' * 4097, 'base=2 boost=4 faster_ui=false')):
        m = Machine(0x7100000000, {'faster-dialogue': DEFAULT, 'faster-dialogue.custom': custom})
        m.boot(); m.activate()
        assert summary in m.text and 'BOOT_OK' in m.text, m.text
        assert len(m.opened) == 2 + len(SLUGS) + 1 and len(m.closed) == 2
    for fault in ('open', 'size', 'read'):
        m = Machine(0x7100000000, {'faster-dialogue': DEFAULT, 'faster-dialogue.custom': b'base_speed=4'})
        m.custom_fault = fault
        m.boot(); m.activate()
        assert 'WARNING:' in m.text and 'custom config' in m.text and 'BOOT_OK' in m.text
        assert 'base=2 boost=4 faster_ui=false' in m.text
        assert len(m.closed) == (1 if fault == 'open' else 2)
    for marker in (None, b'enabled=false\nversion=0.2.0'):
        files = {'faster-dialogue.custom': b'base_speed=4'}
        if marker: files['faster-dialogue'] = marker
        m = Machine(0x7100000000, files)
        m.boot(); m.activate()
        assert len(m.opened) == 2 + (len(SLUGS) + 1 if marker else len(SLUGS)) and len(m.hooks) == 2
    print('PASS: missing/malformed/oversized/unreadable custom preferences preserve defaults')

def check_config_layers():
    for base in (0x7100000000, 0x7200000000):
        for slug in SLUGS:
            # Core supplies the naming and merge behavior for every module.
            accepted_version = b'0.3.18' if slug == 'faster-crafting' else b'0.3.2'
            for regular, custom, active in (
                (DEFAULT, b'enabled=false', False),
                (DEFAULT.replace(b'true', b'false'), b'enabled=true', True),
                (DEFAULT, b'', True),
                (DEFAULT, b'version=0.0.1', False),
                (DEFAULT, b'version=' + accepted_version, True),
                (DEFAULT, b'enabled=false\nversion=bad', True),
                (DEFAULT, b'enabled=false\nenabled=true', True),
                (None, b'enabled=true\nversion=0.3.2', False),
                (b'enabled=true', b'version=0.3.2', False),
            ):
                files = {slug + '.custom': custom}
                if regular is not None: files[slug] = regular
                m = Machine(base, files)
                m.boot(); m.activate()
                assert len(m.hooks) == expected_hook_count(m, [active and name == slug for name in SLUGS]), m.text
                paths = [f'content:/qol-enhancements/config/{name}.ini' for name in SLUGS]
                if regular is not None and b'version=' in regular:
                    paths.insert(SLUGS.index(slug) + 1, f'content:/qol-enhancements/config/{slug}.custom.ini')
                assert m.opened == ['content:/qol-enhancements/config/core.ini', 'content:/qol-enhancements/config/core.custom.ini'] + paths, m.opened
                before = (len(m.opened), len(m.logs), len(m.hooks))
                m.activate()
                assert before == (len(m.opened), len(m.logs), len(m.hooks))
            for fault in ('open', 'size', 'read'):
                m = Machine(base, {slug: DEFAULT, slug + '.custom': b'enabled=false'})
                m.custom_fault = fault
                m.boot(); m.activate()
                assert len(m.hooks) == expected_hook_count(m, [name == slug for name in SLUGS]), m.text
                assert 'WARNING:' in m.text and 'BOOT_OK' in m.text
        regular = DEFAULT + b'base_speed=4\nboost_speed=2\nfaster_ui=true\nfaster_player=true\nfaster_npc_list=Timmy,Tommy'
        cases = (
            (b'base_speed=2\nfaster_ui=false\nfaster_npc_list=Wilbur', 'base=2 boost=2 faster_ui=false faster_player=true npc_count=1'),
            (b'base_speed=bad\nfaster_ui=bad', 'base=4 boost=4 faster_ui=true faster_player=true npc_count=2'),
            (b'faster_npc_list=', 'base=4 boost=4 faster_ui=true faster_player=true npc_count=0'),
            (b'base_speed=2\nversion=bad', 'base=4 boost=4 faster_ui=true faster_player=true npc_count=2'),
            (b'base_speed=2\nbase_speed=2', 'base=4 boost=4 faster_ui=true faster_player=true npc_count=2'),
        )
        for custom, summary in cases:
            m = Machine(base, {'faster-dialogue': regular, 'faster-dialogue.custom': custom})
            m.boot(); m.activate()
            assert summary in m.text and 'BOOT_OK' in m.text, m.text
        # All preference keys work equally in either file.
        preferences = b"base_speed=2\nboost_speed=4\nfaster_ui=true\nfaster_player=true\nfaster_npc_list=Timmy,Tommy,Orville,Wilbur,Kapp'n"
        for files in ({'faster-dialogue': DEFAULT + preferences},
                      {'faster-dialogue': DEFAULT, 'faster-dialogue.custom': preferences}):
            m = Machine(base, files)
            m.boot(); m.activate()
            assert 'base=2 boost=4 faster_ui=true faster_player=true npc_count=6' in m.text, m.text
    print('PASS: shared regular/custom order, enabled/version overrides for all six modules, atomic metadata, failure fallback, inheritance and settings parity at two bases')

if __name__ == '__main__':
    check_all_subsets()
    check_errors()
    check_amiibo_hook()
    check_custom_preferences()
    check_config_layers()
