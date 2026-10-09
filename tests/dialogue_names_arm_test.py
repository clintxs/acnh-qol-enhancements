"""Execute native name-label preparation/getters and compiled capture hooks.

The name delegate's resource lookup is modeled; actual localized MSBT names
feed its native stack holder. No packaged name table or live game access.
"""
from pathlib import Path
import sys
import tempfile
from compression import zstd
from dialogue_arm_test import DialogueMachine, DEFAULT
from elf_loader import ROOT
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_SP, UC_ARM64_REG_PC, UC_ARM64_REG_LR
sys.path.insert(0, str(ROOT / 'qol-enhancements/tools'))
from sarc_tool import read_sarc
from msbt_tool import read_msbt


class NameMachine(DialogueMachine):
    def __init__(self, base, preferences, regular=False):
        super().__init__(base, None if regular else preferences,
                         regular=DEFAULT + preferences if regular else DEFAULT)
        self.name_valid, self.name_fill = base + 0x1b00f0c, base + 0x1b00f10
        self.name_methods = self.ram + 0x68000
        self.write(self.name_methods, self.name_fill)
        self.write(self.name_methods + 0x10, self.name_valid)
        # Native name holder's original vtable and getters, relocated like
        # the game's loader; capacity at +10 is deliberately not text length.
        self.write(base + 0x4ab6b18, base + 0x4837f40)
        for slot, target in ((0x18, 0x5f5b28), (0x20, 0x5f5b30), (0x38, 0xa44264)):
            self.write(base + 0x4837f50 + slot, base + target)
        self.write(self.context, self.meta)
        self.write(self.context + 0x20, self.ram + 0x69000)
        self.write(self.ram + 0x69218, self.obj)
        self.snapshot = self.module.find('dialogue', '4NameE')

    def service(self, u, pc, size, user):
        if getattr(self, 'name_methods', None) is not None and pc in (self.name_valid, self.name_fill, self.base + 0x3f04450):
            if pc == self.name_valid:
                assert self.reg(0) == self.source + 0x128
                u.reg_write(UC_ARM64_REG_X0, self.valid_label)
            elif pc == self.name_fill:
                assert self.reg(0) == self.source + 0x128
                holder = self.reg(1)
                data = self.read(holder + 8)
                assert data == holder + 0xe0 and self.read(holder + 0x10, 4) == 64
                assert len(self.label_data) <= 128
                u.mem_write(data, self.label_data)
                self.last_data = data
            else:
                u.mem_write(self.reg(0), bytes([self.reg(1) & 255]) * self.reg(2))
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
            return
        super().service(u, pc, size, user)

    def label(self, value, valid=True, terminate=True):
        self.valid_label = valid
        self.label_data = value.encode('utf-16-le', errors='surrogatepass') + (b'\0\0' if terminate else b'')
        self.write(self.source + 0x128, self.name_methods)
        self.u.reg_write(UC_ARM64_REG_X0, self.context)
        self.u.reg_write(UC_ARM64_REG_SP, self.stack + 0x3000)
        # Execute real construction, delegate validity/dispatch, original
        # virtual getters and both hooks, stopping before name-pane rendering.
        self.execute(0x1b04808, (0x1b04b90, 0x1b04da8))
        assert 0x1b04824 in self.trace and 0x1b04b80 in self.trace
        return self.string(self.snapshot + 20)

    def tier(self):
        result, _ = self.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
        return result[0x21b7]


def main():
    count = 0
    for base in (0x7100000000, 0x7200000000):
        m = NameMachine(base, 'faster_npc_list=Blathers,Sócrates,フータ,Блезерс,傅达,🦉'.encode(), regular=base == 0x7200000000)
        m.speaker('owl')
        for name in ('Blathers', 'blathers', 'Sócrates', 'フータ', 'Блезерс', '傅达', '🦉'):
            assert m.label(name) == name
            assert bytes(m.u.mem_read(m.last_data, len(m.label_data))) == m.label_data
            assert m.tier() == 3, (name, m.text)
            count += 1
        assert m.label('Blathers') == 'Blathers'
        # Native stack text can be reused immediately: we own the name copy.
        m.u.mem_write(m.last_data, b'Z' * 128)
        assert m.tier() == 3
        for name, valid, terminate in (('Thibou', True, True), ('', True, True),
                                      ('Blathers', False, True), ('B' * 64, True, False),
                                      ('\ud800', True, True), ('A\x0e\0\0\u0400', True, True)):
            m.label(name, valid, terminate)
            assert m.tier() == 1, (repr(name), valid)
            count += 1
        m.label('Blathers')
        m.handle += 1
        m.speaker('owl')
        assert m.tier() == 1 # Same source allocation, new entity generation.
        m.label('Blathers')
        assert m.tier() == 3
        m.speaker('owl', dead=True)
        assert m.tier() == 1
        m.speaker('owl', window=1)
        assert m.tier() == 1 # Name never reclassifies UI or player narration.
        m.speaker('owl', kind=0x4f)
        assert m.tier() == 1
        m.speaker('rco')
        m.label('Tom Nook')
        assert m.tier() == 1
        m.speaker('owl')
        m.label('Blathers')
        m.write(m.meta + 0xc0, m.source + 0x200)
        m.write(m.source + 0x208, m.handle, 4)
        assert m.tier() == 1 # A different source cannot inherit the name.
        ids = NameMachine(base, b'faster_npc_list=owl')
        ids.speaker('owl')
        ids.label('Thibou')
        assert ids.tier() == 3 # Internal IDs are language independent.
        ids.label('', valid=False)
        assert ids.tier() == 3 # An unavailable label does not break ID matching.
        count += 8
        print(f'PASS {base:#x}: original name holder/delegate/getters, owned UTF-8 capture, locale matching, source/generation guards and ID fallback')

    # All 14 supplied language sets, both villagers and special NPCs, including
    # Russian grammar tags with embedded zeros. No generated game data is saved.
    m = NameMachine(0x7100000000, b'faster_npc_list=owl')
    m.speaker('owl')
    locales = 0
    with tempfile.TemporaryDirectory(prefix='qol-npc-names-') as temporary:
        temporary = Path(temporary)
        for resource in sorted((ROOT / 'vanilla-3.0.3/Message').glob('String_*.sarc.zs')):
            archive = temporary / 'strings.sarc'
            archive.write_bytes(zstd.decompress(resource.read_bytes()))
            banks = 0
            for path, data in read_sarc(archive)[1]:
                if Path(path).name not in ('STR_NNpcName.msbt', 'STR_SNpcName.msbt'): continue
                banks += 1
                msbt = temporary / 'names.msbt'
                msbt.write_bytes(data)
                for key, value in read_msbt(msbt):
                    expected = value
                    # Russian grammar tags can occur BETWEEN words (Kid Cat),
                    # not just at the end. Strip only this known resource tag.
                    while '\x0e' in expected:
                        at = expected.index('\x0e')
                        assert expected[at + 1:at + 3] == '2\x16'
                        payload_units = ord(expected[at + 3]) // 2
                        expected = expected[:at] + expected[at + 4 + payload_units:]
                    assert m.label(value) == expected, (resource.name, key, repr(value))
                    count += 1
            assert banks == 2
            locales += 1
    assert locales == 14
    print(f'PASS: {count:,} name cases, both NPC name banks across all {locales} supplied locales; live name-label capture still needs Eden confirmation')


if __name__ == '__main__': main()
