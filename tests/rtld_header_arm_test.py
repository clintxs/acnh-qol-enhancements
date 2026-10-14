"""Run ACNH 3.0.3's actual rtld header reader against Core's built NSO."""
from pathlib import Path
import struct

from unicorn import Uc, UcError, UC_ARCH_ARM64, UC_MODE_ARM
from unicorn.arm64_const import (UC_ARM64_REG_SP, UC_ARM64_REG_X1,
    UC_ARM64_REG_X12, UC_ARM64_REG_X23, UC_ARM64_REG_X24)
from nso_reader import image

ROOT = Path(__file__).resolve().parents[1]
RTLD = (ROOT.parent / 'Extracted' /
        'Animal Crossing_ New Horizons 3.0.3 [01006F8002326800][v2228224][UPD]' /
        'Program #0/0/rtld')
RTLD_ID = '283033deea3fc0b60104a4790098b9ccdb62d006000000000000000000000000'
# Disassembled reader: +0x10fc selects the module header, +0x1160 copies its
# legacy descriptor; +0x11bc is reached only after validating MOD0 magic.
READER, READ_COMPLETE = 0x10fc, 0x11bc
BRANCH_PLUS_8, OLD_BRANCH_PLUS_32, NOP = 0x14000002, 0x14000008, 0xd503201f


def probe(rtld, module, base):
    machine = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    loader, stack = 0x100000, 0x200000
    machine.mem_map(loader, 0x10000)
    machine.mem_write(loader, rtld)
    machine.mem_map(stack, 0x10000)
    machine.reg_write(UC_ARM64_REG_SP, stack + 0x8000)
    machine.mem_map(base, (len(module) + 0xfff) & ~0xfff)
    machine.mem_write(base, bytes(module))
    machine.reg_write(UC_ARM64_REG_X23, base)
    try:
        machine.emu_start(loader + READER, loader + READ_COMPLETE, count=1000)
    except UcError:
        return None, machine.reg_read(UC_ARM64_REG_X1)
    return (machine.reg_read(UC_ARM64_REG_X24),
            machine.reg_read(UC_ARM64_REG_X12) & 0xffffffff), None


def check():
    rtld = image(RTLD, RTLD_ID)
    path = ROOT / 'deploy/subsdk6'
    module = image(path, path.read_bytes()[0x40:0x60].hex())
    first, mod0, trampoline = struct.unpack_from('<3I', module)
    assert first == BRANCH_PLUS_8, 'Nintendo rtld requires b +8 for this header ABI'
    assert trampoline & 0xfc000000 == 0x14000000
    assert 8 + ((trampoline & 0x03ffffff) << 2) == 0x20
    assert module[16:32] == b'~~exlaunch uwu~~'
    expected = struct.unpack_from('<3I', module, mod0)
    assert expected[0] == int.from_bytes(b'MOD0', 'little')
    broken = bytearray(module)
    struct.pack_into('<I', broken, 0, OLD_BRANCH_PLUS_32)
    struct.pack_into('<I', broken, 8, NOP)
    for base in (0x6c40c78000, 0x7100000000):
        actual, invalid = probe(rtld, module, base)
        assert actual == (base + mod0, expected[0]) and invalid is None, actual
        actual, invalid = probe(rtld, broken, base)
        assert actual is None and invalid == base + NOP
        if base == 0x6c40c78000:
            assert invalid == 0x6d15caa01f  # October 14 hardware crash.
    print('PASS: actual ACNH rtld reads corrected Core header at two bases; '
          'old header reproduces the hardware crash address exactly')


if __name__ == '__main__':
    check()
