"""Run Y hurry through compiled SIMD hooks and the native advance state function.

Controller lookup, renderer page cleanup and final UI notification are modeled.
The native advance branches, choice handoff, close/reset and FSM setter execute.
No game, controller, save or live installation is modified.
"""
import itertools
import struct
from dialogue_arm_test import DialogueMachine, GP, Q
from unicorn.arm64_const import *


class HurryMachine(DialogueMachine):
    def __init__(self, base, enabled=True, verbose=True):
        super().__init__(base, enabled=enabled, verbose=verbose)
        self.message_ui, self.choice_ui = self.ram + 0x6c000, self.ram + 0x6c100
        self.state_table = self.ram + 0x6e000
        self.check_adapter = False
        self.prepare()

    def prepare(self, buttons=0x10, state=5, reveal=3, choice=False, timer=-1,
                choice_phase=0, message_phase=1, window=0, context_phase=2):
        self.u.mem_write(self.meta, bytes(0x2700))
        self.u.mem_write(self.obj, bytes(0x3000))
        self.write(self.meta, self.meta)
        self.write(self.meta + 0x20, self.context)
        self.write(self.meta + 0x28, self.obj)
        self.write(self.obj + 0x208, self.meta)
        self.write(self.obj + 0x210, self.context)
        self.write(self.obj + 0x21ac, reveal, 4)
        self.write(self.context + 8, self.message_ui)
        self.write(self.context + 0x18, self.choice_ui)
        self.write(self.context + 0x28, context_phase, 4)
        self.write(self.context + 0x2c, window, 4)
        self.write(self.message_ui, self.message_ui + 0x200)
        self.write(self.choice_ui, self.choice_ui + 0x500)
        self.write(self.message_ui + 8, message_phase, 4)
        self.write(self.choice_ui + 8, choice_phase, 4)
        self.write(self.meta + 0x48, state, 4)
        # +58 is the native default/resume state, not a pending transition.
        self.write(self.meta + 0x58, 0, 4)
        self.write(self.meta + 0x68, 10, 4)
        self.write(self.meta + 0x70, self.state_table)
        self.write(self.meta + 0x2464, timer & 0xffffffff, 4)
        self.write(self.meta + 0x246c, 0xffffffff, 4)
        self.write(self.meta + 0x2475, int(choice), 1)
        self.write(self.meta + 0x2488, self.ram + 0x6d000)
        self.getter_null = False
        self.write(self.app + 0xf5b20, self.controller)
        self.write(self.controller + 8, buttons, 2)
        self.page_clears, self.notifications = 0, 0

    def service(self, u, pc, size, user):
        if self.native and pc == self.base + 0x2435188:
            # Native calls may clobber caller-saved SIMD registers too.
            for r in Q[:8]: u.reg_write(r, 0xdeadbeef)
        if self.native and self.check_adapter:
            if pc == self.base + 0x1afa530 and self.bypass != pc:
                self.before_gp = [u.reg_read(r) for r in GP]
                self.before_q = [u.reg_read(r) for r in Q]
                self.before_sp = u.reg_read(UC_ARM64_REG_SP)
            elif pc == self.base + 0x1afa534:
                after = [u.reg_read(r) for r in GP]
                for i in range(31):
                    if i not in (11, 22): assert after[i] == self.before_gp[i], (i, after, self.before_gp)
                assert [u.reg_read(r) for r in Q] == self.before_q
                assert u.reg_read(UC_ARM64_REG_SP) == self.before_sp
                self.request = after[22]
        if self.native and pc in (self.base + 0x1afd78c, self.base + 0x1b033dc):
            if pc == self.base + 0x1afd78c:
                assert self.reg(0) == self.obj
                self.page_clears += 1
            else:
                assert self.reg(0) == self.meta
                self.notifications += 1
            for r in GP[:19]: u.reg_write(r, 0xdeadbeef)
            for r in Q[:8]: u.reg_write(r, 0xdeadbeef)
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
            return
        super().service(u, pc, size, user)

    def advance(self):
        self.check_adapter = True
        self.request = None
        controller = bytes(self.u.mem_read(self.controller, 0x100))
        sp = self.stack + 0x3000
        for i, r in enumerate(GP): self.u.reg_write(r, 0xabc00000 + i)
        for i, r in enumerate(Q): self.u.reg_write(r, (i + 1) * 0x1122334455667788)
        saved = [self.u.reg_read(r) for r in GP[19:30]]
        self.u.reg_write(UC_ARM64_REG_X0, self.meta)
        self.u.reg_write(UC_ARM64_REG_SP, sp)
        self.u.reg_write(UC_ARM64_REG_LR, self.base + 0x1234)
        self.execute(0x1afa4a4, (0x1234,))
        assert [self.u.reg_read(r) for r in GP[19:30]] == saved
        assert self.u.reg_read(UC_ARM64_REG_SP) == sp
        assert bytes(self.u.mem_read(self.controller, 0x100)) == controller
        self.check_adapter = False
        return self.read(self.meta + 0x48, 4)


def main():
    count = 0
    for base in (0x7100000000, 0x7200000000):
        m, native = HurryMachine(base), HurryMachine(base, enabled=False)
        for window, reveal, choice, timer, message_phase in itertools.product(range(3), (2, 3, 4), (False, True), (-1, 0), (1, 2)):
            m.prepare(window=window, reveal=reveal, choice=choice, timer=timer, message_phase=message_phase)
            expected = 3 if reveal != 3 else 6 if choice else 7
            assert m.advance() == expected, (window, reveal, choice, timer)
            assert m.request == 1 and m.page_clears == (reveal == 2)
            assert m.notifications == (reveal == 3)
            assert m.read(m.meta + 0x2476, 1) == 0 # No persistent advance press.
            # Keeping Y held cannot advance again after native choice/close handoff.
            assert m.advance() == expected and m.request == 0
            count += 1
        # End flag retains its native state-9 route.
        m.prepare(); m.write(m.meta + 0xac, 1, 1)
        assert m.advance() == 9
        # No hurry across revealing text, transient parser state, authored wait,
        # choice opening/active/closing, unknown message states, or ownership loss.
        cases = ([dict(reveal=r) for r in (0, 1, 5)] +
                 [dict(timer=t) for t in (-2, 1, 2, 60)] +
                 [dict(choice_phase=p) for p in (1, 2, 3)] +
                 [dict(message_phase=p) for p in (3, 4, 0xffffffff)] +
                 [dict(state=s) for s in (0, 3, 4, 6, 7, 9)] +
                 [dict(window=3), dict(context_phase=1)])
        for args in cases:
            m.prepare(**args); native.prepare(**args)
            assert m.advance() == native.advance(), args
            assert m.request == 0, args
            count += 1
        for field, value in ((0x208, 0), (0x210, 0), (0x208, m.meta + 0x3000)):
            m.prepare(); m.write(m.obj + field, value)
            assert m.advance() == 5 and m.request == 0
        for getter_null, fallback_null in ((False, False), (True, False), (True, True)):
            for buttons in (0, 1, 2, 8, 0x10, 0x4000, 0x4008, 0x4010, 0xffff, 0):
                m.prepare(buttons=buttons)
                m.getter_null = getter_null
                m.write(m.app + 0xf5b20, 0 if fallback_null else m.controller)
                hurry = bool(buttons & 0x10) and not (getter_null and fallback_null)
                assert m.advance() == (7 if hurry else 5)
                assert m.request == int(hurry)
                count += 1
        # A real advance latch still works without Y and even while hurry
        # guards reject the frame. Do not clear or consume the player's A.
        for choice_phase in (0, 2):
            m.prepare(buttons=0, choice_phase=choice_phase)
            m.write(m.meta + 0x2476, 1, 1)
            assert m.advance() == 7 and m.request == 1
        # Native idle/automatic pages continue to advance without Y.
        m.prepare(buttons=0, message_phase=0)
        assert m.advance() == 7 and m.request == 0

        # The Sunday turnip price line SP_rcm_00_common/082 contains <10:4>.
        # Execute its native input-pause handler, then Advance and the real
        # Print entry/resume branch. No new press, page clear or end/choice
        # notification is needed, even with a choice queued after the text.
        for choice in (False, True):
            m.prepare(reveal=1, choice=choice)
            m.u.reg_write(UC_ARM64_REG_X0, m.obj)
            m.execute(0x1afe49c, (0x1afe4a4,))
            assert m.read(m.obj + 0x21ac, 4) == 4
            assert m.advance() == 3 and m.request == 1
            assert m.page_clears == m.notifications == 0
            assert m.read(m.obj + 0x21ac, 4) == 4
            assert m.read(m.controller + 8, 2) == 0x10
            m.u.reg_write(UC_ARM64_REG_X0, m.meta)
            m.u.reg_write(UC_ARM64_REG_SP, m.stack + 0x3000)
            m.execute(0x1af9f74, (0x1afb60c,))
            assert m.read(m.obj + 0x21ac, 4) == 1
            assert m.read(m.controller + 8, 2) == 0x10
            result, _ = m.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
            assert result[0x21b7] == 5
            count += 1
        for buttons, choice_phase in ((0, 0), (0x10, 1), (0x10, 2), (0x10, 3)):
            m.prepare(buttons=buttons, reveal=4, choice_phase=choice_phase)
            assert m.advance() == 5 and m.request == 0
            count += 1

        # Real reveal/renderer instructions consume the hurry tier. Release
        # Y returns to the configured tier on the very next print update.
        for buttons, held, flag in ((0x10, 0, 5), (0, 0, 1), (8, 0, 1), (0x4010, 1, 5), (2, 1, 3), (0, 0, 1)):
            m.prepare(buttons=buttons, state=3)
            result, calls = m.run((1, 0, 0, 0, 0, 1, 1, -1.), held)
            assert result[0x21b7] == flag and not calls
            rate = 30 if flag == 5 else 4 if flag == 3 else 2
            assert abs(struct.unpack_from('<f', result, 0x2194)[0] - (-2 + 2.4 * rate / 2)) < 1e-5
            for override, complete in itertools.product((-1., 0., .75), (False, True)):
                m.renderer(flag, override, 2.4, .5, complete)
            result, calls = m.run((1, 0, 0, 0, 0, 0, 0, -1.), held)
            assert result[0x168d] == result[0x1cfd] == flag and calls == [0x1488]
            count += 1
        # Replay live SP_tkk/082 fields from the Oct 11 Eden log. The initial
        # context phase 1 becomes ready=2 while revealing; the completed page
        # then waits in message phase 1. Release must leave that page waiting.
        for phase, expected_flag in ((1, 1), (2, 5)):
            m.prepare(state=3, reveal=1, context_phase=phase, message_phase=0)
            result, _ = m.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
            assert result[0x21b7] == expected_flag
            count += 1
        for buttons, choice, expected in ((0, False, 5), (0x10, False, 7), (0x10, True, 6)):
            m.prepare(buttons=buttons, choice=choice)
            m.u.mem_write(m.ram + 0x6d000, b'SP_tkk\0')
            m.write(m.meta + 0x24f8, m.ram + 0x6d100)
            m.u.mem_write(m.ram + 0x6d100, b'082\0')
            assert m.advance() == expected and m.request == int(bool(buttons))
            if choice: assert m.advance() == 6 and m.request == 0
            count += 1
        for state, mode, forced, disabled, override in itertools.product(
                (0, 1, 2, 3, 4, 5), range(3), (0, 1), (0, 1), (-1., 0., .75)):
            m.prepare(state=3); native.prepare(state=3)
            case = (state, mode, forced, disabled, 9, 1, 1, override)
            eligible = state == 1 and mode == 0 and not forced and not disabled
            if not eligible: assert m.run(case, 0) == native.run(case, 0), case
            elif override >= 0:
                result, calls = m.run(case, 0)
                assert abs(struct.unpack_from('<f', result, 0x2194)[0] - (-2 + override)) < 1e-5 and not calls
            count += 1
        for args in (dict(choice_phase=1), dict(choice_phase=2), dict(choice_phase=3),
                     dict(state=6), dict(context_phase=1), dict(window=3)):
            m.prepare(**args)
            result, _ = m.run((1, 0, 0, 0, 0, 1, 1, -1.), 0)
            assert result[0x21b7] != 5, args
        print(f'PASS {base:#x}: Y hurry native page/choice/end paths, live release/fallback, SIMD preservation and protected text')
    print(f'PASS: {count} hurry scenarios; UI/audio/event execution still requires in-game validation')


if __name__ == '__main__': main()
