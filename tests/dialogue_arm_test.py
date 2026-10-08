"""Run compiled C++ hooks through the real SIMD adapter and native dialogue code.

Original reveal/renderer/input instructions execute locally. Message metadata,
parser completion and speaker delegates are modeled; this is not an Eden test.
"""
import itertools
import struct
from core_arm_test import Machine, RAW, DEFAULT
from unicorn.arm64_const import *

HOOKS = (0x1afa00c, 0x1afdec0, 0x1afded4, 0x1afdefc, 0x1a969e0)
CUSTOM = b"base_speed=2\nboost_speed=4\nfaster_ui=true\nfaster_player=true\nfaster_npc_list=Timmy,Tommy,Orville,Wilbur,Kapp'n\n"
GP = [globals()[f'UC_ARM64_REG_X{i}'] for i in range(31)]
Q = [globals()[f'UC_ARM64_REG_Q{i}'] for i in range(32)]

class DialogueMachine(Machine):
    def __init__(self, base, custom=None, enabled=True):
        self.native = False
        files = {'faster-dialogue': DEFAULT} if enabled else {}
        if custom is not None: files['faster-dialogue.custom'] = custom
        super().__init__(base, files)
        self.boot(); self.activate()
        self.callbacks = {base + offset: self.hooks[base + offset] for offset in HOOKS} if enabled else {}
        # Model trampoline relocation by executing pristine displaced code after
        # the actual compiled adapter/callback. The hook allocator is modeled.
        for offset in HOOKS: self.u.mem_write(base + offset, RAW[offset:offset + 20])
        self.adapter = self.module.find('exl_inline_float_hook_impl')
        self.obj, self.meta, self.stack = self.ram + 0x10000, self.ram + 0x14000, self.ram + 0x80000
        self.controller, self.app = self.ram + 0x15000, self.ram + 0xa0000
        self.context, self.source = self.ram + 0x60000, self.ram + 0x61000
        self.entity, self.table, self.entries = self.ram + 0x62000, self.ram + 0x63000, self.ram + 0x64000
        self.write(self.meta + 0x20, self.context)
        self.write(self.meta + 0x28, self.obj)
        self.write(self.meta + 0xc0, self.source)
        self.write(base + 0x4ab27c0, self.ram + 0x65000)
        self.write(self.ram + 0x65000, self.table)
        self.write(self.table + 0x1c0, self.table + 0x100)
        self.write(self.table + 0x108, self.entries)
        self.handle = 0x00100042
        self.write(self.entries + 8, self.entity)
        self.write(self.source + 0x28, self.source + 0x100)
        self.valid, self.resolve = base + 0x1b00de4, base + 0x1b00dfc
        self.write(self.source + 0x100, self.resolve)
        self.write(self.source + 0x110, self.valid)
        self.is_npc = base + 0x1b00e70
        self.write(self.entity, self.entity + 0x900)
        self.write(self.entity + 0x920, self.is_npc)
        self.write(base + 0x4ab37d0, self.entity + 0xa00)
        self.write(base + 0x4ab37d8, self.entity + 0xb00)
        self.write(self.entity + 0xb00, 1, 1)
        self.npc_type_matches = True
        self.write(base + 0x4ab3960, self.ram + 0x66000)
        self.write(self.ram + 0x66000, self.app)
        self.return_adapter = self.ram + 0x400
        self.pending, self.bypass = None, None
        self.entry_stop = set()
        self.fast = 2.4
        self.speaker('rcm', source=False)
        self.bank_safe, self.bank_label = self.ram + 0x67000, self.ram + 0x67100
        self.bank_methods, self.pending_bank = self.ram + 0x67200, self.ram + 0x67300
        self.get_bank, self.get_label, self.ensure_bank = base + 0x1b00f00, base + 0x1b00f04, base + 0x1b00f08
        self.write(self.meta + 0x170, self.bank_methods)
        self.write(self.bank_methods + 0x10, self.get_bank)
        self.write(self.bank_methods + 0x18, self.get_label)
        self.write(self.bank_safe, self.bank_methods + 0x100)
        self.write(self.bank_methods + 0x118, self.ensure_bank)
        self.write(self.bank_safe + 8, self.pending_bank)
        self.write(self.meta + 0x2488, self.meta + 0x2494)
        self.write(self.meta + 0x2490, 64, 4)
        self.native = True

    def speaker(self, name='', kind=10, window=0, source=True, valid=True, stale=False, dead=False):
        self.write(self.context + 0x2c, window, 4)
        self.write(self.meta + 0xc0, self.source if source else 0)
        self.write(self.entity + 0x230, self.handle + int(stale), 4)
        self.write(self.entity + 0x15c, kind, 4)
        self.write(self.entity + 0x248, 2 if dead else 0, 4)
        self.write(self.entity + 0x3e8, self.entity + 0x800)
        self.u.mem_write(self.entity + 0x800, name.encode() + b'\0')
        self.delegate_valid = valid
        self.delegate_calls = 0

    def service(self, u, pc, size, user):
        if not self.native:
            return super().service(u, pc, size, user)
        if pc == self.return_adapter:
            native_pc, sp = self.pending
            assert u.reg_read(UC_ARM64_REG_SP) == sp
            u.reg_write(UC_ARM64_REG_LR, self.read(sp - 0x10))
            self.pending, self.bypass = None, native_pc
            u.reg_write(UC_ARM64_REG_PC, native_pc)
            return
        if pc in self.callbacks:
            if self.bypass == pc: self.bypass = None
            else:
                assert self.pending is None
                sp = u.reg_read(UC_ARM64_REG_SP)
                self.pending = (pc, sp)
                # Same entry semantics as inline_impl.cpp: saved LR at SP-16,
                # adapter LR points eight bytes before its callback pointer.
                self.write(sp - 0x10, u.reg_read(UC_ARM64_REG_LR))
                self.write(self.return_adapter + 8, self.callbacks[pc])
                u.reg_write(UC_ARM64_REG_LR, self.return_adapter)
                u.reg_write(UC_ARM64_REG_PC, self.adapter)
                return
        relative = pc - self.base
        if relative < 0x3f07000: self.trace.append(relative)
        if relative in self.entry_stop or relative in (0x1afdd80, 0x1afe064):
            self.stop = relative
            u.emu_stop()
        elif pc in (self.valid, self.resolve, self.is_npc):
            if pc == self.is_npc:
                assert self.reg(0) == self.entity and self.reg(1) == self.entity + 0xa00
            if pc == self.resolve:
                self.delegate_calls += 1
                u.mem_write(self.reg(1), struct.pack('<III', self.handle, 0, 0))
            for reg in GP[1:19]: u.reg_write(reg, 0xdeadbeef)
            for reg in Q[:8]: u.reg_write(reg, 0xdeadbeef)
            u.reg_write(UC_ARM64_REG_X0, int(self.npc_type_matches if pc == self.is_npc else self.delegate_valid))
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
        elif pc in (self.get_bank, self.get_label, self.ensure_bank):
            if pc == self.get_bank: u.reg_write(UC_ARM64_REG_X0, self.bank_safe)
            elif pc == self.get_label: u.reg_write(UC_ARM64_REG_X0, self.bank_label)
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
        elif relative == 0x3f04470:
            u.mem_write(self.reg(0), bytes(u.mem_read(self.reg(1), self.reg(2))))
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
        elif relative == 0x2435188:
            u.reg_write(UC_ARM64_REG_X0, 0 if self.getter_null else self.controller)
            for reg in GP[1:19]: u.reg_write(reg, 0xdeadbeef)
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
        elif relative == 0x1a97520:
            self.write(self.reg(0) + 12, self.mode, 4)
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
        elif relative == 0x5f1490:
            self.calls.append(self.reg(0) - self.obj)
            self.write(self.obj + 0x168c, 1, 1)
            self.write(self.obj + 0x1cfc, 1, 1)
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
        else:
            super().service(u, pc, size, user)

    def execute(self, start, stops):
        self.trace, self.calls, self.stop = [], [], None
        self.entry_stop = set(stops)
        self.u.emu_start(self.base + start, self.base + 0x1afe12c, count=30000)
        assert self.stop is not None, (hex(self.u.reg_read(UC_ARM64_REG_PC)), start)
        assert self.pending is None

    def run(self, case, held):
        state, self.mode, instant, disabled, timer, a1, a2, override = case
        memory = bytearray(b'\xa5' * 0x3000)
        for offset, fmt, value in ((0x208, '<Q', self.meta), (0x21ac, '<I', state),
                (0x21a8, '<I', timer), (0x2190, '<I', 3), (0x2194, '<f', 1.),
                (0x2198, '<f', 1.), (0x219c, '<f', 1.2), (0x21a0, '<f', self.fast),
                (0x21a4, '<f', override), (0x1690, '<I', 7), (0x1694, '<f', 7.5)):
            struct.pack_into(fmt, memory, offset, value)
        for offset, value in ((0x21b4, held), (0x21b5, instant), (0x21b6, disabled),
                              (0x168c, a1), (0x1cfc, a2)):
            memory[offset] = value
        self.u.mem_write(self.obj, bytes(memory))
        self.write(self.meta + 0x8c, 0, 4)
        self.u.mem_write(self.stack, b'\0' * 0x4000)
        self.u.reg_write(UC_ARM64_REG_X0, self.obj)
        self.u.reg_write(UC_ARM64_REG_SP, self.stack + 0x3000)
        self.execute(0x1afdd20, ())
        result = bytearray(self.u.mem_read(self.obj, len(memory)))
        result[0x21b4] = 0
        return bytes(result), self.calls

    def bank(self, name, native=True):
        self.u.mem_write(self.pending_bank, name.encode() + b'\0')
        if native:
            # Execute the game's actual pending-bank -> active-bank copy. This
            # tests the +2488 field contract instead of populating it by hand.
            self.u.reg_write(UC_ARM64_REG_X19, self.meta)
            self.u.reg_write(UC_ARM64_REG_SP, self.stack + 0x3000)
            self.execute(0x1af89a0, (0x1af8a88,))
            assert self.string(self.meta + 0x2494) == name[:63]

    def input(self, buttons, previous=0, getter_null=False, fallback_null=False):
        self.getter_null = getter_null
        self.write(self.obj + 0x21b4, previous, 1)
        self.write(self.controller + 8, buttons, 2)
        before = bytes(self.u.mem_read(self.controller, 0x100))
        self.write(self.app + 0xf5b20, 0 if fallback_null else self.controller)
        self.u.reg_write(UC_ARM64_REG_X0, self.meta)
        self.u.reg_write(UC_ARM64_REG_SP, self.stack + 0x3000)
        self.execute(0x1af9fb0, (0x1afa010,))
        assert self.read(self.obj + 0x21b4, 1) == (bool(buttons & 0x4002) and not (getter_null and fallback_null))
        assert bytes(self.u.mem_read(self.controller, 0x100)) == before

    def renderer(self, flags, override, fast, scale, complete):
        obj = self.obj + 0x3000
        memory = bytearray(0x680)
        memory[0x204], memory[0x205] = complete, flags
        for offset, fmt, value in ((0x208, '<I', 3), (0x20c, '<f', 1.), (0x210, '<f', scale),
                (0x214, '<f', 1.2), (0x218, '<f', fast), (0x21c, '<f', override)):
            struct.pack_into(fmt, memory, offset, value)
        self.u.mem_write(obj, bytes(memory))
        self.u.reg_write(UC_ARM64_REG_X0, obj)
        self.u.reg_write(UC_ARM64_REG_SP, self.stack + 0x3000)
        self.u.reg_write(UC_ARM64_REG_LR, self.base + 0x1234)
        self.execute(0x1a969c0, (0x1234,))
        selected = override if override >= 0 else fast * (2 if flags == 3 else 1) if flags else 1.2
        expected = 1. if complete else -2. + selected * scale
        assert abs(struct.unpack('<f', self.u.mem_read(obj + 0x20c, 4))[0] - expected) < 1e-5
        assert self.read(obj + 0x205, 1) == flags
        assert self.u.reg_read(UC_ARM64_REG_SP) == self.stack + 0x3000


def normalized(memory):
    result = bytearray(memory)
    for offset in (0x21b7, 0x168d, 0x1cfd): result[offset] = bool(result[offset])
    return result


def main():
    total = 0
    for base in (0x7100000000, 0x7200000000):
        m, native = DialogueMachine(base), DialogueMachine(base, enabled=False)
        for case in itertools.product(range(6), range(3), range(2), range(2), (0, 2, 9),
                                      range(2), range(2), (-1., 0., .75)):
            expected, calls = native.run(case, 1)
            actual, actual_calls = m.run(case, 0)
            assert actual == expected and actual_calls == calls, (case, 'base')
            boosted, boost_calls = m.run(case, 1)
            state, mode, instant, disabled, timer, a1, a2, override = case
            eligible = state == 1 and mode == 0 and not instant and not disabled
            assert boost_calls == calls, (case, 'extra parsing')
            if eligible:
                assert boosted[0x21b7] == 3
                adjusted = bytearray(boosted)
                if a1 and a2 and override < 0:
                    progress = struct.unpack_from('<f', boosted, 0x2194)[0]
                    assert abs(progress - 2.8) < 1e-5, (case, progress)
                    adjusted[0x2194:0x2198] = expected[0x2194:0x2198]
                assert normalized(adjusted) == normalized(expected), case
            else: assert boosted == expected, case
            total += 1
        for buttons, previous, fallback in itertools.product((0, 1, 2, 3, 0x2000, 0x4000, 0x4002, 0xffff),
                (0, 1), ((False, False), (True, False), (True, True))):
            m.input(buttons, previous, *fallback); total += 1
        for args in itertools.product((0, 1, 3), (-1., 0., .75), (0., .6, 1.2, 2., 2.4), (.5, 1.), (False, True)):
            m.renderer(*args); total += 1
        for fast in (0., .6, 1.2, 2., 2.4):
            m.fast = fast
            for held in (0, 1):
                result, _ = m.run((1, 0, 0, 0, 0, 1, 1, -1.), held)
                assert abs(struct.unpack_from('<f', result, 0x2194)[0] - (-2 + fast * (2 if held else 1))) < 1e-5
                total += 1
        custom = DialogueMachine(base, CUSTOM)
        for name, kind, window, source, valid, stale, dead, tier in (
                ('rcm',10,0,True,True,False,False,3), ('rct',10,0,True,True,False,False,3),
                ('dod',10,0,True,True,False,False,3), ('doc',10,0,True,True,False,False,3),
                ('kpp',10,0,True,True,False,False,3), ('rco',10,0,True,True,False,False,1),
                ('kppB',10,0,True,True,False,False,3),
                ('cat00',10,0,True,True,False,False,1), ('',0x4f,0,True,True,False,False,3),
                ('rcm',10,1,True,True,False,False,3), ('',10,1,False,True,False,False,3),
                ('rcm',10,0,False,True,False,False,1), ('rcm',10,2,True,True,False,False,1),
                ('rcm',10,0,True,False,False,False,1), ('rcm',10,0,True,True,True,False,1),
                ('rcm',10,0,True,True,False,True,1), ('rcm',0x85,0,True,True,False,False,1),
                ('rcmxxxxxxxxxxxxxxxxx',10,0,True,True,False,False,1)):
            custom.speaker(name, kind, window, source, valid, stale, dead)
            before = bytes(custom.u.mem_read(custom.context, 0x6000))
            for held in (0, 1, 0):
                result, _ = custom.run((1,0,0,0,0,1,1,-1.), held)
                expected_tier = 3 if held else tier
                assert result[0x21b7] == expected_tier, (name, kind, window, held, result[0x21b7])
                assert abs(struct.unpack_from('<f', result, 0x2194)[0] - (-2 + 2.4 * (2 if expected_tier == 3 else 1))) < 1e-5
                assert bytes(custom.u.mem_read(custom.context, 0x6000)) == before
                total += 1
        custom.speaker('rcm')
        for fault in ('type_mismatch', 'type_unready', 'method_missing', 'method_foreign'):
            custom.npc_type_matches = fault != 'type_mismatch'
            custom.write(custom.entity + 0xb00, int(fault != 'type_unready'), 1)
            method = 0 if fault == 'method_missing' else custom.ram if fault == 'method_foreign' else custom.is_npc
            custom.write(custom.entity + 0x920, method)
            for held in (0, 1):
                result, _ = custom.run((1,0,0,0,0,1,1,-1.), held)
                assert result[0x21b7] == (3 if held else 1), fault
                total += 1
        # The self category must work with no entity, and be independent of UI
        # and NPC settings. Pending next-message IDs must not change this one.
        for prefs, player_boost, ui_boost in ((b'faster_player=true', True, False),
                (b'faster_ui=true', False, True), (b'faster_player=false', False, False)):
            player = DialogueMachine(base, prefs)
            player.speaker(source=False)
            for bank in ('FTR_WorkingTable', 'SYS_Get_Fish', 'SYS_Get_Insect', 'SYS_Get_DiveFish',
                         'SYS_Player', 'TalkSys/SYS_Player.msbt', 'SP_rcm_00_common',
                         'SYS_Door', 'SYS_Get_Fish_Extra', ''):
                player.bank(bank)
                before = bytes(player.u.mem_read(player.meta, 0x2700))
                for held in (0, 1, 0):
                    result, _ = player.run((1,0,0,0,0,1,1,-1.), held)
                    recognized = bank in ('FTR_WorkingTable', 'SYS_Get_Fish', 'SYS_Get_Insect',
                                         'SYS_Get_DiveFish', 'SYS_Player', 'TalkSys/SYS_Player.msbt')
                    assert result[0x21b7] == (3 if held or recognized and player_boost else 1), (prefs, bank)
                    assert bytes(player.u.mem_read(player.meta, 0x2700)) == before
                    total += 1
            player.bank('SYS_Player', native=False)
            result, _ = player.run((1,0,0,0,0,1,1,-1.), 0)
            assert result[0x21b7] == 1 # Pending bank ignored; active empty bank.
            player.bank('SYS_Player')
            player.bank('SP_rcm_00_common', native=False)
            result, _ = player.run((1,0,0,0,0,1,1,-1.), 0)
            assert result[0x21b7] == (3 if player_boost else 1)
            player.speaker(window=1, source=False)
            result, _ = player.run((1,0,0,0,0,1,1,-1.), 0)
            assert result[0x21b7] == (3 if player_boost else 1) # Known self bank wins over blue color.
            player.speaker(source=False)
            player.bank('SP_rcm_00_common')
            result, _ = player.run((1,0,0,0,0,1,1,-1.), 0)
            assert result[0x21b7] == 1 # No player boost leaks to next NPC message.
            player.speaker(window=1, source=False)
            result, _ = player.run((1,0,0,0,0,1,1,-1.), 0)
            assert result[0x21b7] == (3 if ui_boost else 1)
            player.speaker(kind=0x4f)
            result, _ = player.run((1,0,0,0,0,1,1,-1.), 0)
            assert result[0x21b7] == (3 if player_boost else 1)
            player.speaker(source=False)
            player.u.mem_write(player.meta + 0x2494, b'A' * 53 + b'/SYS_Player\0')
            result, _ = player.run((1,0,0,0,0,1,1,-1.), 0)
            assert result[0x21b7] == 1 # A known basename beyond the bound cannot match.
            total += 7
        assert 'base=2 boost=4 faster_ui=true faster_player=true npc_count=6' in custom.text
        if 'SELECT reason=' in custom.text:
            # Opening UI often lasts hundreds of eligible frames. Repeated UI
            # must not exhaust diagnostics before the first NPC conversation.
            probe = DialogueMachine(base, CUSTOM)
            probe.speaker(window=1, source=False)
            for _ in range(140): probe.run((1,0,0,0,0,1,1,-1.), 0)
            assert probe.text.count('SELECT reason=ui-dialog') == 1
            probe.speaker('rcm')
            probe.run((1,0,0,0,0,1,1,-1.), 0)
            assert 'SELECT reason=npc window=0 entity_kind=10 kind=1 npc=rcm speed=4' in probe.text
            probe.write(probe.entity + 0xb00, 0, 1)
            probe.run((1,0,0,0,0,1,1,-1.), 0)
            assert 'SELECT reason=npc-type-unready' in probe.text
            total += 142
            print('PASS: bounded transition diagnostics survive repeated UI and identify NPC lookup failures')
        print(f'PASS {base:#x}: compiled SIMD adapter, native reveal/input/renderers, speaker switches, UI/NPC classification, held/release, dynamic rates')
    absent = DialogueMachine(0x7100000000, CUSTOM, enabled=False)
    assert len(absent.opened) == 5 and 'faster-dialogue.custom.ini' not in ''.join(absent.opened)
    print(f'PASS: {total:,} ARM cases; original parser/audio/emote boundaries retained, live-game testing pending')

if __name__ == '__main__': main()
