"""Compare fresh production-installed Crafting builds without overwriting either ELF.

Native speed/input/model fragments execute through the inherited fixtures.
Scene/GPU/resource services retain those fixtures' explicit modeling limits.
"""
import hashlib
import itertools
import os
from pathlib import Path
from unittest.mock import patch
from crafting_arm_test import CraftingMachine, DEFAULT
from recipe_preview_fixture import PreviewMachine
from quantity_shortcut_fixture import ShortcutMachine
from crafting_scenario_fixture import navigation_fixture, quantity_wrap_fixture


def main():
    filename = os.environ.get('QOL_BASELINE_ELF')
    if not filename:
        print('SKIP: Crafting baseline comparison; set QOL_BASELINE_ELF')
        return
    baseline = Path(filename).read_bytes()
    read_bytes = Path.read_bytes
    def saved_bytes(path):
        return baseline if path.name == 'project.elf' and path.parent.name == 'qol-enhancements' else read_bytes(path)
    def pair(factory, *args):
        current = factory(*args)
        with patch.object(Path, 'read_bytes', saved_bytes): previous = factory(*args)
        return current, previous
    cases = 0
    for base in (0x7100000000, 0x7200000000):
        for maximum in (0,1,3,6,30):
            current, previous = pair(CraftingMachine, base, {'crafting':DEFAULT+f'crafting_speed={maximum}\n'.encode()}, maximum)
            for cooking, elapsed, ready, remote, blocked in itertools.product((False,True), (0,1,29,59,60,61,0xffffffff), (False,True), (False,True), (False,True)):
                for m in (current, previous): m.run(cooking=cooking, elapsed=elapsed, ready=ready, remote=remote, blocked=blocked, count=1, pressed=True)
                assert (current.rate,current.hits,current.recipe_checks)==(previous.rate,previous.hits,previous.recipe_checks)
                for offset,size in ((0xed0,4),(0xc08,4),(0xdb8,4),(0x1920,4),(0x1928,4)):
                    assert bytes(current.u.mem_read(current.obj+offset,size))==bytes(previous.u.mem_read(previous.obj+offset,size))
                cases += 1
        for kitchen in (False,True):
            current, previous = pair(ShortcutMachine, base, kitchen)
            for valid,materials,multiple,supports,buttons,consumed in itertools.product((False,True),repeat=6):
                button_mask = 0x80 if buttons else 0x40
                for m in (current,previous):
                    m.valid,m.materials,m.multiple = valid,materials,multiple
                    m.write(m.seq+0x1c1f0,int(supports),1)
                    m.enter();m.reset_footer();m.hook(m.footer_offset,m.seq)
                    m.input_y();m.input_y(button_mask,button_mask if consumed else 0)
                assert current.hints()==previous.hints()
                assert current.native_inputs==previous.native_inputs
                for offset,size in ((0x1f0,8),(0x1c1c8,8),(0x1c1dc,4),(0x239,1)):
                    assert bytes(current.u.mem_read(current.seq+offset,size))==bytes(previous.u.mem_read(previous.seq+offset,size))
                assert current.read(current.row+0x3c,4)==previous.read(previous.row+0x3c,4)
                cases += 1
            for m in (current,previous):
                m.valid=m.materials=m.multiple=True;m.ready=True;m.write(m.seq+0x1c1f0,1,1)
                m.enter();m.input_y();m.input_y(0x80)
                m.hook(0x3494e7c,m.seq);m.write(m.seq+0x1b0,9,4);m.write(m.seq+0x1f0,0xffffffff,4)
                m.invoke(base+0x3496160,m.seq)
                m.closed=True;m.invoke(base+0x349634c,m.seq)
            assert (current.attached,current.detached,current.favorites)==(previous.attached,previous.detached,previous.favorites)
            assert current.read(current.seq+0x1f0,4)==previous.read(previous.seq+0x1f0,4)
            cases += 1
        for reason in ('normal','busy','spawn-fail','controller','parameters','mode','timeout','suppressed','consumed','surface'):
            print(f'Comparing preview {base:#x}: {reason}',flush=True)
            current,previous = pair(PreviewMachine,base)
            fixtures=[]
            for m in (current,previous):
                phone,page,row,panes=m.phone(surface=reason!='surface',hotel=True)
                if reason=='busy':m.write(m.controller+0x50,4,4)
                if reason=='spawn-fail':m.fail_spawn=True
                m.input(page,0)
                m.input(page,0x10,blocked=int(reason=='suppressed'),consumed=0x10 if reason=='consumed' else 0)
                m.frame(phone)
                if reason=='controller':m.write(m.context+0x30,m.alloc(0x1200))
                if reason=='parameters':m.write(m.context+0x28,m.alloc(0x2200))
                if reason=='mode':m.write(m.controller+0x50,7,4)
                if reason=='timeout':m.write(m.controller+0x10,0,4)
                fixtures.append((phone,page,panes))
            def compare():
                assert current.events==previous.events,(reason,current.events,previous.events)
                for machine_index,(phone,page,panes) in enumerate(fixtures):
                    m=(current,previous)[machine_index]
                    snapshot=(tuple(m.read(panes[name]+0x58,1) for name in panes),m.read(m.controller+0x50,4),m.read(m.controller+0x10,4),m.read(m.controller+0x6c,4),bytes(m.u.mem_read(m.controller+0x80,12)),bytes(m.u.mem_read(m.parameters+0x2c,40)))
                    if machine_index==0:expected=snapshot
                    else:assert snapshot==expected,reason
            def frame(m, phone):
                # A foreign/unfinished renderer has no modeled native base-state
                # table. Match the inherited ownership-loss fixture boundary:
                # run the installed phone callback, including its Orig service.
                if reason in ('controller','parameters','mode','timeout'):
                    m.hook(0x2e9fbb0,phone)
                else:m.frame(phone)
            compare();cases+=1
            for _ in range(122 if reason=='timeout' else 3):
                for m,(phone,page,panes) in zip((current,previous),fixtures):frame(m,phone)
                compare();cases+=1
            if reason=='normal':
                for index in range(24):
                    for m,(phone,page,panes) in zip((current,previous),fixtures):
                        m.input(page,4 if index%3==0 else 0,left=(.8,.2),right=(.4 if index%2 else -.7,.9),blocked=int(index%7==0),consumed_axes=2 if index%5==0 else 0)
                        m.frame(phone)
                    compare();cases+=1
            for m,(phone,page,panes) in zip((current,previous),fixtures):
                m.input(page,0);m.input(page,0x20);frame(m,phone);m.input(page,0);frame(m,phone)
                m.hook(0x2e9d538,phone)
            compare();cases+=1
    # Reuse the complete inherited navigation/wrap scenarios on both builds,
    # retaining their assertions and comparing observable input and native
    # service event streams. Ignore unobserved padding in the actor request;
    # item/recipe fields match the established native assertions.
    original_service, original_input = PreviewMachine.service, PreviewMachine.input
    def replay(scenario, *args):
        digest = hashlib.sha256(); records=[]; counts = {'events':0, 'inputs':0}
        def record(value):
            records.append(value); digest.update(repr(value).encode()); digest.update(b'\n')
        def service(m,u,pc,size,data):
            before = len(m.events)
            result = original_service(m,u,pc,size,data)
            for event in m.events[before:]:
                if event[0] in ('grid_enter','grid_leave'):
                    # Native event objects live on each caller's stack; compare
                    # event order/frequency, while inherited assertions verify
                    # their selected/focused row and material update results.
                    event = (event[0],)
                if event[0] == 'change_item':
                    event = (event[0],int.from_bytes(event[1][:8],'little'),int.from_bytes(event[1][0x24:0x26],'little'))
                record(('native',event)); counts['events'] += 1
            return result
        def input_sample(m,*sample_args,**kwargs):
            result = original_input(m,*sample_args,**kwargs)
            record(('input',result,bytes(m.u.mem_read(m.last_input_row,0x48)),bytes(m.u.mem_read(m.controller+0x80,12))))
            counts['inputs'] += 1
            return result
        with patch.object(PreviewMachine,'service',service), patch.object(PreviewMachine,'input',input_sample):
            scenario(*args)
        return digest.hexdigest(), counts, records
    for base in (0x7100000000,0x7200000000):
        for scenario,args in ((navigation_fixture,(base,)),(quantity_wrap_fixture,(base,5)),(quantity_wrap_fixture,(base,15))):
            actual = replay(scenario,*args)
            with patch.object(Path,'read_bytes',saved_bytes): expected = replay(scenario,*args)
            if actual[:2] != expected[:2]:
                for index,(left,right) in enumerate(zip(actual[2],expected[2])):
                    if left!=right: raise AssertionError((scenario.__name__,args,index,left,right))
                raise AssertionError((scenario.__name__,args,actual[:2],expected[:2]))
            cases += actual[1]['events'] + actual[1]['inputs']
            print('PASS baseline trace:',scenario.__name__,args,actual[1],flush=True)
    print(f'PASS: {cases} current/baseline Crafting comparisons at two ASLR bases; exact native rates/counters, hints/shortcut requests and completion, preview event order/call frequency, UI restoration, rotations, ownership loss, timeout and cancellation')

if __name__ == '__main__': main()
