"""Compiled camera policy around the original ACNH eligibility function.

Hook installation is modeled; native action snapshots, lock checks and return
paths execute unchanged. Actual camera animation/right-stick feel needs Eden.
"""
from core_arm_test import Machine, DEFAULT, RAW
from auto_swim_arm_test import BINDINGS
from unicorn.arm64_const import *

ELIGIBILITY = 0x241dab4
FINGERPRINTS = (ELIGIBILITY, 0x241dac4, 0x241dae4, 0x241db08, 0x241dc6c)
CONFIG = DEFAULT + b'hold_to_hoover=false\nhold_to_advance_dialogue=false\nauto_vault=false\nauto_ladder=false\n'
CAMERA_ONLY = CONFIG + b'auto_swim=false\n'

class CameraMachine(Machine):
    def __init__(self, base):
        super().__init__(base, {'controls': CAMERA_ONLY})
        self.boot(); self.activate()
        assert len(self.hooks) == 3 and base+ELIGIBILITY in self.hooks, self.text
        self.callback = self.module.find('SwimmingCameraTiltEligibility', 'Callback')
        # The harness records installs rather than relocating originals. Restore
        # the native entry and let the compiled wrapper call that original code.
        self.u.mem_write(base+ELIGIBILITY, RAW[ELIGIBILITY:ELIGIBILITY+20])
        self.write(self.module.find('SwimmingCameraTiltEligibility', 'OrigRef'), base+ELIGIBILITY)
        self.service_address = self.ram+0x10000
        self.write(base+0x4b487d4, 23, 2)
        self.write(base+0x53f08d8, 1, 1)  # Fallback flags already initialized.
        self.write(base+0x53f0c70, 1, 1)  # Fallback snapshot already initialized.
    def prepare(self, action, flags=0, resident=0, buffer=0, valid=1, kind=23, service=True):
        self.write(self.base+0x53f5738, self.service_address if service else 0)
        self.write(self.base+0x538e140, resident, 1)
        self.write(self.base+0x538e168, resident, 1)
        self.write(self.service_address+0x200, flags, 4)
        self.write(self.base+0x53f08d0, flags, 4)
        if service and resident < 8:
            self.write(self.service_address+0xd68+resident*4, buffer, 4)
            snapshot = self.service_address+0x6e8+resident*0xd0+(buffer&1)*0x68
        else:
            snapshot = self.base+0x53f0c08
        self.write(snapshot, kind, 2)
        self.write(snapshot+4, action, 4)
        self.write(snapshot+0x64, valid, 1)
    def result(self, hooked):
        self.u.reg_write(UC_ARM64_REG_X19, 0x12345678)
        self.u.reg_write(UC_ARM64_REG_X29, 0x87654321)
        self.call(self.callback if hooked else self.base+ELIGIBILITY)
        assert self.u.reg_read(UC_ARM64_REG_X19) == 0x12345678
        assert self.u.reg_read(UC_ARM64_REG_X29) == 0x87654321
        return self.u.reg_read(UC_ARM64_REG_W0)

def main():
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        m = CameraMachine(base)
        # Every valid native action, plus invalid values. The only changed
        # result is surface SwimStride; DiveStride already passes natively.
        for action in (*range(0x14a), 0xffffffff):
            for flags in (0, 1<<9, 0xffffffff, 0xfffffdff):
                m.prepare(action, flags)
                assert m.result(False) == int(not(flags & (1<<9)) and action != 0xf7)
                assert m.result(True) == int(not(flags & (1<<9)))
                cases += 1
        for resident in range(8):
            for buffer in (0, 1, 3):
                for valid, kind in ((1,23), (0,23), (1,24)):
                    m.prepare(0xf7, resident=resident, buffer=buffer, valid=valid, kind=kind)
                    assert m.result(False) == int(not valid or kind != 23)
                    assert m.result(True) == 1
                    cases += 1
        # Native initialized fallback snapshots/flags: service absent or no
        # valid resident. Lock bit 9 always retains precedence over swimming.
        for service, resident in ((False,0), (True,8), (True,255)):
            for flags in (0, 1<<9):
                m.prepare(0xf7, flags, resident=resident, service=service)
                assert m.result(False) == 0
                assert m.result(True) == int(not flags)
                cases += 1
        for files, camera, movement in (
            ({'controls':CONFIG}, True, True),
            ({'controls':CAMERA_ONLY}, True, False),
            ({'controls':CONFIG+b'swim_stroke_boost=false\n'}, True, True),
            ({'controls':CONFIG,'controls.custom':b'auto_swim=false'}, True, False),
            ({'controls':CAMERA_ONLY,'controls.custom':b'auto_swim=true'}, True, True),
            ({'controls':CONFIG+b'camera_tilt_while_swimming=false\n'}, True, True),
            ({'controls':CONFIG.replace(b'enabled=true',b'enabled=false')}, False, False),
            ({'controls.custom':b'enabled=true\nauto_swim=true'}, False, False),
            ({'swimming':DEFAULT}, False, False),
            ({'swimming':DEFAULT,'swimming.custom':b'auto_swim=false','controls':CONFIG}, True, True),
        ):
            n=Machine(base,files); n.boot(); n.activate()
            assert (base+ELIGIBILITY in n.hooks) == camera
            assert all((base+a in n.hooks) == movement for a in BINDINGS)
            # The native three-position tilt controller is left intact.
            assert bytes(n.u.mem_read(base+0x7cd7bc,0x400)) == RAW[0x7cd7bc:0x7cdbbc]
        for offset in FINGERPRINTS:
            n=Machine(base,{'controls':CONFIG})
            n.write(base+offset,0xdeadbeef,4); n.boot(); n.activate()
            assert 'camera tilt: DISABLED' in n.text and base+ELIGIBILITY not in n.hooks
            assert all(base+a in n.hooks for a in BINDINGS)
        # A movement conflict must not prevent the independent camera feature.
        n=Machine(base,{'controls':CONFIG})
        n.write(base+BINDINGS[0],0xdeadbeef,4); n.boot(); n.activate()
        assert base+ELIGIBILITY in n.hooks and all(base+a not in n.hooks for a in BINDINGS)
    print(f'PASS: {cases} native camera action/lock/snapshot cases at two ASLR bases; only SwimStride rejection relaxed; three-position controller unchanged; automatic Controls camera fix, boost/movement settings, disabled modules and isolated conflicts')

if __name__ == '__main__': main()
