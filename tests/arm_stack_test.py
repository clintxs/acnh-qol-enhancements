"""Run compiled C++ callbacks and original Split in Unicorn, with engine stubs.

Module ELF relocations are applied at two bases. Inline-hook adapters here model
Exlaunch's register save/callback/displaced-instruction contract, not its installer.
"""
from pathlib import Path
import struct
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_PROT_READ, UC_PROT_EXEC
from unicorn.arm64_const import *

ROOT=Path(__file__).resolve().parents[1]
GRAB_FIVE=0x4F550001
GRAB_TEN=0x4F550002
FIVE_CALLBACK=0x4F551001
TEN_CALLBACK=0x4F551002
elf=ROOT.joinpath('project.elf').read_bytes()
raw=ROOT.parent.joinpath('Grab 5/working/main.raw').read_bytes()
u16=lambda b,o:struct.unpack_from('<H',b,o)[0]
u32=lambda b,o:struct.unpack_from('<I',b,o)[0]
u64=lambda b,o:struct.unpack_from('<Q',b,o)[0]
sections=[struct.unpack_from('<IIQQQQIIQQ',elf,u64(elf,40)+i*u16(elf,58)) for i in range(u16(elf,60))]
symbols={}
symbol_sizes={}
symbol_types={}
tables={}
for index,s in enumerate(sections):
    if s[1] not in (2,11):continue
    names=sections[s[6]];strings=elf[names[4]:names[4]+names[5]]
    table=[]
    for offset in range(s[4],s[4]+s[5],s[9]):
        name,info,other,section,value,size=struct.unpack_from('<IBBHQQ',elf,offset)
        name=strings[name:strings.find(b'\0',name)].decode()
        table.append((name,value,section))
        if section and name:
            symbols[name]=value
            symbol_sizes[name]=size
            symbol_types[name]=info&15
    tables[index]=table

def symbol(*needles):
    matches=[v for k,v in symbols.items() if all(n in k for n in needles)]
    assert len(matches)==1,(needles,matches)
    return matches[0]

class Machine:
    def __init__(self,base,withdrawal_hint=False):
        self.base=base;self.mod=base+0x7000000;self.ram=0x710000000
        self.u=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
        self.u.mem_map(base,(len(raw)+4095)&~4095);self.u.mem_write(base,raw)
        image_size=max(va+memsz for typ,flags,off,va,pa,sz,memsz,align in
                       (struct.unpack_from('<IIQQQQQQ',elf,u64(elf,32)+i*u16(elf,54)) for i in range(u16(elf,56))) if typ==1)
        self.u.mem_map(self.mod,(image_size+4095)&~4095)
        self.imports={}
        for i in range(u16(elf,56)):
            typ,flags,off,va,pa,sz,memsz,align=struct.unpack_from('<IIQQQQQQ',elf,u64(elf,32)+i*u16(elf,54))
            if typ==1:self.u.mem_write(self.mod+va,elf[off:off+sz])
        for s in sections:
            if s[1]!=4:continue
            for off in range(s[4],s[4]+s[5],s[9]):
                target,info,addend=struct.unpack_from('<QQq',elf,off)
                typ=info&0xffffffff
                if typ==1027:value=self.mod+addend
                elif typ in (257,1025,1026):
                    name,val,sec=tables[s[6]][info>>32]
                    if sec:value=self.mod+val+addend
                    else:
                        if name not in self.imports:self.imports[name]=self.ram+0x80000+16*len(self.imports)
                        value=self.imports[name]
                else:raise AssertionError(typ)
                self.write(self.mod+target,value)
        self.u.mem_map(self.ram,0x100000);self.sp=self.ram+0xf0000
        self.stop=base+0x100;self.stubs={}
        self.write(self.mod+symbol('s_ModuleBitset'),2)
        self.write(self.mod+symbol('s_ModuleInfos')+72,base)
        self.u.hook_add(UC_HOOK_CODE,self.hook)
        for name,off in [('BuildStackMenu',0x307e7e0),('MapStackAction',0x307d490),
                         ('ResolveStackCaption',0x19f59b0),('ResolveStackPostAction',0x19f5b80),
                         ('EnterStorageCommand',0x316dd58),('UpdateStorageCommand',0x316deec),
                         ('SharedPageInput',0x3c58d24),
                         ('QuickStorePlay',0x308aa80),('QuickStoreCommand',0x308c5f8),
                         ('BuildPocketStorageHint',0x3094fb4)]:
            self.write(self.mod+symbol(name,'s_FnPtr'),base+off)
        self.write(self.mod+symbol('InputHandlers'),self.mod+symbol('15QuickStoreInput'))
        self.adapter(0x3076480,'AdjustSourceCount',0x51000401,0x3076484,0x80000)
        self.adapter(0x30764f0,'AdjustDestinationCount',None,0x30764f4,0x80200,0x19f2bc0)
        self.adapter(0x308c764,'DispatchStackAction',0xd63f0100,0x308c768,0x80400)
        self.adapter(0x1a9bfa0,'ReplaceMenuCaption',0x91002280,0x1a9bfa4,0x80600)
        if withdrawal_hint:
            self.adapter(0x316f490,'RenameStorageSelectHint',None,0x316f494,0x80800,0x2de2b00)
        # Loader writes complete. Keep both module and main executable text RX.
        self.u.mem_protect(base,0x3f07000,UC_PROT_READ|UC_PROT_EXEC)
        for i in range(u16(elf,56)):
            typ,flags,off,va,pa,sz,memsz,align=struct.unpack_from('<IIQQQQQQ',elf,u64(elf,32)+i*u16(elf,54))
            if typ==1 and flags&1:self.u.mem_protect(self.mod+va,(memsz+4095)&~4095,UC_PROT_READ|UC_PROT_EXEC)
        self.u.mem_protect(self.mod+0x80000,0x1000,UC_PROT_READ|UC_PROT_EXEC)
    def read(self,p,n=8):return int.from_bytes(self.u.mem_read(p,n),'little')
    def write(self,p,v,n=8):self.u.mem_write(p,int(v).to_bytes(n,'little'))
    def reg(self,i):return self.u.reg_read(UC_ARM64_REG_X0+i) if i<29 else self.u.reg_read((UC_ARM64_REG_FP,UC_ARM64_REG_LR)[i-29])
    def put(self,i,v):self.u.reg_write(UC_ARM64_REG_X0+i if i<29 else (UC_ARM64_REG_FP,UC_ARM64_REG_LR)[i-29],v)
    def hook(self,u,pc,size,_):
        if pc in self.stubs:
            value=self.stubs[pc]()
            if value is not None:self.put(0,value)
            u.reg_write(UC_ARM64_REG_PC,self.reg(30))
    def callback(self,name):
        matches=[v for k,v in symbols.items() if name in k and 'Callback' in k and symbol_types[k]==2]
        assert len(matches)==1,(name,matches)
        return self.mod+matches[0]
    def run(self,start,end=None):
        self.u.reg_write(UC_ARM64_REG_SP,self.sp);self.put(30,self.stop)
        self.u.emu_start(start,self.stop if end is None else end,count=1000000)
        assert self.u.reg_read(UC_ARM64_REG_PC)==(self.stop if end is None else end),'execution did not return'
        if end is None:assert self.u.reg_read(UC_ARM64_REG_SP)==self.sp
    def adapter(self,site,name,displaced,resume,offset,call=None):
        start=self.mod+offset;code=[]
        def branch(src,dst,link=False):
            delta=(dst-src)//4;assert -(1<<25)<=delta<(1<<25)
            return (0x94000000 if link else 0x14000000)|(delta&0x3ffffff)
        code.append(0xd10403ff) # sub sp,sp,#0x100
        for r in range(0,30,2):code.append(0xa9000000|((r//2)*2<<15)|((r+1)<<10)|(31<<5)|r)
        code.extend([0xf9007bfe,0x910003e0]) # str lr,[sp,#240]; mov x0,sp
        code.append(branch(start+4*len(code),self.callback(name),True))
        for r in range(0,30,2):code.append(0xa9400000|((r//2)*2<<15)|((r+1)<<10)|(31<<5)|r)
        code.extend([0xf9407bfe,0x910403ff])
        if displaced is not None:code.append(displaced)
        if call is not None:code.append(branch(start+4*len(code),self.base+call,True))
        code.append(branch(start+4*len(code),self.base+resume))
        self.u.mem_write(start,struct.pack('<'+'I'*len(code),*code))
        self.write(self.base+site,branch(self.base+site,start),4)

cases=0
for base in (0x80000000,0x1234500000):
    h=Machine(base);menu=h.ram+0x100;context=h.ram+0x200;item=h.ram+0x300
    h.write(context,item);h.write(context+8,h.ram+0x400)
    for actions in ([35,46],[46],[35,46,22],[35,47],[],[0,1,2,3,46],[0,1,2,3,4,46],[0,1,2,3,4,5,46]):
        for count in (5,6,10,11,30):
            for free in (False,True):
                def builder():
                    for i,a in enumerate(actions):h.write(menu+4*i,a,4)
                    h.write(menu+28,len(actions),4)
                h.stubs={base+0x307e7e0:builder,base+0x19f02d0:lambda:count,base+0x2572fb0:lambda:int(free)}
                h.put(0,menu);h.put(1,context);h.run(h.callback('BuildStackMenu'))
                expected=list(actions)
                if 46 in actions and count>5 and free and len(actions)<7:
                    expected[actions.index(46)+1:actions.index(46)+1]=[GRAB_FIVE,GRAB_TEN] if count>10 and len(actions)<=5 else [GRAB_FIVE]
                assert [h.read(menu+i*4,4) for i in range(h.read(menu+28,4))]==expected
                cases+=1
    for action in (46,47,63,66,67,68,69,GRAB_FIVE,GRAB_TEN,0x4F550003):
        h.stubs={base+0x307d490:lambda:123,base+0x19f5b80:lambda:h.reg(0)}
        h.put(0,action);h.run(h.callback('MapStackAction'));assert h.reg(0)==({GRAB_FIVE:FIVE_CALLBACK,GRAB_TEN:TEN_CALLBACK}.get(action,123));cases+=1
        h.put(0,action);h.run(h.callback('ResolveStackPostAction'));assert h.reg(0)==(46 if action in (GRAB_FIVE,GRAB_TEN) else action);cases+=1
    h.write(base+0x4ab2020,h.ram+0x700)
    def caption():
        h.write(h.reg(8),h.ram+0x710);h.write(h.reg(8)+8,h.reg(0))
    h.stubs={base+0x19f59b0:caption}
    for action in (46,47,66,67,68,69,GRAB_FIVE,GRAB_TEN,0x4F550003):
        h.put(0,action);h.put(8,h.ram+0x600);h.run(h.callback('ResolveStackCaption'))
        assert h.read(h.ram+0x600)==h.ram+0x710
        value=h.read(h.ram+0x608)
        assert value==({GRAB_FIVE:46,GRAB_TEN:47}.get(action,action))
        cases+=1
    # Dispatch inline callback updates the saved x0/x8 only for private callbacks.
    saved=h.ram+0x800;frame=h.ram+0x900
    for callback in (0,24,59,60,FIVE_CALLBACK,TEN_CALLBACK,0x4F551003,0xffffffff):
        h.write(saved+29*8,frame);h.write(frame+28,callback,4)
        h.write(saved+19*8,h.ram+0x1000);h.write(saved,0x1234);h.write(saved+8*8,0x5678)
        h.put(0,saved);h.run(h.callback('DispatchStackAction'))
        if callback in (FIVE_CALLBACK,TEN_CALLBACK):
            assert h.read(saved)==h.ram+0x1000
            assert h.read(saved+64)==h.mod+symbol('SplitFive' if callback==FIVE_CALLBACK else 'SplitTen')
        else:assert h.read(saved)==0x1234 and h.read(saved+64)==0x5678
        cases+=1
    # Execute the ORIGINAL bounds check/table selection before the inline hook.
    # The pristine image has unrelocated pointers, so relocate fallback entry 0.
    assert h.read(base+0x4a06740)==0x30742d0
    assert h.read(base+0x4a06748)==0
    h.write(base+0x4a06740,base+0x30742d0)
    for action,name in ((GRAB_FIVE,'SplitFive'),(GRAB_TEN,'SplitTen')):
        h.stubs={}
        h.put(0,action);h.run(h.callback('MapStackAction'))
        token=h.reg(0)
        assert token==(FIVE_CALLBACK if action==GRAB_FIVE else TEN_CALLBACK)
        called=[]
        def selected_handler():
            assert h.reg(0)==h.ram+0x1000
            called.append(name)
            return 0 # Follow the original failure-return branch to the epilogue.
        def wrong_handler():
            raise AssertionError('Vanilla fallback must never be called for our token')
        h.stubs={h.mod+symbol(name):selected_handler,base+0x30742d0:wrong_handler}
        h.put(0,token);h.put(19,h.ram+0x1000);h.put(29,frame)
        h.run(base+0x308c72c,base+0x308c718)
        assert h.read(frame+28,4)==token and called==[name]
        cases+=1
    # Real compiled file loader/parser and text hook, filesystem/locale calls stubbed.
    contents=ROOT.joinpath('modules/Pockets/romfs/qol-enhancements/messages/pockets.ini').read_bytes()
    row=h.ram+0x5000;text_frame=h.ram+0x7000;buffer=h.ram+0x9000
    catalog_symbol=next(k for k in symbols if k.endswith('8MessagesE'))
    locale=1;opens=0;closes=0;failure='';logs=[]
    def open_file():
        global opens
        opens+=1
        path=bytes(h.u.mem_read(h.reg(1),80)).split(b'\0')[0]
        assert path==b'content:/qol-enhancements/messages/pockets.ini' and h.reg(2)==1
        if failure=='missing':return 1
        h.write(h.reg(0),0x123);return 0
    def file_size():
        assert h.reg(1)==0x123
        h.write(h.reg(0),16385 if failure=='oversize' else len(contents));return 0
    def read_file():
        assert h.reg(0)==0x123 and h.reg(1)==0 and h.reg(3)==len(contents)
        if failure=='read':return 1
        h.u.mem_write(h.reg(2),contents);return 0
    def close_file():
        global closes
        assert h.reg(0)==0x123
        closes+=1
    def log_message():
        logs.append(bytes(h.u.mem_read(h.reg(2),h.reg(1))).decode()) # string_view: length, pointer.
    h.stubs={base+0x37cefc4:lambda:locale,h.mod+symbol('SvcLogger','LogRaw'):log_message}
    for name,address in h.imports.items():
        if '8OpenFile' in name:h.stubs[address]=open_file
        elif '11GetFileSize' in name:h.stubs[address]=file_size
        elif '8ReadFile' in name:h.stubs[address]=read_file
        elif '9CloseFile' in name:h.stubs[address]=close_file
    def reset_catalog():
        h.write(h.mod+symbol('CatalogAttempted'),0,1)
        h.u.mem_write(h.mod+symbols[catalog_symbol],bytes(symbol_sizes[catalog_symbol]))
    def replace_text(action,expected,capacity=0x800):
        h.write(row+0x168,action,4)
        h.write(text_frame-0xf0+8,buffer);h.write(text_frame-0xf0+16,capacity,4)
        h.u.mem_write(buffer,('Vanilla caption\0').encode('utf-16-le'))
        h.put(19,row);h.put(29,text_frame);h.put(20,h.ram+0xb000)
        h.run(base+0x1a9bfa0,base+0x1a9bfa4)
        assert h.reg(0)==h.ram+0xb008 # displaced ADD x0,x20,#8 still executed.
        actual=bytes(h.u.mem_read(buffer,256)).decode('utf-16-le').split('\0')[0]
        assert actual==expected,(actual,expected)
    for action in (46,47,68,69,0x4F550003):
        replace_text(action,'Vanilla caption');cases+=1
    assert opens==0
    for locale,expected in ((1,'Grab 5'),(0,'5コわける'),(12,'Взять 5'),(14,'分出5个'),(999,'Grab 5')):
        replace_text(GRAB_FIVE,expected);cases+=1
    locale=1
    replace_text(GRAB_TEN,'Grab 10');cases+=1
    replace_text(GRAB_FIVE,'Vanilla caption',16);cases+=1
    assert opens==closes==1 and any('TEXT_READY' in log for log in logs)
    for failure in ('missing','read','oversize','invalid'):
        reset_catalog();opens=closes=0;logs=[]
        if failure=='invalid':contents=b'[USen]\nbroken line'
        replace_text(GRAB_FIVE,'Grab 5');replace_text(GRAB_TEN,'Grab 10');cases+=2
        assert opens==1 and closes==(0 if failure=='missing' else 1)
        assert any('TEXT_FALLBACK' in log for log in logs)
    controller=h.ram+0x1000;selected=h.ram+0x30000;global_slot=h.ram+0x30100
    h.write(controller+0x1c514,2,1);h.write(base+0x4ab43c0,global_slot);h.write(global_slot,0)
    h.write(base+0x4ac0510,h.ram+0x30200)
    for quantity in (1,5,10):
        for count in (6,10,11,30,99):
            if count<=quantity:continue
            for first_free in (0,3,39):
                h.u.mem_write(selected,struct.pack('<II',0xabcd1234,count));result={}
                def setter():h.write(h.reg(0)+12,h.reg(1),4)
                def remainder():
                    assert h.reg(0)==controller+0x1f8 and h.reg(1)==2 and h.reg(3)==2
                    result['source']=bytes(h.u.mem_read(h.reg(2),8))
                def insert():
                    assert h.reg(0)==controller and h.reg(2)==first_free
                    result['destination']=bytes(h.u.mem_read(h.reg(1),8))
                h.stubs={base+0x2572e00:lambda:40,base+0x2577ea0:lambda:selected,
                         base+0x267d6c4:lambda:0,base+0x19f02d0:lambda:h.read(h.reg(0)+12,4),
                         base+0x19f2bc0:setter,base+0x2574700:remainder,
                         base+0x2577c70:lambda:first_free,base+0x3095900:insert}
                h.put(0,controller)
                for i in range(19,30):h.put(i,0x100+i)
                h.run(base+0x3076348 if quantity==1 else h.mod+symbol('SplitFive' if quantity==5 else 'SplitTen'))
                assert result=={'source':struct.pack('<II',0xabcd1234,count-quantity),'destination':struct.pack('<II',0xabcd1234,quantity)},result
                for i in range(19,30):assert h.reg(i)==0x100+i
                assert h.reg(0)==1
                cases+=1
    # Storage shortcut: compiled hook + ORIGINAL menu builder + ORIGINAL confirmed
    # selection processor. Only inventory/catalog/refresh engine services are stubbed.
    h.u.mem_map(h.ram+0x100000,0x100000)
    storage=h.ram+0x1000;storage_menu=h.ram+0xb1000;page=h.ram+0xb5000
    grid=h.ram+0xb6000;engine=h.ram+0x110000;metadata=h.ram+0x170000
    safe_vtable=base+u64(raw,0x4ab2020)+16
    h.write(base+0x4ab2020,safe_vtable-16)
    safe_prepare=base+u64(raw,u64(raw,0x4ab2020)+16+24)
    h.write(safe_vtable+24,safe_prepare)
    for offset in (0,8,16):h.write(base+0x49c7208+offset,base+u64(raw,0x49c7208+offset))
    h.write(storage,base+0x4a16028);h.write(storage+0x220,0,4)
    h.write(storage+0x1b0,5,4);h.write(storage+0x200,0,1)
    h.write(storage+0xaa9d0,storage_menu);h.write(storage+0x210,page)
    h.write(page+0x588,grid)
    h.write(grid+0x2e8,10,4);h.write(grid+0x430,2,4);h.write(grid+0x438,0,4)
    h.write(grid+0x3e8,0,1);h.write(grid+0x3cc,100,4)
    h.write(storage_menu+0x1b0,1,4);h.write(storage_menu+0x218,base+0x4a164e8)
    h.write(storage_menu+0x220,storage);h.write(storage_menu+0x228,base+0x316b2dc)
    h.write(storage_menu+0x230,0)
    h.write(storage_menu+0x1f0,0xffffffff,4);h.write(storage_menu+0x1f4,0xffffffff,4)
    h.write(base+0x4ab43c0,h.ram+0x180000);h.write(h.ram+0x180000,engine)
    h.write(engine+0x57e58,metadata);h.write(metadata+0x1f8,0,2)
    h.write(metadata+0x1d8,1,4);h.write(metadata+0x1e0,metadata+0x300)
    h.write(metadata+0x300,metadata+0x400)
    events=[];can_move=True;selected_slot=0
    item_bytes=struct.pack('<II',0xabcd1234,30)
    def normal_menu():
        assert h.reg(0)==storage
        events.append('menu')
    def selected_storage_item():
        assert h.reg(0)==storage+0x228 and h.reg(1)==20+selected_slot
        # 12-byte item descriptor is returned indirectly through x8.
        h.u.mem_write(h.reg(8),struct.pack('<I',0)+item_bytes)
        events.append('lookup')
    def move_to_pockets():
        assert h.reg(0)==storage+0x1f8 and h.reg(2)==1 and h.reg(3)==0
        assert bytes(h.u.mem_read(h.reg(1),8))==item_bytes
        events.append('move')
        return int(can_move)
    def remove_from_storage():
        assert h.reg(0)==storage and h.reg(1)==20+selected_slot
        events.append('remove-refresh')
    h.stubs={base+0x316dd58:normal_menu,safe_prepare:lambda:None,
             h.mod+symbol('SvcLogger','LogRaw'):lambda:None,
             base+0x3164a78:selected_storage_item,base+0x267d6c4:lambda:0,
             base+0x25738b0:move_to_pockets,base+0x3170c64:remove_from_storage}
    # Real state dispatcher and pending-transition consumer, with UI rendering
    # boundaries mocked. Unlike the original tests, check entry -> update ->
    # next-state entry, not merely that the processor wrote a pending integer.
    state_table=h.ram+0x190000
    h.write(storage+0x1d0,13,4);h.write(storage+0x1d8,state_table)
    for off in (0,8,16):h.write(base+0x4a16510+off,base+u64(raw,0x4a16510+off))
    for state,entry,update in ((3,base+0x316cd9c,0),(8,base+0x316e0d0,0),
                              (5,h.callback('EnterStorageCommand'),h.callback('UpdateStorageCommand'))):
        record=state_table+state*0x40
        h.write(record,base+0x4a16510);h.write(record+8,storage)
        h.write(record+0x10,entry);h.write(record+0x20,update)
    h.stubs[base+0x316cd9c]=lambda:events.append('play-enter')
    h.stubs[base+0x316e0d0]=lambda:events.append('error-enter')
    def reset_storage_result():
        events.clear()
        h.write(storage+0x1b0,5,4)
        h.write(storage+0x1f0,0xffffffff,4);h.write(storage+0xaa9e9,selected_slot,1)
        h.write(storage_menu+0x26d8,4,4);h.write(storage_menu+0x3a0,99,4)
        h.write(storage_menu+0x26c9,2,1)
    for selected_slot in (0,1,9,17):
        for initial_state,can_move in ((0,False),(0,True),(1,False),(1,True)):
            h.write(storage_menu+0x1b0,initial_state,4)
            reset_storage_result()
            # Selection originates in the update phase (slot 1); entry runs
            # after +0x200 has returned to slot 0, before both are cleared.
            h.write(storage+0x1b0,3,4);h.write(storage+0x1f4,5,4)
            h.put(19,storage);h.run(base+0x2b0e3c8,base+0x2b0e3f0)
            assert events==[] # Entry must neither transfer nor start the popup.
            assert h.read(storage+0x1f0)==0xffffffffffffffff
            h.write(storage+0x200,1,1)
            h.put(0,storage+0x1a8);h.run(base+0x3dbb268)
            assert events==(['lookup','move','remove-refresh'] if can_move else ['lookup','move'])
            assert h.read(storage+0x1f4,4)==(3 if can_move else 8)
            if not can_move:assert h.read(storage+0xaa9f8,4)==0x4a
            assert h.read(storage_menu+0x26d8,4)==4 and h.read(storage_menu+0x3a0,4)==99
            assert h.read(storage_menu+0x26c9,1)==0xff and h.read(storage_menu+0x1b0,4)==initial_state
            before=list(events)
            h.put(0,storage);h.run(h.callback('UpdateStorageCommand')) # Must not repeat it.
            assert events==before
            h.write(storage+0x200,0,1)
            h.put(19,storage);h.run(base+0x2b0e3c8,base+0x2b0e3f0)
            assert h.read(storage+0x1b0,4)==(3 if can_move else 8)
            assert events==before+['play-enter' if can_move else 'error-enter']
            assert h.read(storage+0x1f0)==0xffffffffffffffff
            cases+=1
    for ptr,width,bad in ((storage,8,0),(storage+0x220,4,1),(storage+0x220,4,2),
                          (storage+0x1b0,4,3),(storage+0x1f0,4,8),(storage+0x200,1,2),
                          (storage+0xaa9e9,1,255),(storage+0xaa9d0,8,0),
                          (storage_menu+0x1b0,4,3),(storage_menu+0x1f0,4,2),
                          (storage_menu+0x1f4,4,2),(storage_menu+0x218,8,0),
                          (storage_menu+0x220,8,0),(storage_menu+0x228,8,0),(storage_menu+0x230,8,1)):
        reset_storage_result();old=h.read(ptr,width);h.write(ptr,bad,width)
        h.put(0,storage);h.run(h.callback('EnterStorageCommand'))
        assert events==['menu'] and h.read(storage_menu+0x26c9,1)==2
        h.write(ptr,old,width);cases+=1
    # If the binding changes between entry and update, open the vanilla menu
    # without touching inventory; consuming the token prevents a later retry.
    reset_storage_result();h.put(0,storage);h.run(h.callback('EnterStorageCommand'))
    h.write(storage_menu+0x230,1)
    h.put(0,storage);h.run(h.callback('UpdateStorageCommand'))
    assert events==['menu']
    h.write(storage_menu+0x230,0);h.write(storage_menu+0x26c9,0xff,1)
    h.put(0,storage);h.run(h.callback('UpdateStorageCommand'))
    assert events==['menu'];cases+=1
    for count,action,bank,key,flags,extra in ((0,0,True,True,0,0),(2,0,True,True,0,0),
                                            (1,1,True,True,0,0),(1,0,False,True,0,0),
                                            (1,0,True,False,0,0),(1,0,True,True,1,0),
                                            (1,0,True,True,0,1)):
        def changed_builder():
            desc=h.reg(1)
            h.write(desc+0x44,count,4)
            h.write(desc+0x48,base+0x3fb5ed4 if bank else 0)
            h.write(desc+0x50,base+0x3fc6534 if key else 0)
            h.write(desc+0x58,action,4);h.write(desc+0x5c,flags,4);h.write(desc+0x60,extra,4)
        h.stubs[base+0x316b2dc]=changed_builder
        reset_storage_result();h.put(0,storage);h.run(h.callback('EnterStorageCommand'))
        assert events==['menu'] and h.read(storage_menu+0x26c9,1)==2
        cases+=1
    # Y shortcut: execute compiled hooks plus ORIGINAL state transition, command
    # entry/update, PutInStorage callback and refresh update. Stub only engine/UI
    # boundaries and menu eligibility; these cannot substitute for an Eden test.
    pocket=h.ram+0x20000;page=h.ram+0x5000;child=h.ram+0x50000
    ctx=h.ram+0x9000;binding=h.ram+0xa000;hint=h.ram+0xb000;unit=h.ram+0xc000
    snapshot=h.ram+0xd000;rows=h.ram+0xe000;item=h.ram+0xf000
    app=h.ram+0x60000;masks=h.ram+0x61000;helper=h.ram+0x62000
    helper_vt=h.ram+0x63000;states=h.ram+0x65000
    focus=h.ram+0x68000;focus_rows=h.ram+0x69000;focus_controller=h.ram+0x6a000
    widgets=h.ram+0x70000;panes=h.ram+0x7f000
    menu_page=h.ram+0x78000;menu_layouts=h.ram+0x79000
    menu_layout=h.ram+0x7a000;menu_animation=h.ram+0x7b000
    opening_event=h.ram+0x7c000;event_queue=h.ram+0x7d000
    for p,n in ((pocket,0x1c600),(page,0x800),(child,0x3000),(ctx,0x100),
                (binding,0x100),(hint,0x100),(unit,0x100),(snapshot,0x30),
                (rows,0x240),(app,0x500),(masks,0x100),(helper,0x100),
                (helper_vt,0x100),(states,0x800)):
        h.u.mem_write(p,bytes(n))
    h.write(pocket,base+0x4a06e28);h.write(pocket+0x210,page);h.write(pocket+0x218,child)
    h.write(page,base+0x4a06b00);h.write(page+0x210,ctx)
    h.u.mem_write(focus,bytes(0x100));h.u.mem_write(focus_controller,bytes(0x80))
    h.write(ctx+0x48,focus);h.write(focus+0x18,2,4);h.write(focus+0x20,focus_rows)
    h.write(focus_rows,focus_controller);h.write(focus_rows+8,focus_controller+0x40)
    for slot in range(40):
        widget=widgets+slot*0x230
        h.u.mem_write(widget,bytes(0x230));h.write(page+0x590+8*slot,widget)
        h.write(widget+0x218,panes+16*slot)
    h.write(page+0x468,1,4);h.write(page+0x46c,4,4);h.write(page+0x470,binding)
    h.write(binding,base+0x4a070d8);h.write(binding+8,pocket);h.write(binding+0x10,base+0x3088724)
    h.write(page+0x570,hint);h.write(hint+0xa0,unit);h.write(unit+0x14,5,4)
    h.write(snapshot+8,2,4);h.write(snapshot+0x10,rows)
    h.u.mem_map(base+0x59f8000,0x1000)
    h.write(base+0x59f83f0,app);h.write(app+0x418,masks);h.write(ctx+0x58,1,4)
    h.write(child+0x218,base+0x4a071c8);h.write(child+0x220,pocket)
    h.write(child+0x228,base+0x3088df0);h.write(child+0x1f0,0xffffffffffffffff)
    for p,n in ((menu_page,0x400),(menu_layout,0x200),(menu_animation,0x40),
                (opening_event,0x60),(event_queue,0x60)):
        h.u.mem_write(p,bytes(n))
    h.write(child+0x210,menu_page);h.write(menu_page+0x3f0,1,4)
    h.write(menu_page+0x3f8,menu_layouts);h.write(menu_layouts,menu_layout)
    h.write(menu_layout+0x1b0,menu_animation);h.write(menu_animation+8,menu_layout)
    h.write(menu_animation+0x28,opening_event)
    h.write(pocket+0x258,helper);h.write(helper,helper_vt);h.write(helper_vt+0xb0,h.ram+0x64000)
    h.write(pocket+0x1d0,15,4);h.write(pocket+0x1d8,states)
    for state,entry,update in ((3,h.callback('QuickStorePlay'),0),
                              (7,base+0x308c5c4,h.callback('QuickStoreCommand')),
                              (12,h.ram+0x64010,0),(14,base+0x308fb44,base+0x308fb60)):
        record=states+state*0x40
        h.write(record,base+0x4a16510);h.write(record+8,pocket)
        h.write(record+0x10,entry);h.write(record+0x20,update)
    h.write(base+0x4a06740+29*16,base+0x3076d78)
    own_home=True;eligible=True;room=True;capacity=40;selected_slot=9
    events=[];transferred=[];native_inputs=[];native_consume=0xffffffff;quick_logs=[];confirmations=[]
    ui_global=h.ram+0x84900;ui_app=h.ram+0x85000;cursor=h.ram+0x86000
    cursor_page=h.ram+0x87000;cursor_layout=h.ram+0x88000;cursor_animation=h.ram+0x89000
    animation_interface=h.ram+0x8a000;animation_impl=h.ram+0x8b000;animation_target=h.ram+0x8c000
    h.write(base+0x4ab2230,ui_global);h.write(ui_global,ui_app);h.write(ui_app+0xd0,cursor)
    h.write(cursor,base+0x4a0ea80);h.write(cursor+0x210,cursor_page)
    h.write(cursor_page+0x570,cursor_layout);h.write(cursor_layout+0x208,cursor_animation)
    h.write(cursor_animation,animation_interface);h.write(cursor_animation+8,animation_target)
    h.write(cursor_animation+0x18,animation_impl)
    h.write(animation_interface+0x18,h.ram+0x8d000);h.write(animation_impl+0x190,h.ram+0x8d010)
    h.write(base+0x4ac4e68,base+u64(raw,0x4ac4e68))
    taps=[];tap_prepares=[]
    def prepare_tap():
        assert h.reg(0)==cursor_animation and h.read(h.reg(1)+8,4)==1
        assert h.read(h.reg(1))==base+u64(raw,0x4ac4e68)+16
        assert events[-1]=='play'
        tap_prepares.append(selected_slot)
    def start_tap():
        assert h.reg(0)==animation_target
        assert h.u.reg_read(UC_ARM64_REG_S0)==0x3f800000
        assert events[-1]=='play'
        taps.append(selected_slot)
    item_bytes=struct.pack('<HHI',0x1234,0x5678,30)
    def pocket_menu():
        assert h.reg(0)==pocket and h.read(pocket+0x1c514,1)==selected_slot
        desc=h.reg(1)
        h.write(desc+0x40,0x28,4) # Real builder's nonzero default is not rejection.
        h.write(desc+0x44,3,4)
        for i,a in enumerate((35,49 if eligible else 46,22)):
            h.write(desc+0x48+i*32+16,a,4)
    def pocket_item():
        assert h.reg(0)==pocket+0x1f8 and h.reg(1)==selected_slot
        return item
    def store_item():
        assert h.reg(0)==pocket+0x1f8
        assert bytes(h.u.mem_read(h.reg(1),8))==item_bytes
        events.append('store')
        if room:transferred.append(bytes(h.u.mem_read(h.reg(1),8)))
        return int(room)
    def remove_item():
        assert h.reg(0)==pocket and h.reg(1)==selected_slot
        events.append('remove-refresh');h.write(item,0xfffe)
        h.write(widgets+selected_slot*0x230+0x2c3,0,1)
        h.write(widgets+selected_slot*0x230+0x315,0,1)
    def enter_play():
        events.append('play');h.write(ctx+0x54,0,1);h.write(pocket+0x1c514,255,1)
    def native_input():
        native_inputs.append((h.reg(0),h.reg(1),h.reg(2)))
        if native_consume:h.write(rows+0x3c,native_consume,4)
    def confirmation():
        assert h.reg(0)==widgets+selected_slot*0x230
        assert h.read(h.reg(1))==safe_vtable
        assert bytes(h.u.mem_read(h.read(h.reg(1)+8),7))==b'Decide\0'
        assert h.reg(2)==opening_event
        assert h.read(menu_page+0x40,4)==0 # Native valid-item value; empty/disabled yields 1.
        assert h.read(menu_layout+0x40,4)==h.read(unit+0x10,4)
        assert events[-1]=='remove-refresh' and h.read(pocket+0x1c50c,4)==0
        confirmations.append(selected_slot)
    h.stubs={base+0x3c58d24:native_input,base+0x308aa80:enter_play,
             base+0x2572e00:lambda:capacity,base+0x2815598:lambda:int(own_home),
             base+0x2577ea0:pocket_item,base+0x3088df0:pocket_menu,
             base+0x257d630:store_item,base+0x30956f8:remove_item,
             base+0x307d490:lambda:29,base+0x19f5b80:lambda:0,
             base+0x242ffe8:lambda:1,h.ram+0x64000:lambda:events.append('helper-close'),
             h.ram+0x64010:lambda:events.append('error'),
             base+0x3c66950:confirmation,
             h.ram+0x8d000:prepare_tap,h.ram+0x8d010:start_tap,
             h.mod+symbol('SvcLogger','LogRaw'):lambda:quick_logs.append(bytes(h.u.mem_read(h.reg(2),h.reg(1))).decode())}
    def reset_pocket():
        h.write(cursor+0x218,panes+16*selected_slot)
        h.write(pocket+0x1b0,3,4);h.write(pocket+0x1f0,0xffffffffffffffff)
        h.write(pocket+0x200,0,1);h.write(pocket+0x1c50c,0,4)
        h.write(pocket+0x1c516,255,1);h.write(pocket+0x1c518,255,1)
        h.write(focus_controller+0x10,panes+16*selected_slot)
        h.write(focus_controller+0x50,panes+16*selected_slot)
        h.write(child+0x26d8,4,4);h.write(child+0x3a0,99,4);h.write(child+0x26c9,255,1)
        h.u.mem_write(item,item_bytes);h.u.mem_write(rows,bytes(0x240))
        widget=widgets+selected_slot*0x230
        h.write(widget+0x2c3,1,1);h.write(widget+0x315,0,1)
        h.write(widget+0x2a0,menu_page);h.write(widget+0x2a8,menu_layout)
        h.write(widget+0x298,opening_event)
        h.write(menu_page+0x40,1,4);h.write(menu_layout+0x40,99,4)
        h.put(0,pocket);h.run(h.callback('QuickStorePlay'))
        events.clear();transferred.clear();confirmations.clear();taps.clear();tap_prepares.clear()
    def y_input(buttons=0,blocked=0,consumed=0,row=0):
        h.write(rows+row*0x48,buttons,4);h.write(rows+row*0x48+0x3c,consumed,4)
        h.put(0,page);h.put(1,snapshot);h.put(2,blocked);h.run(h.callback('SharedPageInput'))
        assert native_inputs[-1]==(page,snapshot,blocked)
    def transition():
        h.write(pocket+0x200,0,1);h.put(19,pocket)
        h.run(base+0x2b0e3c8,base+0x2b0e3f0)
        assert h.read(pocket+0x1f0)==0xffffffffffffffff
    def update():
        h.write(pocket+0x200,1,1);h.put(0,pocket+0x1a8);h.run(base+0x3dbb268)
    for selected_slot in (0,9,39):
        for child_state in (0,1):
            for room in (True,False):
                h.write(child+0x1b0,child_state,4);reset_pocket()
                y_input();y_input(0x10080)
                assert h.read(pocket+0x1f0,4)==7 and h.read(rows+0x3c,4)==0xffffffff
                assert events==[] and bytes(h.u.mem_read(item,8))==item_bytes
                y_input(0x10080);transition()
                assert events==['helper-close'] and h.read(ctx+0x54,1)==1
                update()
                assert events==(['helper-close','store','remove-refresh'] if room else ['helper-close','store'])
                assert transferred==([item_bytes] if room else [])
                assert confirmations==([selected_slot] if room else [])
                assert taps==[] # Wait for native refresh and return to browsing.
                assert h.read(pocket+0x1f4,4)==(14 if room else 12)
                assert h.read(child+0x26c9,1)==255 and h.read(child+0x26d8,4)==4
                assert h.read(child+0x3a0,4)==99 and h.read(child+0x1b0,4)==child_state
                before=list(events);update();assert events==before # No repeated transfer.
                assert confirmations==([selected_slot] if room else [])
                transition()
                assert h.read(pocket+0x1b0,4)==(14 if room else 12)
                if room:
                    update();assert h.read(pocket+0x1f4,4)==3
                    transition();assert h.read(ctx+0x54,1)==0 and events[-1]=='play'
                    assert taps==tap_prepares==[selected_slot]
                    h.put(0,pocket);h.run(h.callback('QuickStorePlay'))
                    assert taps==[selected_slot] # Consumed once, no repeated click.
                    y_input(0x10080);y_input(0x10080)
                    assert h.read(pocket+0x1f0)==0xffffffffffffffff
                else:
                    assert h.read(pocket+0x1c50c,4)==0x27 and events[-1]=='error'
                    assert taps==[]
                cases+=1
    room=True;selected_slot=9
    for ptr,width,bad in ((pocket+0x1c4fc,4,1),(pocket+0x1c4fc,4,8),
        (pocket+0x1b0,4,5),(pocket+0x1f4,4,4),(pocket+0x200,1,2),
        (pocket+0x1b0,4,8),(pocket+0x1b0,4,9),
        (pocket+0x1c4f0,8,1<<9),
        (pocket+0x1c50c,4,39),(focus_controller+0x10,8,0),(focus_controller+0x10,8,panes+40*16),
        (pocket+0x1c51a,2,4),(child+0x1b0,4,3),(child+0x1f4,4,2),
        (child+0x218,8,0),(child+0x220,8,0),(child+0x228,8,0),(child+0x230,8,1),
        (ctx+0x54,1,1),(masks+0x10,4,1),(binding+0x10,8,0),
        (unit+0x14,4,6),(unit+0x10,4,8),(snapshot+8,4,9),(item,2,0xfffe)):
        reset_pocket();y_input();old=h.read(ptr,width);h.write(ptr,bad,width)
        y_input(0x10080);assert h.read(pocket+0x1f0,4)==0xffffffff and events==[] and not confirmations
        h.write(ptr,old,width);cases+=1
    for setting in ('outside','ineligible','capacity','blocked','consumed','upstream-consumed','A','B','X','direction'):
        reset_pocket();y_input()
        own_home=setting!='outside';eligible=setting!='ineligible';capacity=9 if setting=='capacity' else 40
        y_input(0x10080|{'A':16,'B':32,'X':64,'direction':1}.get(setting,0),
                blocked=int(setting=='blocked'),consumed=0xffffffff if setting=='upstream-consumed' else (128 if setting=='consumed' else 0))
        assert h.read(pocket+0x1f0,4)==0xffffffff and events==[]
        own_home=True;eligible=True;capacity=40
        y_input(0x10080) # Removing block while still held must NOT activate.
        assert h.read(pocket+0x1f0,4)==0xffffffff;cases+=1
    reset_pocket();y_input(0x10080);y_input(0x10080)
    assert h.read(pocket+0x1f0,4)==0xffffffff # First/opening sample held.
    y_input();y_input(0x10080);assert h.read(pocket+0x1f0,4)==7;cases+=1
    for change in ('identity','eligibility','home','binding','selection'):
        reset_pocket();y_input();y_input(0x10080);transition()
        if change=='identity':h.write(item,0x4321,2)
        if change=='eligibility':eligible=False
        if change=='home':own_home=False
        if change=='binding':h.write(child+0x230,1)
        if change=='selection':h.write(pocket+0x1c514,10,1)
        update();assert events==['helper-close'] and h.read(pocket+0x1f4,4)==3
        transition();assert events[-1]=='play' and not transferred and not confirmations
        own_home=True;eligible=True;h.write(child+0x230,0);cases+=1
    reset_pocket();h.write(unit+0x10,1,4);y_input(row=1)
    y_input(0x10080,row=0);assert h.read(pocket+0x1f0,4)==0xffffffff
    y_input(0x10080,row=1);assert h.read(pocket+0x1f0,4)==7;cases+=1
    assert any('QUICK_STORE_TRACE binding owner=' in line for line in quick_logs)
    assert any('eligibility=not-own-home' in line for line in quick_logs)
    assert any('first_binding vt=' in line for line in quick_logs)
    assert any('built rows=3 error=40' in line for line in quick_logs)
    assert any('command cancelled' in line for line in quick_logs)
    reset_pocket();h.write(unit+0x10,0,4)
    for _ in range(80):y_input();y_input(0x10080,blocked=1)
    assert sum('QUICK_STORE_TRACE press=' in line for line in quick_logs)==64
    assert h.read(pocket+0x1f0)==0xffffffffffffffff and not transferred
    cases+=1
    # Regression: Nook's Cranny can leave selection bits behind in normal mode.
    # Match the user's mask, with focus both inside and outside its set bits.
    for selected_slot in (20,23,32):
        for room in (True,False):
            reset_pocket();h.write(pocket+0x1c4e8,0x1ff800000)
            y_input();y_input(0x10080);transition();update()
            assert transferred==([item_bytes] if room else [])
            assert confirmations==([selected_slot] if room else [])
            assert h.read(pocket+0x1c4e8)==0x1ff800000
            before=list(events);update();assert events==before
            transition();assert h.read(pocket+0x1b0,4)==(14 if room else 12)
            if room:
                update();transition();assert events[-1]=='play'
            cases+=1
    room=True
    # A leftover mask is harmless only in normal mode; reject actual special UIs.
    for mode in range(1,11):
        reset_pocket();h.write(pocket+0x1c4fc,mode,4)
        y_input();y_input(0x10080)
        assert h.read(pocket+0x1f0)==0xffffffffffffffff and not transferred
        assert not confirmations and h.read(pocket+0x1c4e8)==0x1ff800000
        h.write(pocket+0x1c4fc,0,4);cases+=1
    # Revalidate the mode after queuing, too: no transfer if another UI takes over.
    reset_pocket();y_input();y_input(0x10080);transition()
    h.write(pocket+0x1c4fc,8,4);update()
    assert not transferred and not confirmations
    assert h.read(pocket+0x1f4,4)==3
    h.write(pocket+0x1c4fc,0,4);h.write(pocket+0x1c4e8,0);cases+=1
    # Regression: +1c516 tracks equipped-item highlighting, NOT dragging.
    # Reproduce the user's equipped slot 5 while storing other focused slots.
    for selected_slot in (11,13,20,21,22):
        reset_pocket();h.write(pocket+0x1c516,5,1)
        y_input();y_input(0x10080);transition();update()
        assert transferred==[item_bytes] and confirmations==[selected_slot]
        assert h.read(pocket+0x1c516,1)==5 # Never clear the equipped-item state.
        update();assert transferred==[item_bytes] and confirmations==[selected_slot]
        cases+=1
    # Item feedback no longer depends on the unopened child menu's resources.
    reset_pocket();h.write(unit+0x10,1,4)
    y_input(row=1);y_input(0x10080,row=1);transition();update()
    assert transferred==[item_bytes] and confirmations==[selected_slot]
    assert h.read(menu_layout+0x40,4)==1
    h.write(unit+0x10,0,4);cases+=1
    for pointer,size in ((child+0x210,8),(menu_page+0x3f0,4),
                         (menu_page+0x3f8,8),(menu_layouts,8),(menu_layout+0x1b0,8)):
        reset_pocket();saved=h.read(pointer,size);h.write(pointer,0,size)
        y_input();y_input(0x10080);transition();update()
        assert transferred==[item_bytes] and confirmations==[selected_slot]
        h.write(pointer,saved,size);cases+=1
    # Native selector: disabled OR widget+315 set yields 1 (invalid), else 0.
    for enabled,valid in ((0,0),(0,1),(1,0),(1,1)):
        widget=widgets+selected_slot*0x230
        h.write(widget+0x2c3,enabled,1);h.write(widget+0x315,valid,1)
        h.put(0,widget);h.run(base+0x2fbfe5c)
        assert h.reg(0)==int(not enabled or valid);cases+=1
    # Failure must not publish success feedback parameters.
    reset_pocket();room=False
    y_input();y_input(0x10080);transition();update()
    assert not confirmations and h.read(menu_page+0x40,4)==1
    assert h.read(menu_layout+0x40,4)==99
    room=True;cases+=1
    # Missing or stale cursor resources never block the transfer or sound.
    for pointer,width,bad in ((ui_app+0xd0,8,0),(cursor,8,base+0x4a06e28),
                             (cursor+0x218,8,0),(cursor+0x26c,1,1),
                             (cursor_page+0x570,8,0),(cursor_layout+0x208,8,0),
                             (cursor_animation+8,8,0),(cursor_animation+0x18,8,0)):
        reset_pocket();y_input();y_input(0x10080);transition();update()
        saved=h.read(pointer,width);h.write(pointer,bad,width)
        transition();update();transition()
        assert transferred==[item_bytes] and confirmations==[selected_slot] and taps==[]
        h.write(pointer,saved,width);cases+=1
    # Run REAL feedback helper and Decide delegate, including override selection.
    del h.stubs[base+0x3c66950]
    h.stubs[safe_prepare]=lambda:None
    def copy_feedback_text():
        h.u.mem_write(h.reg(0),bytes(h.u.mem_read(h.reg(1),h.reg(2))))
        return h.reg(0)
    h.stubs[base+0x3f04470]=copy_feedback_text
    buffer_vtable=base+u64(raw,0x4ab2bc0)+16
    h.write(base+0x4ab2bc0,buffer_vtable-16);h.write(buffer_vtable+24,safe_prepare)
    delegate=menu_animation;override_event=h.ram+0x7e000;event_array=menu_layouts
    h.write(base+0x49fd220,base+u64(raw,0x49fd220))
    for override in (False,True):
        reset_pocket()
        for p,n in ((opening_event,0x60),(override_event,0x60),(event_queue,0x60)):
            h.u.mem_write(p,bytes(n))
        widget=widgets+selected_slot*0x230
        h.write(delegate,base+0x49fd220);h.write(delegate+8,widget)
        h.write(delegate+16,base+0x2fbeef0);h.write(delegate+24,0)
        h.write(widget+0x1e8,delegate)
        h.write(widget+0x360,2 if override else 0xffffffff,4)
        h.write(widget+0x350,3,4);h.write(widget+0x358,event_array)
        h.write(event_array+16,override_event)
        for event in (opening_event,override_event):h.write(event+0x38,event_queue)
        h.write(event_queue+0x40,event_queue+0x40);h.write(event_queue+0x48,event_queue+0x40)
        h.write(event_queue+0x54,0x10,4)
        y_input();y_input(0x10080);transition();update()
        assert h.read(event_queue+0x40)==(override_event if override else opening_event)+0x10
        assert h.read(event_queue+0x50,4)==1
        assert h.read(menu_page+0x40,4)==0
        assert transferred==[item_bytes] and events==['helper-close','store','remove-refresh']
        update();assert h.read(event_queue+0x50,4)==1 and transferred==[item_bytes]
        cases+=1
print(f'PASS {cases} compiled ARM cases at two ASLR bases, RX code: stacks, text, withdrawal, Y quick store and native dispatch/state lifecycle.')
print('Engine services and inline adapters mocked; Exlaunch installer and Eden integration still require testing.')
