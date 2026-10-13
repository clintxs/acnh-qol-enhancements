"""Production registration, request phase and borrowed identity through ARM code.

Execute the real SIMD adapter/native reveal pipeline; input, registry services
and hook allocation remain modeled as in the inherited dialogue fixture.
"""
import struct
from dialogue_arm_test import DialogueMachine, HOOKS
from core_arm_test import Machine, RAW
from unicorn.arm64_const import UC_ARM64_REG_PC, UC_ARM64_REG_LR, UC_ARM64_REG_X0, UC_ARM64_REG_X1

class RequestMachine(DialogueMachine):
    def __init__(self, *args, **kwargs):
        self.rewrite_request = None
        self.requests, self.policy_calls, self.identity_calls = [], 0, 0
        self.custom_selector, self.custom_speed = None, 4
        super().__init__(*args, **kwargs)
        self.policy_entry = self.module.find('SelectConfiguredDialogueSpeed')
    def service(self, u, pc, size, user):
        if pc == self.custom_selector:
            # RevealDecision returns indirectly in x8; all diagnostic fields
            # are value-initialized. Return no retained message/identity borrow.
            u.mem_write(self.reg(8), struct.pack('<I', self.custom_speed) + bytes(44))
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
            return
        if getattr(self, 'native', False):
            if pc == self.base + 0x1afdec0 and self.bypass != pc:
                self.requests.append(self.reg(9))
                if self.rewrite_request is not None:
                    self.write(self.obj + 0x21b4, self.rewrite_request, 1)
            if pc == getattr(self, 'policy_entry', None): self.policy_calls += 1
            if pc == self.is_npc: self.identity_calls += 1
        return super().service(u, pc, size, user)

for base in (0x7100000000, 0x7200000000):
    for pointer, normal, boosted in ((0, 2, 4), (1, 0, 4), (1, 4, 3), (1, 2, 31)):
        m = Machine(base, {})
        m.u.reg_write(UC_ARM64_REG_X0, pointer)
        m.u.reg_write(UC_ARM64_REG_X1, normal | boosted << 32)
        m.call(m.module.find('RegisterFasterDialoguePolicy'))
        assert m.reg(0) == 0 and not m.hooks
    m = RequestMachine(base)
    eligible = (1, 0, 0, 0, 0, 1, 1, -1.)
    m.rewrite_request = 1
    actual, _ = m.run(eligible, 0)
    assert m.requests[-1] == 0 and actual[0x21b7] == 1 # Use the native loaded request, not a later field reread.
    m.rewrite_request = 0
    actual, _ = m.run(eligible, 1)
    assert m.requests[-1] == 1 and actual[0x21b7] == 3
    m.rewrite_request = None
    m.speaker('', window=1)
    m.policy_calls = m.identity_calls = 0
    actual, _ = m.run(eligible, 0)
    assert actual[0x21b7] == 3 and m.read(m.obj + 0x21b4, 1) == 0
    assert m.policy_calls == 1 and m.identity_calls == 0 # UI needs no entity query; automatic boost is not the request.
    if 'DIAGNOSTIC: unnamed-v1' in m.text:
        assert 'SELECT reason=ui-dialog window=1 entity_kind=4294967295 kind=2 npc= speed=4' in m.text
    m.speaker('unlisted')
    actual, _ = m.run(eligible, 0)
    assert actual[0x21b7] == 1 and m.identity_calls == 1
    if 'DIAGNOSTIC: unnamed-v1' in m.text:
        assert 'SELECT reason=npc window=0 entity_kind=10 kind=1 npc=unlisted speed=2' in m.text
    for state, mode, forced, disabled in ((0, 0, 0, 0), (1, 1, 0, 0), (1, 0, 1, 0), (1, 0, 0, 1)):
        before = m.policy_calls
        m.run((state, mode, forced, disabled, 0, 1, 1, -1.), 1)
        assert m.policy_calls == before # Native protection decides whether the policy runs.
    actual, _ = m.run((1, 0, 0, 0, 0, 1, 1, .75), 1)
    assert abs(struct.unpack_from('<f', actual, 0x2194)[0] - (-2 + .75)) < 1e-6
    # Fresh production registration using a temporary by-value declaration.
    # Replace every argument register afterward; later native execution still
    # uses the copied callback/rates. Invalid callback output uses base safely.
    m = RequestMachine(base, enabled=False)
    initial_hooks = set(m.hooks)
    m.custom_selector = m.ram + 0x190000
    m.u.reg_write(UC_ARM64_REG_X0, m.custom_selector)
    m.u.reg_write(UC_ARM64_REG_X1, 2 | 4 << 32)
    m.call(m.module.find('RegisterFasterDialoguePolicy'))
    assert m.reg(0) == 1 and set(m.hooks) - initial_hooks == {base + offset for offset in HOOKS[:-1]}
    m.callbacks = {base + offset: m.hooks[base + offset] for offset in HOOKS if base + offset in m.hooks}
    for offset in HOOKS: m.u.mem_write(base + offset, RAW[offset:offset + 20])
    m.u.reg_write(UC_ARM64_REG_X0, m.custom_selector + 4)
    m.u.reg_write(UC_ARM64_REG_X1, 2 | 4 << 32)
    m.call(m.module.find('RegisterFasterDialoguePolicy'))
    assert m.reg(0) == 0 and len(m.hooks) == len(initial_hooks) + 7
    for selected, flag in ((4, 3), (999, 1), (2, 1)):
        m.custom_speed = selected
        actual, _ = m.run(eligible, 0)
        assert actual[0x21b7] == flag
        expected = -2 + 2.4 * (4 if flag == 3 else 2) / 2
        assert abs(struct.unpack_from('<f', actual, 0x2194)[0] - expected) < 1e-6
    print(f'PASS {base:#x}: production policy registration, copied declarations/conflict rejection, exact native request phase, lazy identity, protected/scripted native paths and invalid-tier fallback')
