"""Replay inherited Shopping Card assertions and native-call sequences per ELF.

Save/UI services are synthetic; actual native money/dispatch/animation bodies
execute in the inherited suites. No root ELF is replaced during comparison.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ('shopping_card_arm_test.py', 'shopping_display_arm_test.py',
           'shopping_cash_arm_test.py', 'shopping_storage_arm_test.py')

def replay(script, baseline):
    saved = Path(baseline).read_bytes() if baseline else None
    original_read = Path.read_bytes
    def read(path):
        return saved if saved is not None and path == ROOT/'project.elf' else original_read(path)
    with patch.object(Path, 'read_bytes', read):
        import core_arm_test as core
        digest = hashlib.sha256()
        count = 0
        money = {0x257abf0:'cash', 0x257ad90:'wallet-space', 0x257af90:'add-cash',
                 0x257b340:'debit', 0x257f8b0:'savings', 0x257f9f0:'deposit', 0x257fb30:'withdraw'}
        others = {0x60f4f0:'dispatch-action', 0x60fb10:'dispatch-query',
                  0xc97260:'bell-count-down', 0x2579fe0:'purchase-space',
                  0x257b6f0:'capacity', 0x3353b14:'number-step',
                  0x3025400:'feedback', 0x3025a4c:'format', 0x3c1b588:'text'}
        original_init = core.Machine.__init__
        def init(machine, *args, **kwargs):
            service = machine.service
            def observed(u, pc, size, data):
                nonlocal count
                offset = pc-machine.base
                native = machine.services.get(pc)
                name = money.get(offset) or others.get(offset)
                if not name and native and native[0]=='card_original': name='original-'+native[1]
                if name:
                    value=[name]
                    if offset in money or name in ('original-ShopWallet','original-ShopDebit'):
                        pointer=machine.reg(0)
                        value.append(machine.read(pointer,1) if pointer else None)
                    if name in ('withdraw','deposit','debit','add-cash','original-ShopDebit','format'):
                        value.append(machine.reg(1))
                    if name=='purchase-space': value.extend((machine.reg(1),machine.reg(2)))
                    if name=='text':
                        value.append(bytes(u.mem_read(machine.reg(1),machine.reg(2)*2)).hex())
                    if name=='number-step': value.append(bytes(u.mem_read(machine.reg(0),16)).hex())
                    digest.update((json.dumps(value)+'\n').encode());count+=1
                return service(u,pc,size,data)
            machine.service=observed
            original_init(machine,*args,**kwargs)
        with patch.object(core.Machine,'__init__',init):
            if script=='shopping_card_arm_test.py': __import__('shopping_card_arm_test')
            else: runpy.run_path(str(ROOT/'tests'/script),run_name='__main__')
        print(json.dumps({'native_calls':count,'sha256':digest.hexdigest()}))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replay',choices=SCRIPTS);parser.add_argument('--saved')
    args=parser.parse_args()
    if args.replay: replay(args.replay,args.saved);return
    baseline=os.environ.get('QOL_BASELINE_ELF')
    if not baseline: print('SKIP Shopping Card baseline (set QOL_BASELINE_ELF)');return
    calls=0
    for script in SCRIPTS:
        command=[sys.executable,str(Path(__file__).resolve()),'--replay',script]
        current=subprocess.check_output(command,text=True)
        previous=subprocess.check_output(command+['--saved',str(Path(baseline).resolve())],text=True)
        assert current==previous,(script,current,previous)
        evidence=json.loads(current.splitlines()[-1]);calls+=evidence['native_calls']
        print(f'PASS shopping baseline: {script}; {evidence["native_calls"]} native calls match',flush=True)
    print(f'PASS shopping baseline: {len(SCRIPTS)} inherited suites; {calls} native calls match accepted ELF')
if __name__=='__main__': main()
