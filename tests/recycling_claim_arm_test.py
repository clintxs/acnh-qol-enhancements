"""Compiled Claim bypass and native recycling transfer/record removal lifecycle.

Import also runs the Pockets suite. Engine inventory insertion and UI rendering
are stubs; original menu builder, selection processor and state transitions run.
"""
from arm_stack_test import Machine, raw, u64, symbol
import struct

cases=0
for base in (0x80000000,0x1234500000):
    h=Machine(base)
    owner=h.ram+0x1000;menu=h.ram+0x20000;page=h.ram+0x24000;grid=h.ram+0x25000
    items=h.ram+0x26000;states=h.ram+0x27000
    name_window=h.ram+0x2b000
    h.write(owner+0x220,name_window);h.write(name_window,base+0x4a01130)
    h.write(base+0x4a01130+0x1c0,base+0x30127b8)
    h.write(base+0x4a01130+0x200,base+u64(raw,0x4a01130+0x200))
    h.write(base+0x4ac4eb8,h.ram+0x2b300)
    player=h.ram+0x28000;player_vt=player+0x100;global_object=player+0x200
    h.u.mem_map(h.ram+0x100000,0x800000)
    h.u.mem_map(base+0x5474000,0x14000)
    empty_item=struct.pack('<Q',0xfffe)
    h.u.mem_write(base+0x5486008,empty_item)
    save=h.ram+0x100000;storage=save+0x276cb8;savedata=h.ram+0x400000
    safe_vtable=base+u64(raw,0x4ab2020)+16
    h.write(base+0x4ab2020,safe_vtable-16)
    safe_prepare=base+u64(raw,u64(raw,0x4ab2020)+16+24)
    h.write(safe_vtable+24,safe_prepare)
    for off in (0,8,16):
        h.write(base+0x49c7208+off,base+u64(raw,0x49c7208+off))
        h.write(base+0x4a16510+off,base+u64(raw,0x4a16510+off))
    h.write(owner,base+0x49df828);h.write(owner+0x210,page);h.write(owner+0x228,menu)
    h.write(owner+0x1c1d8,60,4);h.write(owner+0x1c1e0,items)
    h.write(page+0x578,grid)
    h.write(menu+0x218,base+0x49dfba8);h.write(menu+0x220,owner)
    h.write(menu+0x228,base+0x2defbfc);h.write(menu+0x1f0,0xffffffffffffffff)
    h.write(owner+0x1d0,6,4);h.write(owner+0x1d8,states)
    for state,entry,update in ((3,h.ram+0x29000,0),(5,h.ram+0x29010,0),
        (4,h.callback('EnterRecyclingMenu'),h.callback('UpdateRecyclingMenu'))):
        record=states+state*0x40
        h.write(record,base+0x4a16510);h.write(record+8,owner)
        h.write(record+0x10,entry);h.write(record+0x20,update)
    h.write(base+0x5474040,global_object);h.write(global_object+0x10,global_object+0x20)
    h.write(global_object+0x20,player);h.write(player,player_vt);h.write(player_vt+0x30,h.ram+0x29020)
    h.write(storage,0);h.write(storage+8,savedata);h.write(storage+0x20,0)
    item_bytes=struct.pack('<HHI',0x1234,0x5678,30)
    events=[];room=True;insert_ok=True;slot=0;record_index=7
    def can_claim():
        assert h.reg(0)==owner+0x1f8 and h.reg(1)==items+slot*12
        events.append('capacity')
        return int(room)
    def insert():
        assert h.reg(0)==owner+0x1f8 and h.reg(1)==items+slot*12
        assert h.reg(2)==1 and h.reg(3)==0
        events.append('insert')
        return int(insert_ok)
    def refresh():
        assert h.reg(0)==grid and h.reg(1)==slot
        events.append('refresh')
        return 0 # No rendered widget in this fixture.
    h.stubs={safe_prepare:lambda:None,base+0x2df1c58:lambda:events.append('menu'),
        base+0x2577890:can_claim,base+0x25738b0:insert,base+0x3765104:refresh,
        base+0x3764fe0:lambda:events.append('focus-restored'),
        h.ram+0x29000:lambda:events.append('browse'),h.ram+0x29010:lambda:events.append('full-message'),
        h.ram+0x29020:lambda:save,
        base+0x3759380:lambda:None, # UI manager only; native close/queue runs.
        h.mod+symbol('SvcLogger','LogRaw'):lambda:None}
    def reset():
        events.clear()
        h.write(name_window+0x1f0,0xffffffffffffffff);h.write(name_window+0x201,0,1)
        h.write(owner+0x1b0,3,4);h.write(owner+0x1f0,0xffffffff,4);h.write(owner+0x1f4,4,4)
        h.write(owner+0x200,0,1);h.write(owner+0x218,slot,4)
        h.u.mem_write(items+slot*12,item_bytes+struct.pack('<I',record_index))
        h.u.mem_write(savedata+0x3c4fc0+record_index*8,item_bytes)
        h.u.mem_write(owner+0x1c1e8,b'\xff'*8)
        h.write(menu+0x26d8,3,4);h.write(menu+0x3a0,99,4);h.write(menu+0x26c9,255,1)
    def transition():
        h.write(owner+0x200,0,1);h.put(19,owner);h.run(base+0x2b0e3c8,base+0x2b0e3f0)
        assert h.read(owner+0x1f0)==0xffffffffffffffff
    def update():
        h.write(owner+0x200,1,1);h.put(0,owner+0x1a8);h.run(base+0x3dbb268)
    for slot in (0,19,59):
        for child_state in (0,1):
            for room,insert_ok in ((False,True),(True,False),(True,True)):
                h.write(menu+0x1b0,child_state,4);reset();transition()
                assert events==[] and h.read(owner+0x1b0,4)==4
                update();success=room and insert_ok
                assert events==(['capacity','insert','refresh','focus-restored'] if success else
                                ['capacity','insert'] if room else ['capacity'])
                assert h.read(owner+0x1f4,4)==(3 if success else 5)
                saved=bytes(h.u.mem_read(savedata+0x3c4fc0+record_index*8,8))
                assert saved==(empty_item if success else item_bytes)
                assert bool(h.read(owner+0x1c1e8+slot//8,1)&(1<<(slot%8))) != success
                assert h.read(menu+0x1b0,4)==child_state and h.read(menu+0x26c9,1)==255
                assert h.read(menu+0x26d8,4)==3 and h.read(menu+0x3a0,4)==99
                assert h.read(name_window+0x201,1)==int(success)
                assert h.read(name_window+0x1f0,4)==(4 if success else 0xffffffff)
                transition();assert events[-1]==('browse' if success else 'full-message')
                cases+=1
    room=insert_ok=True;slot=0
    for guard in ('missing-window','other-window','alternate-phase'):
        reset()
        if guard=='missing-window':h.write(owner+0x220,0)
        if guard=='other-window':h.write(name_window,0)
        if guard=='alternate-phase':h.write(name_window+0x200,1,1)
        transition();update()
        assert h.read(owner+0x1f4,4)==3
        assert h.read(name_window+0x201,1)==int(guard=='alternate-phase')
        if guard=='alternate-phase':assert h.read(name_window+0x1f4,4)==4
        h.write(owner+0x220,name_window);h.write(name_window,base+0x4a01130)
        h.write(name_window+0x200,0,1);cases+=1
    for ptr,width,bad in ((owner,8,0),(owner+0x218,4,60),(owner+0x1c1d8,4,0),
        (owner+0x1c1e0,8,0),(owner+0x1c1e8,1,0),(items,2,0xfffe),
        (menu+0x218,8,0),(menu+0x220,8,0),(menu+0x228,8,0),(menu+0x230,8,1),
        (menu+0x1b0,4,2),(menu+0x1f0,4,2)):
        reset();old=h.read(ptr,width);h.write(ptr,bad,width)
        transition();assert events==['menu']
        assert h.read(name_window+0x201,1)==0
        h.write(ptr,old,width);cases+=1
    for change in ('item','slot','binding','available'):
        reset();transition();assert events==[]
        ptr,width,bad={'item':(items,2,0x5678),'slot':(owner+0x218,4,1),
            'binding':(menu+0x230,8,1),'available':(owner+0x1c1e8,1,0)}[change]
        old=h.read(ptr,width);h.write(ptr,bad,width)
        update();assert events==['menu'];h.write(ptr,old,width);cases+=1
    bank=h.ram+0x2a000;key=bank+32;h.u.mem_write(bank,b'CmnSubWin\0')
    for count,action,caption,flags,extra,callback in ((2,0,b'1602',0,0,False),
        (0,0,b'1602',0,0,False),(1,1,b'1602',0,0,False),(1,0,b'0202',0,0,False),
        (1,0,b'1602',1,0,False),(1,0,b'1602',0,1,False),(1,0,b'1602',0,0,True)):
        def changed_builder():
            desc=h.reg(1);h.write(desc+0x44,count,4)
            h.write(desc+0x48,bank);h.write(desc+0x50,key);h.write(desc+0x58,action,4)
            h.write(desc+0x5c,flags,4);h.write(desc+0x60,extra,4)
            if callback:h.write(desc+0x130,owner)
        h.u.mem_write(key,caption+b'\0');h.stubs[base+0x2defbfc]=changed_builder
        reset();transition();assert events==['menu'];cases+=1
print(f'PASS {cases} recycling Claim cases: native transfer, removal, full pockets, fallback, and first-open lifecycle at two bases.')
