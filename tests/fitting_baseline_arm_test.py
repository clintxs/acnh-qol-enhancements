"""Replay unchanged fitting scenarios with native-call and outcome comparisons.

The accepted ELF is read independently in each child process; no root output is
replaced. Native checkout/input/feedback execute with inherited synthetic UI/save
services. This is not live rendering or a native transaction success guarantee.
"""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ('runtime_arm_test.py','fitting_back_arm_test.py','purchase_arm_test.py',
           'feedback_arm_test.py','fitting_hint_arm_test.py','caption_contract_arm_test.py')

def replay(script, baseline):
    saved = Path(baseline).read_bytes() if baseline else None
    original_read = Path.read_bytes
    def read(path):
        return saved if saved is not None and path == ROOT/'project.elf' else original_read(path)
    with patch.object(Path,'read_bytes',read):
        import runtime_arm_test as runtime
        import core_arm_test as core
        native_digest = hashlib.sha256(); outcome_digest = hashlib.sha256()
        native_count = 0; outcome_count = 0
        addresses = {0x311ca40:'collect',0x311e180:'price',0x311dec0:'gross',
                     0x311d3b0:'checkout',0x31141cc:'answer',0x257c230:'storage-space',
                     0x25731d0:'pocket-space',0x257abf0:'funds',0x257b340:'payment',
                     0x257d630:'storage-delivery',0x25738b0:'pocket-delivery',0x25772d0:'ticket',
                     0x24d74cc:'record',0x269b044:'statistic-1',0x269a9f0:'statistic-2',
                     0x1a67430:'milestone',0x31017e0:'focus',0x3c58d24:'input',0x2de2b00:'footer'}
        def serial(value):
            if isinstance(value,bytes): return value.hex()
            if isinstance(value,(list,tuple)): return [serial(v) for v in value]
            if isinstance(value,dict): return {k:serial(v) for k,v in value.items()}
            return value
        def record(digest, value): digest.update((json.dumps(serial(value),sort_keys=True)+'\n').encode())
        def trace(machine,pc):
            nonlocal native_count
            offset = pc-machine.base
            service = machine.services.get(pc)
            if offset not in addresses and not (service and service[0]=='original'): return
            name = addresses.get(offset) or 'original-'+service[1]
            value = [name]
            if name in ('collect','original-Collect','answer','original-Dialog','ticket','statistic-1','statistic-2'):
                value.append(machine.reg(2))
            elif name in ('checkout','original-Checkout','payment','milestone'): value.append(machine.reg(1))
            elif name in ('storage-delivery','pocket-delivery'):
                value.append(bytes(machine.u.mem_read(machine.reg(1),8)))
            elif name in ('record','original-PurchaseRecord'):
                pointer = machine.reg(1)
                value.extend((machine.read(pointer+4,4),machine.read(pointer+0xc,4)))
            elif name=='price' and hasattr(machine,'owner'):
                state=machine.reg(0)
                if state==machine.owner+0x1c330: value.append('outfit-total')
                else:
                    item=machine.read(machine.read(state+0x10))
                    value.append(bytes(machine.u.mem_read(item,8)))
            record(native_digest,value); native_count += 1
        def init_wrapper(original):
            def init(machine,*args,**kwargs):
                service = machine.service
                def observed(u,pc,size,data):
                    trace(machine,pc)
                    return service(u,pc,size,data)
                machine.service = observed
                original(machine,*args,**kwargs)
            return init
        def call_wrapper(original, has_bag):
            def call(machine,*args,**kwargs):
                nonlocal outcome_count
                result = original(machine,*args,**kwargs)
                value = {}
                if has_bag and hasattr(machine,'bag'):
                    count=machine.read(machine.count,4); assert count<=256
                    value['bag']=[bytes(machine.u.mem_read(machine.bag+i*12,12)) for i in range(count)]
                    value['owner']=[machine.read(machine.owner+off,4) for off in
                                    (0x1b0,0x1f0,0x1f4,0x1c318,0x1c4c4,0x1c4c8,0x1c4f0)]
                    value['buttons']=[machine.read(machine.rows,4),machine.read(machine.rows+0x3c,4)]
                    value['original_calls']=dict(Counter(machine.calls))
                for field in ('paid','delivered','records','gross','tickets','native_collects',
                              'events','taps','prepares','texts','text_events','opened'):
                    if hasattr(machine,field): value[field]=getattr(machine,field)
                if hasattr(machine,'pane'):
                    value['badge']=bytes(machine.u.mem_read(machine.pane+0x48,8))+bytes(machine.u.mem_read(machine.pane+0x58,2))
                record(outcome_digest,value); outcome_count += 1
                return result
            return call
        with patch.object(runtime.Machine,'__init__',init_wrapper(runtime.Machine.__init__)), \
             patch.object(runtime.Machine,'call',call_wrapper(runtime.Machine.call,True)), \
             patch.object(core.Machine,'__init__',init_wrapper(core.Machine.__init__)), \
             patch.object(core.Machine,'call',call_wrapper(core.Machine.call,False)):
            if script=='runtime_arm_test.py': runtime.main()
            else: runpy.run_path(str(ROOT/'tests'/script),run_name='__main__')
        print(json.dumps({'native_calls':native_count,'native_sha256':native_digest.hexdigest(),
                          'outcomes':outcome_count,'outcome_sha256':outcome_digest.hexdigest()}))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replay',choices=SCRIPTS);parser.add_argument('--saved')
    args=parser.parse_args()
    if args.replay: replay(args.replay,args.saved);return
    baseline=os.environ.get('QOL_BASELINE_ELF')
    if not baseline: print('SKIP fitting baseline replay (set QOL_BASELINE_ELF)');return
    calls=outcomes=0
    for script in SCRIPTS:
        command=[sys.executable,str(Path(__file__).resolve()),'--replay',script]
        current=subprocess.check_output(command,text=True)
        previous=subprocess.check_output(command+['--saved',str(Path(baseline).resolve())],text=True)
        assert current==previous,(script,current,previous)
        evidence=json.loads(current.splitlines()[-1]);calls+=evidence['native_calls'];outcomes+=evidence['outcomes']
        print(f'PASS fitting baseline: {script}; {evidence["native_calls"]} native calls and {evidence["outcomes"]} outcomes match',flush=True)
    print(f'PASS fitting baseline: {len(SCRIPTS)} inherited suites; {calls} native calls and {outcomes} outcomes match accepted ELF')
if __name__=='__main__': main()
