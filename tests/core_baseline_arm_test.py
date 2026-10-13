"""Compare production startup outcomes while retaining inherited Core assertions.

Each child loads its own ELF. Engine FS/hook/cache services remain synthetic;
the inherited tests execute installed scan initialization/countdown/expiry code.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]

def replay(baseline):
    saved = Path(baseline).read_bytes() if baseline else None
    original_read = Path.read_bytes
    def read(path):
        return saved if saved is not None and path == ROOT/'project.elf' else original_read(path)
    with patch.object(Path,'read_bytes',read):
        import core_arm_test as core
        digest=hashlib.sha256(); count=0
        original_activate=core.Machine.activate
        def activate(machine,*args,**kwargs):
            nonlocal count
            result=original_activate(machine,*args,**kwargs)
            # FeatureResults<64>::Entry: id pointer, state, published, reason
            # pointer. Compare values/strings, never ELF-local pointer addresses.
            entries=machine.module.find('ActivationResults')
            states=[]
            for index in range(64):
                entry=entries+index*24
                identifier=machine.read(entry)
                if identifier:
                    reason=machine.read(entry+16)
                    states.append([machine.string(identifier),machine.read(entry+8,4),
                                   machine.read(entry+12,1),machine.string(reason) if reason else None])
            bindings=[]
            for offset in (0x49e8378,0x4a06740,0x4a10398,0x49cd3f8):
                value=machine.read(machine.base+offset)
                bindings.append(value-machine.base if machine.base<=value<machine.base+0x6000000 else 'module')
            outcome={'hooks':sorted(address-machine.base for address in machine.hooks),
                     'states':states,'opened':machine.opened,'closed':machine.closed,
                     'bindings':bindings}
            digest.update((json.dumps(outcome,sort_keys=True)+'\n').encode());count+=1
            return result
        with patch.object(core.Machine,'activate',activate):
            core.check_all_subsets()
            core.check_errors()
            core.check_amiibo_hook()
            core.check_custom_preferences()
            core.check_config_layers()
            core.check_crafting_settings()
            core.check_pockets_settings()
            core.check_catalog_settings()
        print(json.dumps({'activation_outcomes':count,'sha256':digest.hexdigest()}))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replay',action='store_true');parser.add_argument('--saved')
    args=parser.parse_args()
    if args.replay: replay(args.saved);return
    baseline=os.environ.get('QOL_BASELINE_ELF')
    if not baseline: print('SKIP Core baseline (set QOL_BASELINE_ELF)');return
    command=[sys.executable,str(Path(__file__).resolve()),'--replay']
    current=subprocess.check_output(command,text=True)
    previous=subprocess.check_output(command+['--saved',str(Path(baseline).resolve())],text=True)
    assert current==previous,(current,previous)
    evidence=json.loads(current.splitlines()[-1])
    print(f'PASS Core baseline: {evidence["activation_outcomes"]} production activation outcomes match accepted ELF; inherited assertions unchanged')
if __name__=='__main__':main()
