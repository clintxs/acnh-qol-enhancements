"""Execute 1.0.2 startup and affected callbacks over every supplied FPS patch.

Native text is patched exactly as the emulator/IPS loader would patch it. Hook
publication and engine services are modeled; this is not a live emulator test.
"""
from pathlib import Path
import argparse
import hashlib
import json
import itertools
import math
import struct
import sys
from unittest.mock import patch as mock_patch
from unicorn.arm64_const import UC_ARM64_REG_X0
from core_arm_test import Machine, DEFAULT, RAW, SLUGS, expected_hook_count

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / 'tools'))
from fps_patches import read_patch, variants

def machine(base, patch, slugs=SLUGS, custom=None):
    files={slug: DEFAULT for slug in slugs}
    if custom is not None: files['core.custom']=custom
    m = Machine(base, files)
    for offset, value in patch.items(): m.u.mem_write(base+offset, bytes([value]))
    m.boot(); m.activate()
    return m

def menu_cases(m, fps, patch):
    owner, menu = m.ram+0x2000, m.ram+0x3000
    m.write(owner+0x218, menu)
    m.write(owner+0x200, 3, 1)
    requested = owner+0x1f0+3*4
    callback = m.module.find('UpdatePocketMenu', 'Original')
    count = 0
    for result, age, previous in itertools.product((0,1,2,0xffffffff), (-1,0,1,2,3,4,5), (-1,0,7,12)):
        pair = result | ((age & 0xffffffff) << 32)
        m.write(menu+0x1b0, pair)
        m.write(requested, previous & 0xffffffff, 4)
        m.u.reg_write(UC_ARM64_REG_X0, owner); m.call(callback)
        actual = m.read(requested, 4)
        # Execute the exact patched native predicate in a scratch copy, without
        # our entry hook. It follows both of the SpeedFix external stub jumps.
        original = bytes(m.u.mem_read(m.base+0x308c590, 20))
        native = bytes(patch.get(i, RAW[i]) for i in range(0x308c590,0x308c5a4))
        m.u.mem_write(m.base+0x308c590, native)
        m.write(requested, previous & 0xffffffff, 4)
        m.u.reg_write(UC_ARM64_REG_X0, owner); m.call(m.base+0x308c590)
        expected = m.read(requested, 4)
        m.u.mem_write(m.base+0x308c590, original)
        assert actual == expected, (fps,result,age,previous,actual,expected)
        count += 1
    return count

def crafting_cases(m, fps):
    actor, context = m.ram+0x5000, m.ram+0x8000
    m.write(context+0x200+19*8, actor)
    samples = []
    for offset in (0x22fc6bc,0x2279a14):
        for updates in (0,fps//2,fps,fps*2,fps*3):
            m.write(actor+0x1c04,updates,4)
            m.u.reg_write(UC_ARM64_REG_X0,context); m.call(m.hooks[m.base+offset])
            value = struct.unpack('<f',m.u.mem_read(context,4))[0]
            expected = 1 + 5 * math.log10(1 + 9 * min(updates*30//fps, 60)/60)
            assert abs(value-expected) < 1e-5, (fps,updates,value,expected)
            samples.append(value)
    # Amiibo default remains five minutes at each nominal update rate.
    context = m.ram+0x9000
    m.u.reg_write(UC_ARM64_REG_X0,context); m.call(m.hooks[m.base+0xbc4144])
    assert m.read(context+8*8,4) == fps*300
    return samples

def dialogue_cases(base, changes):
    import core_arm_test
    import dialogue_arm_test
    from dialogue_hurry_arm_test import HurryMachine
    patched=bytearray(RAW)
    for offset,value in changes.items(): patched[offset]=value
    with mock_patch.object(core_arm_test,'RAW',bytes(patched)), mock_patch.object(dialogue_arm_test,'RAW',bytes(patched)):
        m=HurryMachine(base)
        for buttons,choice,expected in ((0x10,False,7),(0x10,True,6),(0,False,5)):
            m.prepare(buttons=buttons,choice=choice)
            assert m.advance()==expected
            assert m.request==bool(buttons)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--external',type=Path,default=PROJECT.parent/'external')
    args=parser.parse_args()
    paths=variants(args.external)
    assert len(paths)==5, paths
    audited=json.loads((PROJECT/'docs/fps-patch-inputs.json').read_text())['inputs']
    hashes={entry['variant']:entry['sha256'] for entry in audited}
    cases=0
    for path in paths:
        variant=path.relative_to(args.external).parts[0]
        assert hashlib.sha256(path.read_bytes()).hexdigest()==hashes[variant], path
        patch=read_patch(path)
        fps=int(path.relative_to(args.external).parts[0].split('fps')[0])
        for base in (0x7100000000,0x7200000000):
            m=machine(base,patch)
            assert 'BOOT_OK' in m.text and 'errors=0' in m.text,m.text
            assert len(m.hooks)==expected_hook_count(m,(True,)*7),m.text
            assert m.read(m.module.find('UpdateFrequency'),4)==fps
            spans={byte for address in m.hooks for byte in range(address-base,address-base+20)}
            # Every FPS-mod byte outside our declared hook spans is preserved.
            for offset,value in patch.items():
                if offset not in spans: assert m.read(base+offset,1)==value,hex(offset)
            # Identify external FPS branches back into overwritten entry spans.
            # UpdateMenu's helper back-edge is handled explicitly in C++.
            for offset in {i&~3 for i in patch}:
                word=int.from_bytes(bytes(patch.get(i,RAW[i]) for i in range(offset,offset+4)),'little')
                if offset in spans: continue # Exlaunch relocates this source instruction.
                if word & 0x7c000000 == 0x14000000: bits,delta=26,word&0x3ffffff
                elif word & 0xff000010 == 0x54000000 or word & 0x7e000000 == 0x34000000:
                    bits,delta=19,(word>>5)&0x7ffff
                elif word & 0x7e000000 == 0x36000000: bits,delta=14,(word>>5)&0x3fff
                else: continue
                if delta & (1<<(bits-1)): delta-=1<<bits
                target=offset+delta*4
                if target in spans and m.base+target not in m.hooks:
                    assert target==0x308c59c,(path,hex(offset),hex(target))
            cases+=menu_cases(m,fps,patch)
            crafting_cases(m,fps)
            dialogue_cases(base,patch)
        for slugs in (('pockets',),('controls',)):
            m=machine(0x7100000000,patch,slugs)
            assert 'BOOT_OK' in m.text and 'errors=0' in m.text,m.text
        capped=machine(0x7100000000,patch,(),b'amiibo_timeout_frames=65535')
        context=capped.ram+0x9000
        capped.u.reg_write(UC_ARM64_REG_X0,context); capped.call(capped.hooks[capped.base+0xbc4144])
        assert capped.read(context+8*8,4)==65535 and 'capped at 65535' in capped.text
        disabled=machine(0x7100000000,patch,(),b'amiibo_timeout_frames=0')
        assert disabled.base+0xbc4144 not in disabled.hooks
        # Unknown edits at supported sites,
        # and corrupted helper code must still reject the affected feature.
        for offset in (0x308c594,0x1afa53c,0x3088644,0x3090d04):
            corrupted=dict(patch)
            corrupted.update((offset+i,byte) for i,byte in enumerate(b'\xef\xbe\xad\xde'))
            m=machine(0x7100000000,corrupted)
            assert 'BOOT_PARTIAL' in m.text and 'instruction mismatch' in m.text,m.text
        print(f'PASS {path.relative_to(args.external).parts[0]}: startup, isolated modules, patch preservation, menu predicate, crafting ramp, amiibo duration, rejection guards',flush=True)
    print(f'PASS {cases} compiled menu/native equivalence cases across five variants at two ASLR bases')

if __name__=='__main__': main()
