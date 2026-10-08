"""Execute compiled Core entrypoint and post-mount activation at two ASLR bases.

FS, hook installation, writable aliases and cache services are modeled. Every
module installer and its validation run from the final ELF. No game/save writes.
"""
from pathlib import Path
from itertools import product
import struct
import json
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm64_const import *
from elf_loader import Module, ROOT
RAW = (ROOT / 'Grab 5/working/main.raw').read_bytes()
SLUGS = ('catalog', 'pockets', 'faster-dialogue', 'fitting-room', 'amiibo')
COUNTS = (16, 16, 0, 11, 0)
DEFAULT = b'enabled=true\nversion=1.0.0\n'

class Machine:
    def __init__(self, base, files):
        self.base = base
        self.u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        self.services, self.logs, self.hooks = {}, [], {}
        self.files = {f'content:/qol-enhancements/config/{slug}.ini': data for slug, data in files.items()}
        self.handles, self.opened, self.closed = {}, [], []
        self.open_error = self.read_error = self.size_error = False
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
                if self.open_error: result = 0x402
                elif path not in self.files: result = 0x202
                else:
                    handle = len(self.opened)
                    self.handles[handle] = self.files[path]
                    self.write(self.reg(0), handle)
            elif 'GetFileSize' in value:
                if self.size_error: result = 0x402
                else: self.write(self.reg(0), len(self.handles[self.reg(1)]))
            elif 'ReadFile' in value:
                if self.read_error: result = 0x402
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
        for flags in product((False, True), repeat=5):
            m = Machine(base, {slug: DEFAULT for slug, enabled in zip(SLUGS, flags) if enabled})
            m.boot()
            assert len(m.hooks) == 1 and not m.opened and 'WAITING_FOR_CONTENT' in m.text
            m.activate()
            assert len(m.hooks) == 1 + sum(count for count, enabled in zip(COUNTS, flags) if enabled), m.text
            assert 'BOOT_OK' in m.text and f'enabled_modules={sum(flags)} errors=0' in m.text, m.text
            assert len(m.opened) == 5 and len(m.closed) == sum(flags)
            assert m.read(base + 0xbc4140, 4) == (0x52846508 if flags[4] else 0x52803848)
            assert m.read(base + 0x1af9ff0, 4) == (0x52880049 if flags[2] else 0x528c0049)
            assert (m.read(base + 0x4a10398) != base + 0x3c58d24) == flags[3]
            if flags[2]:
                # Final runtime writes must equal the audited standalone boost.
                reference = json.loads((Path(__file__).parent / 'baseline_hashes.json').read_text())
                for offset, payload in reference['dialogue'].items():
                    expected = bytes.fromhex(payload)
                    assert bytes(m.u.mem_read(base + int(offset, 16), len(expected))) == expected
            before = (len(m.hooks), len(m.opened), len(m.logs))
            m.activate()
            assert before == (len(m.hooks), len(m.opened), len(m.logs))
    print('PASS: all 32 module subsets at two bases; deferred activation, isolated hooks/patches, exact boost payload, once-only reads')

def check_errors():
    for slug in SLUGS:
        for data, expected in ((b'enabled=true\nversion=0.9.9', 'below minimum'),
                               (b'enabled=true\nversion=1.1.0', 'newer than Core'),
                               (b'enabled=false\nversion=0.1.0', 'enabled=false'),
                               (b'enabled=true', 'config invalid'),
                               (b'x' * 4097, 'config invalid'),
                               (b'', 'config invalid')):
            m = Machine(0x7100000000, {slug: data})
            m.boot(); m.activate()
            assert expected in m.text, m.text
            if b'version=1.1.0' in data:
                assert 'BOOT_OK' in m.text and 'enabled_modules=1' in m.text
            else:
                assert len(m.hooks) == 1 and m.read(m.base + 0xbc4140, 4) == 0x52803848
                assert m.read(m.base + 0x1af9ff0, 4) == 0x528c0049
    for fault in ('open_error', 'read_error', 'size_error'):
        m = Machine(0x7100000000, {slug: DEFAULT for slug in SLUGS})
        setattr(m, fault, True)
        m.boot(); m.activate()
        assert len(m.hooks) == 1 and 'BOOT_PARTIAL' in m.text
    # A startup conflict prevents even the Core discovery hook.
    m = Machine(0x7100000000, {})
    m.write(m.base + 0x37b5460, 0, 4)
    m.boot()
    assert not m.hooks and not m.opened and 'PROFILE_MISMATCH' in m.text
    # Dialogue helpers must remain pristine; reject before writing any record.
    for offset in (0x1af9ff0, 0x3f06fb0):
        m = Machine(0x7100000000, {'faster-dialogue': DEFAULT, 'amiibo': DEFAULT})
        m.write(m.base + offset, 0xdeadbeef, 4)
        m.boot(); m.activate()
        assert 'Faster Dialogue] DISABLED' in m.text and 'Amiibo: ENABLED' in m.text
        assert m.read(m.base + 0x1afdec8, 4) == 0x3934dec9
    print('PASS: minimum/future/disabled/malformed/oversize versions, file failures, startup and patch conflicts')

if __name__ == '__main__':
    check_all_subsets()
    check_errors()
