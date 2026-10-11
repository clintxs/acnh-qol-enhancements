"""Check locale-independent first-catch stops through compiled hurry hooks.

Validate stable IDs in every supplied TalkSys locale archive, then execute
native Advance against those active message pairs. Resource text is never
packaged or used by the runtime classifier.
"""
from pathlib import Path
import sys
import tempfile
from compression import zstd
from dialogue_hurry_arm_test import HurryMachine
from unicorn.arm64_const import UC_ARM64_REG_X19, UC_ARM64_REG_W22, UC_ARM64_REG_SP

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from sarc_tool import read_sarc
from msbt_tool import read_msbt

BANKS = ('SYS_Get_Fish', 'SYS_Get_Insect', 'SYS_Get_DiveFish')
LOCALES = {'CNzh', 'EUde', 'EUen', 'EUes', 'EUfr', 'EUit', 'EUnl', 'EUru',
           'JPja', 'KRko', 'TWzh', 'USen', 'USes', 'USfr'}


def catch_resources():
    result = {}
    with tempfile.TemporaryDirectory(prefix='qol-catch-locales-') as temporary:
        sarc, msbt = Path(temporary) / 'talk.sarc', Path(temporary) / 'catch.msbt'
        for archive in sorted((ROOT.parent / 'vanilla-3.0.3/Message').glob('TalkSys_*.sarc.zs')):
            locale = archive.name.removeprefix('TalkSys_').removesuffix('.sarc.zs')
            sarc.write_bytes(zstd.decompress(archive.read_bytes()))
            entries = {}
            for path, data in read_sarc(sarc)[1]:
                bank = Path(path).stem
                if bank not in BANKS: continue
                msbt.write_bytes(data)
                messages = dict(read_msbt(msbt))
                assert messages['001'] and messages['002'] and messages['003'], (locale, bank)
                if locale in ('USen', 'EUen'): assert messages['001'].endswith('Yes!')
                entries[bank] = messages
            assert set(entries) == set(BANKS), locale
            result[locale] = entries
    assert set(result) == LOCALES, set(result)
    return result


def main():
    resources = catch_resources()
    count = 0
    for base in (0x7100000000, 0x7200000000):
        m, native = HurryMachine(base), HurryMachine(base, enabled=False)
        for locale, entries in resources.items():
            for bank in entries:
                for reveal in (2, 3, 4):
                    m.prepare(window=2, reveal=reveal)
                    m.message_id(bank, '001')
                    # Continuous Y cannot consume any page of this ID,
                    # including localized page breaks/input pauses.
                    for _ in range(3):
                        assert m.advance() == 5 and m.request == 0, (locale, bank, reveal)
                        assert m.page_clears == m.notifications == 0
                    # Manual input still uses the original native path.
                    m.write(m.meta + 0x2476, 1, 1)
                    assert m.advance() == (7 if reveal == 3 else 3)
                    assert m.request == 1
                    count += 1
                # The first-catch text itself still uses the 30x tier.
                m.prepare(state=3, window=2)
                m.message_id(bank, '001')
                result, _ = m.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
                assert result[0x21b7] == 5
                # Reuse the same manager and held button for the next ID.
                # Species/pun and completion messages have no sticky stop.
                m.write(m.meta + 0x48, 5, 4)
                m.write(m.obj + 0x21ac, 3, 4)
                for label in ('002', '003', 'Fish_02233', 'Insect_02001', '001_01', '0001'):
                    m.message_id(bank, label)
                    assert m.advance() == 7 and m.request == 1, (locale, bank, label)
                    m.write(m.meta + 0x48, 5, 4)
                    count += 1
        # A choice queued after a first-catch message still needs a manual
        # advance; it then opens normally and held Y cannot select an answer.
        for bank in BANKS:
            m.prepare(choice=True); m.message_id(bank, '001')
            assert m.advance() == 5 and m.request == 0
            m.write(m.meta + 0x2476, 1, 1)
            assert m.advance() == 6 and m.request == 1
            m.write(m.meta + 0x2476, 0, 1)
            assert m.advance() == 6 and m.request == 0
        # Exact bank + label pair only: unrelated 001/"Yes!" messages,
        # missing/empty/truncated IDs must never reuse an old catch match.
        for bank, label in (('SYS_Get_Fish_BeyDailyQ', '001'), ('SYS_Get_Insect_Fish_Fes', '001'),
                            ('SYS_Player', '001'), ('SP_rcm_00_common', '001'),
                            ('SYS_Get_Fish', ''), ('', '001'), ('X' * 64, '001'),
                            ('SYS_Get_Fish', '001' + 'X' * 64)):
            m.prepare(); m.message_id('SYS_Get_Fish', '001')
            assert m.advance() == 5
            m.message_id(bank, label)
            assert m.advance() == 7 and m.request == 1, (bank, label)
            count += 1
        for field in (0x2488, 0x24f8):
            m.prepare(); m.message_id('SYS_Get_Fish', '001')
            assert m.advance() == 5
            m.write(m.meta + field, 0)
            # A null SafeString violates later native close code's contract;
            # execute our callback and displaced load only for this case.
            m.u.reg_write(UC_ARM64_REG_X19, m.meta)
            m.u.reg_write(UC_ARM64_REG_W22, 0)
            m.u.reg_write(UC_ARM64_REG_SP, m.stack + 0x3000)
            m.execute(0x1afa530, (0x1afa534,))
            assert m.u.reg_read(UC_ARM64_REG_W22) == 1
        # Unheld Y and native automatic readiness retain original behavior.
        for phase in (0, 1):
            for buttons in (0, 0x10):
                m.prepare(buttons=buttons, message_phase=phase)
                native.prepare(buttons=buttons, message_phase=phase)
                m.message_id('SYS_Get_Fish', '001'); native.message_id('SYS_Get_Fish', '001')
                assert m.advance() == native.advance()
        print(f'PASS {base:#x}: first-catch ID stops, manual advance, 30x reveal, exact matching and message/choice isolation')
    print(f'PASS: {count} catch scenarios; all 3 first-catch IDs verified in all 14 supplied locales')


if __name__ == '__main__': main()
