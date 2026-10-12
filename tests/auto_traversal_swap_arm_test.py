"""Compiled borrowing/restoration with real native action-data copying.

Inventory lookup, animation transitions and terrain are modeled. The native
RequestAction body is executed, including its complete 0x7a-byte request copy.
The user confirmed silent equipment/restoration working in game.
"""
import struct
from auto_ladder_arm_test import LadderMachine, CONFIG
from auto_vault_arm_test import EXTRA
from core_arm_test import Machine, DEFAULT, RAW
from unicorn.arm64_const import *

SWAP_CONFIG = CONFIG.replace(b'auto_equip_traversal=false', b'auto_equip_traversal=true')
EMPTY, NET, POLE, LADDER, AXE = 0xfffe, 0x1234567800001001, 0x1002, 0x1003, 0x999900001004
KINDS = {EMPTY: 0, NET: 1, POLE: 0x28, LADDER: 0x11, AXE: 3}
DEPENDENCIES = (0x2222950, 0x2272704, 0x2572e00, 0x2577ea0, 0x242d8f0, 0x19d5d04, 0x19d78cc, 0x256a0c0, 0x2277260, 0x23758a0, 0x2375ed0, 0x2376018)

class SwapMachine(LadderMachine):
    def __init__(self, base):
        super().__init__(base, config=SWAP_CONFIG, expected_hooks=9 + EXTRA)
        self.controller = self.ram + 0x16000
        self.write(self.player + 0x70, self.controller)
        self.write(self.controller + 0x280, 0xc, 4)
        self.held_slot = 0
        self.held = NET
        self.tool = KINDS[self.held]
        self.inventory = [NET, POLE, LADDER, AXE]
        self.pocket_pointer = self.ram + 0x17000
        self.action_requests = []
        self.dispatch_original = self.ram + 0xd0c0
        self.write(self.module.find('AutoTraversalSwap', 'OrigRef'), self.dispatch_original)
        self.dispatch_result = 0
        self.enter_original = self.ram + 0xd0d0
        self.write(self.module.find('AutoTraversalActionFinished', 'OrigRef'), self.enter_original)
        self.ladder_valid = False
        self.ladder_result = False
        self.u.mem_write(self.player+0x18e0, bytes(range(32)))

    def service(self, u, pc, size, context):
        if hasattr(self, 'action_requests'):
            if pc == self.base + 0x2275730 and getattr(self, 'native_idle', False):
                self.idle_order.append('begin')
                result = 0
            elif pc == self.base + 0x226ac64 and getattr(self, 'native_idle', False):
                self.idle_order.append('setup')
                result = 0
            elif pc == self.base + 0x2277260 and getattr(self, 'native_idle', False):
                assert self.idle_order == ['begin', 'setup']
                u.reg_write(UC_ARM64_REG_PC, self.module.find('AutoTraversalActionFinished', 'Callback'))
                return
            elif pc == self.base + 0x242d8f0:
                assert self.read(self.reg(0), 1) == 0
                self.write(self.reg(8), self.held)  # Native Item result ABI: X8.
                result = 0
            elif pc == self.base + 0x256a0c0:
                result = self.reg(2) == self.held_slot and self.read(self.reg(1)) == self.held
            elif pc == self.base + 0x2572e00:
                result = len(self.inventory)
            elif pc == self.base + 0x2577ea0:
                self.write(self.pocket_pointer, self.inventory[self.reg(1)])
                result = self.pocket_pointer
            elif pc == self.base + 0x19d5d04:
                self.write(self.reg(0)+8, self.read(self.reg(1)))
                result = self.reg(0)
            elif pc == self.base + 0x19d78cc:
                result = KINDS.get(self.read(self.reg(0)+8), 0)
            elif pc == self.base + 0x3dbb160:
                assert not getattr(self, 'inside_entry', False) or self.entry_finalized
                assert self.reg(0) == self.controller + 0x278 and self.reg(1) == 0x29
                data = bytes(u.mem_read(self.player + 0x1860, 0x7a))
                assert data[:32] == bytes(range(32))
                item = struct.unpack_from('<Q', data, 32)[0]
                slot = struct.unpack_from('<b', data, 0x29)[0]
                assert data[0x28] == 0 and data[0x2a] == 1 and data[0x2e] == 0
                self.action_requests.append((item, slot))
                self.write(self.controller + 0x280, 0x29, 4)
                result = 0
            elif pc == self.enter_original:
                self.entry_finalized = True
                result = 0
            elif pc == self.dispatch_original:
                result = self.dispatch_result
            else:
                return super().service(u, pc, size, context)
            u.reg_write(UC_ARM64_REG_X0, result)
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
            return
        return super().service(u, pc, size, context)

    def dispatch(self):
        self.u.reg_write(UC_ARM64_REG_X0, self.player)
        self.call(self.module.find('AutoTraversalSwap', 'Callback'))
        return self.reg(0)

    def enter(self, action):
        self.entry_finalized = False
        self.inside_entry = True
        self.write(self.controller+0x280, action, 4)
        self.u.reg_write(UC_ARM64_REG_X0, self.player)
        self.u.reg_write(UC_ARM64_REG_X1, action)
        self.call(self.module.find('AutoTraversalActionFinished', 'Callback'))
        self.inside_entry = False

    def enter_idle(self):
        # Execute the real native idle-entry wrapper, including its tail call
        # to the completed-entry hook. Only native setup services are modeled.
        self.native_idle = self.inside_entry = True
        self.entry_finalized = False
        self.idle_order = []
        self.write(self.controller+0x280, 5, 4)
        self.u.reg_write(UC_ARM64_REG_X0, self.player)
        self.call(self.base + 0x228f9d8)
        assert self.entry_finalized and self.idle_order == ['begin', 'setup']
        self.native_idle = self.inside_entry = False

    def finish_change(self):
        self.enter(0x29)
        self.held = self.action_requests[-1][0]
        self.tool = KINDS[self.held]
        self.enter(0xc)

    def begin(self, ladder=False):
        self.ladder_valid = self.ladder_result = ladder
        self.vaultable = not ladder
        self.write(self.player + 0x6a0, 31, 2)
        assert self.automatic(0, prime=False) == 1
        assert self.action_requests[-1] == ((LADDER, 2) if ladder else (POLE, 1))
        self.finish_change()


def main():
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        # Omitted keys now activate all traversal paths; explicit custom false
        # still disables them without disabling other Controls features.
        for custom, expected in ((None, 20 + EXTRA),
                                 (b'auto_vault=false\nauto_ladder=false\nauto_equip_traversal=false', 13)):
            files = {'controls': DEFAULT + b''}
            if custom is not None: files['controls.custom'] = custom
            defaults = Machine(base, files)
            defaults.boot(); defaults.activate()
            assert len(defaults.hooks) == expected, defaults.text
            assert ('auto placed ladder: READY;' in defaults.text) == (custom is None)
            assert ('auto equip: READY;' in defaults.text) == (custom is None)
            cases += 1
        for ladder in (False, True):
            for prior in (NET, AXE, EMPTY):
                m = SwapMachine(base)
                m.held, m.tool = prior, KINDS[prior]
                m.held_slot = {NET:0, AXE:3, EMPTY:-1}[prior]
                m.begin(ladder)
                # Wait through equip animation; no traversal/restoration yet.
                m.write(m.controller+0x280, 0x29, 4)
                assert m.dispatch() == 0 and len(m.action_requests) == 1
                m.write(m.controller+0x280, 0xc, 4)
                assert m.dispatch() == 1
                assert len(m.action_requests) == 1
                # Old Move while the native traversal is still queued must
                # neither restore nor request a second traversal.
                assert m.dispatch() == 0 and len(m.action_requests) == 1
                # Traverse action must finish before restoration, even if
                # input is released during the animation.
                for action in ((0x8a,0x8b,0x8c) if ladder else (0xb4,0xb5,0xb6)):
                    m.enter(action)
                    assert m.dispatch() == 0
                    cases += 1
                m.stick(0.,0.)
                m.enter(0xc)
                assert len(m.action_requests) == 2  # Restored by entry, no dispatch needed.
                assert m.dispatch() == 0
                assert m.action_requests[-1] == (prior, {NET:0, AXE:3, EMPTY:-1}[prior])
                m.finish_change()
                assert m.dispatch() == 0 and len(m.action_requests) == 2
                cases += 1
        # Abort before traversal restores the prior item; manual tool selection
        # and a new player identity instead cancel our ownership.
        for reason in ('release', 'turn', 'move', 'blocked', 'manual', 'identity', 'remove', 'reorder', 'failure'):
            m = SwapMachine(base); m.begin()
            if reason == 'release': m.stick(0.,0.)
            if reason == 'turn': m.write(m.actor+0x2b4, 100, 4)
            if reason == 'move': m.u.mem_write(m.actor+0x278, struct.pack('<f',1.))
            if reason == 'blocked': m.vaultable = False
            if reason == 'manual': m.held, m.tool = AXE, KINDS[AXE]
            if reason == 'identity': m.write(m.player+0x230, 99, 4)
            if reason == 'remove': m.inventory[0] = EMPTY; m.stick(0.,0.)
            if reason == 'reorder': m.inventory[0], m.inventory[3] = m.inventory[3], m.inventory[0]; m.stick(0.,0.)
            if reason == 'failure': m.commit_ok = False
            result = m.dispatch()
            assert result == (0 if reason in ('manual','identity') else 1), reason
            if reason in ('manual','identity'): assert len(m.action_requests) == 1
            else: assert m.action_requests[-1] == ((EMPTY,-1) if reason == 'remove' else (NET,3) if reason == 'reorder' else (NET,0)), reason
            cases += 1
        # Native hops win, tools must exist, temporary non-pocket held items
        # are left alone, strong intent and exact movement proof still apply.
        for reason in ('no-pole', 'ordinary-hop', 'weak', 'temporary', 'native-ladder-error', 'changed-held'):
            m = SwapMachine(base)
            if reason == 'no-pole': m.inventory[1] = EMPTY
            if reason == 'ordinary-hop': m.hop_result = 1
            if reason == 'weak': m.stick(0.,.5)
            if reason == 'temporary': m.inventory[0] = EMPTY
            if reason == 'native-ladder-error': m.inventory[1] = EMPTY; m.ladder_result=True; m.ladder_valid=False
            if reason == 'changed-held':
                assert m.automatic(0, prime=False) == 2
                m.held, m.tool = AXE, KINDS[AXE]
                assert m.automatic(1, prime=False) == 0
                cases += 1; continue
            m.write(m.player+0x6a0,31,2)
            assert m.automatic(0, prime=False) == (1 if reason == 'ordinary-hop' else 0), reason
            assert m.action_requests == [], reason
            cases += 1
        # Identical items in separate slots restore the original slot.
        m = SwapMachine(base); m.inventory[3] = NET; m.held_slot = 3
        m.begin(); m.stick(0.,0.); assert m.dispatch() == 1
        assert m.action_requests[-1] == (NET, 3)
        cases += 1
        # Either equipped traversal tool can borrow the other one.
        for held, ladder in ((LADDER,False),(POLE,True)):
            m = SwapMachine(base); m.held, m.tool = held, KINDS[held]
            m.held_slot = 2 if held == LADDER else 1
            m.begin(ladder); m.stick(0.,0.); assert m.dispatch() == 1
            assert m.action_requests[-1] == (held, m.held_slot)
            cases += 1
        for ladder in (False,True):
            for facing in (0,0x40000000,0x80000000,0xc0000000):
                m = SwapMachine(base)
                m.direction = m.input_direction = facing
                m.write(m.actor+0x2b4,facing,4)
                m.begin(ladder)
                assert m.read(m.actor+0x2b4,4) == facing
                assert m.dispatch() == 1
                assert m.dispatch() == 0 and len(m.action_requests) == 1
                m.enter(0x8a if ladder else 0xb4)
                m.enter(0xc)
                assert m.dispatch() == 0 and m.action_requests[-1] == (NET,0)
                cases += 1
        # Landing into idle restores without any walking dispatcher, even if
        # input is unavailable. Intermediate/queued actions never restore.
        for ladder in (False, True):
            for prior in (NET, EMPTY):
                for changed in (False, True):
                    m = SwapMachine(base)
                    m.held, m.tool = prior, KINDS[prior]
                    m.held_slot = 0 if prior == NET else -1
                    m.begin(ladder)
                    assert m.dispatch() == 1
                    m.enter(5)  # No real traversal entry yet: queued gap.
                    assert len(m.action_requests) == 1
                    m.enter(0x8a if ladder else 0xb4)
                    m.stick(0., 0.)
                    m.write(m.player+0x12f8, 0, 1)
                    if changed: m.held, m.tool = AXE, KINDS[AXE]
                    m.enter_idle()  # Real idle wrapper completes, no dispatch.
                    assert len(m.action_requests) == (1 if changed else 2)
                    if not changed: assert m.action_requests[-1] == (prior, 0 if prior == NET else -1)
                    m.enter(5)  # Once only; no repeated restoration.
                    assert len(m.action_requests) == (1 if changed else 2)
                    cases += 1
        for animate in (0,1):
            m = SwapMachine(base)
            m.write(m.player+0x190e, animate, 1)
            m.u.reg_write(UC_ARM64_REG_X23, m.player+0x190b)
            m.u.emu_start(base+0x2376018, base+0x237617c, count=2)
            assert m.u.reg_read(UC_ARM64_REG_PC) == base+(0x237617c if animate == 0 else 0x2376020)
            cases += 1
        # A queued equip that hasn't entered/exited ToolChange is not ready.
        m = SwapMachine(base)
        m.write(m.player+0x6a0,31,2); assert m.automatic(0, prime=False) == 1
        m.write(m.controller+0x280,0xc,4)
        assert m.dispatch() == 0 and len(m.action_requests) == 1
        m.finish_change(); assert m.dispatch() == 1
        cases += 1
        for dependency in DEPENDENCIES:
            config = DEFAULT + b'hold_to_hoover=false\nhold_to_advance_dialogue=false\nauto_vault=true\nauto_ladder=true\nauto_equip_traversal=true\n'
            n = Machine(base, {'controls':config})
            n.u.mem_write(base+dependency, b'\0'*4); n.boot(); n.activate()
            assert base+0x2222950 not in n.hooks
            assert base+0x230b8d0 in n.hooks and base+0x2396bfc in n.hooks
            assert 'auto equip: DISABLED' in n.text
            cases += 1
    print(f'PASS: {cases} auto equipment, traversal, restoration and fingerprint cases at two ASLR bases')

if __name__ == '__main__': main()
