"""Shared inherited navigation and cyclic quantity scenarios, with assertions intact.

Real native grid, focus, model replacement, repeat, animation and feedback paths
execute. GPU/scene/layout/resource boundaries retain the original fixture model.
"""
from recipe_preview_fixture import *

def navigation_fixture(base):
    m=PreviewMachine(base)
    phone,page,row,panes=m.phone(hotel=True)
    grid=m.read(page+0x580)
    count=35; columns=4; visible=12; pool=32
    # Nonmonotonic IDs stand in for the app's already sorted/filtered list.
    items=[0x1000000000000000 | ((i*17+0x88)&0xffff) for i in range(count)]
    # Load real category rows so navigation itself covers rotating -> fixed
    # -> rotating transitions without closing/reacquiring the renderer.
    resource=bytearray((ASSETS.parent/'Bcsv/ItemParam.bcsv').read_bytes())
    total,stride,fields,version=struct.unpack_from('<IIHB',resource)
    header=28 if version else 12
    offsets=dict(struct.unpack_from('<II',resource,header+8*i) for i in range(fields))
    fixed_kinds={zlib.crc32(n.encode()) for n in ('RoomWall','RoomFloor','Rug','Fence',
                                               '内装：壁紙','内装：床板','ラグ','柵')}
    native_rows={}; kind_items={}
    for i in range(total):
        offset=header+8*fields+i*stride; struct.pack_into('<I',resource,offset,offset)
        item_id=struct.unpack_from('<H',resource,offset+offsets[0x54706054])[0]
        kind=struct.unpack_from('<I',resource,offset+offsets[0x5270eb75])[0]
        native_rows[item_id]=(offset,kind); kind_items.setdefault(kind,item_id)
    fixed_ids={i for i,(_,kind) in native_rows.items() if kind in fixed_kinds}
    for index,names in enumerate((('RoomWall','内装：壁紙'),('RoomFloor','内装：床板'),
                                  ('Rug','ラグ'),('Fence','柵')),5):
        items[index]=next(kind_items[zlib.crc32(n.encode())] for n in names if zlib.crc32(n.encode()) in kind_items)
    data=base+0x30000000
    m.u.mem_map(data,(len(resource)+4095)&~4095); m.u.mem_write(data,bytes(resource))
    table=m.alloc(0x200); pointers=m.alloc((max(native_rows)+1)*8); slot=m.alloc()
    m.write(slot,table); m.write(base+0x4ac0428,slot)
    m.write(table+0x1d8,max(native_rows)+1,4); m.write(table+0x1e0,pointers)
    for item_id,(offset,_) in native_rows.items(): m.write(pointers+item_id*8,data+offset)
    recipes=m.alloc(count*8)
    for i,item in enumerate(items):
        recipe=m.alloc(0xd0); m.write(recipe+8,item); m.write(recipes+8*i,recipe)
    m.write(phone+0x8a350,count,4); m.write(phone+0x8a358,recipes)
    m.services[base+0x2e96b80]=('preview','details')
    # Execute the real native item-change body and verify the root request;
    # scene rebuilding itself is a boundary, like scene allocation on open.
    m.services[base+0x192d634]=('preview','change_item')
    m.services[base+0x267d4ac]=('preview','noop')
    m.services[base+0x192da60]=('preview','zero')
    default_item=m.alloc(); m.write(default_item,0xfffe); m.write(base+0x4ab37e0,default_item)
    # Native reset, viewport clamp, focus and input-list membership execute.
    # Only graphical row visibility and layout position writes are modeled.
    m.services[base+0x37654a0]=('preview','noop')
    grid_methods=m.alloc(0x150); m.write(grid,grid_methods)
    for off,target in ((0x100,m.stub('noop')),(0x110,base+0x3765b70)):
        m.write(grid_methods+off,target)
    layout=m.alloc(); layout_methods=m.alloc(); layout_ops=m.alloc(0xa0)
    m.write(grid+0x270,layout); m.write(layout,layout_methods)
    m.write(layout+8,m.alloc()); m.write(layout+0x18,layout_ops)
    m.write(layout_methods+0x18,m.stub('noop')); m.write(layout_ops+0x98,m.stub('noop'))
    context=m.alloc(0x100); states=m.alloc(8*8)
    m.write(context+0x18,8,4); m.write(context+0x20,states)
    m.write(context+0x14,0x1400,4)
    for i in range(8): m.write(states+8*i,m.alloc(0x130))
    m.write(grid+0x2d8,context)
    row_methods=m.alloc(0x148)
    m.write(row_methods+0x140,m.stub('noop')); m.write(row_methods+0xf0,m.stub('zero'))
    input_methods=m.alloc(); m.write(input_methods+0x18,m.stub('noop'))
    enter_methods=m.alloc(); m.write(enter_methods,m.stub('grid_enter'))
    leave_methods=m.alloc(); m.write(leave_methods,m.stub('grid_leave'))
    rows=m.alloc(pool*8)
    for i in range(pool):
        button=m.alloc(0x300); key=m.alloc(0x1500)
        m.write(button,row_methods); m.write(button+0x218,key)
        m.write(key,input_methods); m.write(key+0x18,context)
        m.write(key+0x20,enter_methods); m.write(key+0x40,leave_methods)
        m.write(key+0x14b0,0xff,4)
        m.write(rows+8*i,button)
    m.write(grid+0x208,pool,4); m.write(grid+0x210,rows)
    for off,value in ((0x298,2),(0x2e8,columns),(0x2ec,3),(0x2f0,visible),
                      (0x2f4,visible),(0x2f8,pool),(0x3cc,count),(0x3d4,count),(0x3d8,2)):
        m.write(grid+off,value,4)
    delegate=m.alloc(); m.write(delegate+0x10,m.stub('zero'))
    m.write(grid+0x330,delegate)
    key=m.alloc(); m.write(key+0x14,0xffffffff,4); m.write(grid+0x4c0,key)
    m.button(page,m.read(rows),0)
    m.confirm(page); m.frame(phone); m.frame(phone)
    assert m.read(phone+0x218,4)==0
    original_handle=m.read(m.controller+0x6c,4)
    pose_epoch=m.read(m.module.find('PreviewPoseEpoch'),4)
    pose_origin=bytes(m.u.mem_read(m.controller+0x80,12))
    last_selection=0
    def step(held=0,left=(0,0),**kwargs):
        m.input(page,held,left=left,**kwargs); m.frame(phone)
    def press(direction,stick=False):
        step(); step(0 if stick else (8 if direction>0 else 4),left=(direction,0) if stick else (0,0))
    def selected(expected):
        nonlocal pose_epoch,last_selection
        assert m.read(phone+0x218,4)==expected, (expected,m.read(phone+0x218,4),m.read(grid+0x298,4),m.read(m.controller+0x74,1),m.events[-8:],m.text[-1000:])
        row=m.invoke(base+0x3765104,grid,expected)
        assert row and row==m.read(rows+8*((expected//columns-m.read(grid+0x430,4)+m.read(grid+0x438,4))*columns+expected%columns))
        focused=m.read(m.read(states)+0x10)
        assert focused==m.read(row+0x218), (hex(focused),hex(row))
        assert m.read(grid+0x440,4)*columns+m.read(grid+0x444,4)==expected
        assert m.read(m.controller+0x6c,4)==original_handle
        assert m.read(m.parameters+0x2c)==items[expected]
        pose_epoch+=int(expected!=last_selection); last_selection=expected
        assert m.read(m.module.find('PreviewPoseEpoch'),4)==pose_epoch
        assert m.read(m.module.find('PreviewItem'))==items[expected]
        assert m.read(m.module.find('PreviewOriginXY'))==int.from_bytes(pose_origin[:8],'little')
        assert m.read(m.module.find('PreviewOriginZ'),4)==int.from_bytes(pose_origin[8:],'little')
        assert m.read(panes['N_TradeBox_00']+0x18)==panes['N_QolPreview_00']
        assert m.string(panes['T_TradeBoxNum_00']+0x100)=='12'
        change=[e for e in m.events if e[0]=='change_item'][-1][1]
        assert struct.unpack_from('<Q',change)[0]==items[expected]
        assert struct.unpack_from('<H',change,0x24)[0]==0xfffe  # Actor recipe field.
        assert [e for e in m.events if e[0]=='details'][-1][1]==expected
        fixed=(items[expected]&0xffff) in fixed_ids
        assert m.read(m.module.find('PreviewRequestedFixedPose'),1)==int(fixed)
        assert all(m.read(panes[n]+0x58,1)&1==0 for n in ('L_BtnList_00','L_Cursor_00','L_ScrollBar_00'))
    # A real displayed carrier keeps its converted/rotated pose between the
    # UI request and asynchronous scene replacement, including fixed targets.
    root=m.model_entity
    component=m.alloc(0x300); carrier=m.alloc(0x400)
    m.u.mem_write(component,bytes(m.u.mem_read(m.component,0x2c8)))
    m.write(component+0x48,carrier); m.write(carrier+0x230,0x112345,4)
    m.write(carrier+0x234,0x12345,4); m.write(carrier+0x15c,6,4)
    m.u.mem_write(component+0x2a0,struct.pack('<3f',1,1,1))
    m.u.mem_write(component+0x2b0,struct.pack('<3I',0x10000000,0x80000000,0))
    m.u.mem_write(component+0x2bc,bytes(12))
    scene_owned=m.module.find('PreviewScenePose'); m.write(scene_owned,1,1)
    m.write(m.module.find('PreviewCarriers'),0x112345,4)
    displayed=m.drawable_matrix(component,protected=True)
    for i in range(1,count):
        press(1,stick=bool(i%2)); selected(i)
        assert m.read(scene_owned,1)==1
        assert m.read(m.module.find('PreviewCarriers'),4)==0x112345
        assert m.drawable_matrix(component,protected=True)==displayed
        if 5<=i<=8:
            pose=bytes(m.u.mem_read(m.controller+0x80,12))
            m.input(page,0,right=(1,1)); m.frame(phone)
            assert bytes(m.u.mem_read(m.controller+0x80,12))==pose
    press(1); selected(count-1)
    for i in range(count-2,-1,-1): press(-1,stick=bool(i%2)); selected(i)
    press(-1); selected(0)
    # One input dispatch queues one UI update; duplicate consumed snapshots
    # cannot double-step. Held repeat has delay and cadence, release/reverse immediate.
    step(); m.input(page,8); snapshot=m.last_input_row
    snap=m.alloc(); m.write(snap+8,8,4); m.write(snap+0x10,snapshot)
    m.hook(0x3c58d24,page,snap,0); m.frame(phone); selected(1)
    for _ in range(12): step(8); selected(1)
    step(8); selected(2)
    for _ in range(4): step(8); selected(2)
    step(8); selected(3)
    step(4); step(4); selected(2)
    press(1); selected(3)
    for held,left,kwargs in ((12,(0,0),{}),(0,(0,1),{}),(0,(0.4,0),{}),(0,(1,1),{}),
                             (0,(float('nan'),0),{}),(8,(0,0),{'blocked':1}),
                             (8,(0,0),{'consumed':8}),(0,(1,0),{'consumed_axes':1})):
        step(); step(held,left,**kwargs); selected(3)
    # Native UI input also synthesizes kinds 0/1 for vertical stick movement.
    # Test the actual held bits alongside the axes, not only zero-button mocks.
    for held,left in ((1,(0,1)),(2,(0,-1)),(3,(0,0))):
        step(); step(held,left); selected(3)
    # Inconsistent/busy lists never index an invalid row or change the model.
    next_recipe=m.read(recipes+4*8)
    for location,value,size in ((phone+0x1f0,0,4),(grid+0x3cc,count-1,4),
                                (grid+0x298,1,4),(grid+0x2e8,0,4),
                                (grid+0x2f4,0,4),(recipes+4*8,0,8),
                                (next_recipe+8,0xfffe,2)):
        old=m.read(location,size); m.write(location,value,size)
        press(1); assert m.read(phone+0x218,4)==3
        m.write(location,old,size)
        step(blocked=1); step(); selected(3)
    # Missing snapshots clear queued navigation. B wins over same-frame next.
    step(); m.input(page,8); m.hook(0x3c58d24,page,0,0); m.frame(phone); selected(3)
    press(1); selected(4)
    step(); m.input(page,0x28); m.frame(phone)
    assert m.read(phone+0x218,4)==4
    assert m.read(panes['L_BtnList_00']+0x58,1)&1
    row=m.invoke(base+0x3765104,grid,4)
    assert m.read(m.read(states)+0x10)==m.read(row+0x218)
    step(); m.confirm(page); m.frame(phone); m.frame(phone)
    assert m.read(m.parameters+0x2c)==items[4]
    assert len([e for e in m.events if e[0]=='spawn'])==2
    m.hook(0x2e9d538,phone)
    print(f'PASS: linear d-pad/left-stick navigation across rows and pooled viewports; native index/focus/material/model updates, held repeat, input gates, invalid/busy lists, fixed poses, endpoints and B cursor/reopen; ASLR {base:#x}')


def native_preview_presets():
    # Read the supplied native BYML rather than inventing category angles.
    archive=resources((ASSETS.parent/'Pack/StaticParam.pack').read_bytes())
    data=archive['Param/Actor/ItemPreviewSwapActor.byml']
    def u24(p): return int.from_bytes(data[p:p+3],'little')
    def u32(p): return struct.unpack_from('<I',data,p)[0]
    def strings(p):
        return [data[p+u32(p+4+4*i):].split(b'\0')[0].decode() for i in range(u24(p+1))] if p else []
    keys=strings(u32(4)); texts=strings(u32(8))
    def value(kind,word):
        if kind in (0xc0,0xc1): return node(word)
        if kind==0xd2: return struct.unpack('<f',struct.pack('<I',word))[0]
        if kind==0xa0: return texts[word]
        if kind==0xff: return None
        assert kind in (0xd0,0xd1,0xd3)
        return word
    def node(p):
        count=u24(p+1)
        if data[p]==0xc0:
            words=(p+4+count+3)&~3
            return [value(data[p+4+i],u32(words+4*i)) for i in range(count)]
        assert data[p]==0xc1
        return {keys[u24(p+4+8*i)]:value(data[p+7+8*i],u32(p+8+8*i)) for i in range(count)}
    presets=set()
    def collect(entry):
        if isinstance(entry,dict):
            if '4a42cf70' in entry: presets.add(tuple(entry['4a42cf70']))
            for child in entry.values(): collect(child)
        elif isinstance(entry,list):
            for child in entry: collect(child)
    collect(node(u32(12)))
    assert (15.0,-130.0,0.0) in presets and len(presets)>40
    return presets


def quantity_wrap_fixture(base,pool=5):
    margin=(pool-1)//2
    m=PreviewMachine(base)
    page=m.alloc(0x600); grid=m.alloc(0x600); context=m.alloc(0x100)
    m.write(page,base+0x4a3a7d8); m.write(page+0x578,grid)
    m.write(page+0x210,m.alloc())
    assert struct.unpack_from('<Q',RAW,0x4aca2e8)[0]+16==0x4a3a7d8
    assert struct.unpack_from('<Q',RAW,0x4ac7288)[0]+16==0x49de8f0
    for vtable in (0x4a3a7d8,0x49de8f0):
        assert struct.unpack_from('<Q',RAW,vtable+0x50)[0]==0x3c58d24
    methods=m.alloc(0x150); m.write(grid,methods)
    for off,target in ((0x100,m.stub('noop')),(0x108,m.stub('noop')),
                       (0x110,base+0x3765b70),(0x118,m.stub('noop')),(0x120,m.stub('noop'))):
        m.write(methods+off,target)
    layout=m.alloc(); layout_methods=m.alloc(); layout_ops=m.alloc(0xa0)
    m.write(grid+0x270,layout); m.write(layout,layout_methods)
    m.write(layout+8,m.alloc()); m.write(layout+0x18,layout_ops)
    m.write(layout_methods+0x18,m.stub('noop')); m.write(layout_ops+0x98,m.stub('quantity_position'))
    states=m.alloc(8*8); m.write(context+0x18,8,4); m.write(context+0x20,states)
    m.write(context+0x14,0x1400,4)
    for i in range(8): m.write(states+8*i,m.alloc(0x130))
    m.write(grid+0x2d8,context)
    row_methods=m.alloc(0x148); m.write(row_methods+0x140,m.stub('noop')); m.write(row_methods+0xf0,m.stub('zero'))
    input_methods=m.alloc(); m.write(input_methods+0x18,m.stub('noop'))
    enter_methods=m.alloc(); m.write(enter_methods,m.stub('grid_enter'))
    leave_methods=m.alloc(); m.write(leave_methods,m.stub('grid_leave'))
    rows=m.alloc(pool*8)
    for i in range(pool):
        button=m.alloc(0x300); key=m.alloc(0x1500)
        m.write(button,row_methods); m.write(button+0x218,key)
        m.write(button+0x2f0,m.alloc(8))
        m.write(key,input_methods); m.write(key+0x18,context)
        m.write(key+0x20,enter_methods); m.write(key+0x40,leave_methods)
        m.write(key+0x14b0,0xff,4); m.write(rows+8*i,button)
    m.write(grid+0x208,pool,4); m.write(grid+0x210,rows)
    for off,value in ((0x298,2),(0x2e0,3),(0x2e8,1),(0x2ec,1),(0x2f0,1),
                      (0x2f4,1),(0x2f8,pool),(0x3cc,10),(0x3d4,10),(0x3d0,10),
                      (0x3d8,margin),(0x3e8,7),(0x418,2),(0x434,9),(0x440,9),(0x430,9)):
        m.write(grid+off,value,4)
    delegate=m.alloc(); m.write(delegate,base+0x2d00198); m.write(delegate+0x10,m.stub('accept')); m.write(grid+0x330,delegate)
    key=m.alloc(); m.write(key+0x14,0xffffffff,4); m.write(grid+0x4c0,key)
    # Execute the real quantity selection callback via the native grid's +370
    # delegate. Formatting/material widgets are boundaries, not reimplemented.
    sequence=m.alloc(0x1c300); seq_methods=m.alloc(0x230)
    m.write(sequence,seq_methods); m.write(sequence+0x230,page)
    m.write(seq_methods+0x228,m.stub('zero')); m.write(sequence+0x304,0x3ff,2)
    selection_methods=m.alloc(); m.write(selection_methods,base+0x2d00198)
    m.write(selection_methods+0x10,m.stub('accept'))
    m.write(grid+0x370,selection_methods); m.write(grid+0x378,sequence)
    m.write(grid+0x380,base+0x3469e1c); m.write(grid+0x388,0)
    m.services[base+0x3464e40]=('preview','noop')
    m.services[base+0x3465370]=('preview','details')
    # Execute the real quantity label binder, including its circular index
    # calculation, type check and 1..native maximum bounds. Text writing is
    # the boundary. Keep its native owner/delegate ABI, like selection above.
    m.quantity_numbers={}
    m.write(grid+0x338,sequence); m.write(grid+0x340,base+0x3469b50)
    type_guard=m.alloc(); m.write(type_guard,1,1)
    m.write(base+0x4ac4d98,type_guard); m.write(base+0x4ac4d90,m.alloc())
    m.write(row_methods,m.stub('accept'))
    m.services[base+0x34649bc]=('preview','quantity_number')
    m.services[base+0x37654a0]=('preview','noop')
    # Execute native scroll animation/feedback. Only timeline/audio playback,
    # row rendering and text writes are modeled; native interpolation and binding
    # indices execute. Native grid setup adds four padding rows to the viewport;
    # the two L_Btn panes in the layout are templates, not the full runtime pool.
    m.write(grid+0x2b8,1,4); m.write(grid+0x2c0,m.alloc())
    m.write(grid+0x3b0,0,4); m.write(grid+0x3c4,4,4)
    m.u.mem_write(grid+0x308,struct.pack('<f',100.0))
    scroll_binding=m.alloc(); sound_selector=m.alloc()
    m.write(grid+0x450,scroll_binding); m.write(grid+0x468,sound_selector)
    m.services[base+0x3c66950]=('preview','quantity_scroll')
    m.write(base+0x4a5f620,base+struct.unpack_from('<Q',RAW,0x4a5f620)[0])
    # Run native logical-button dispatch and its repeat timer, rather than
    # assuming the axis-consumed bit also suppresses synthesized directions.
    clock=m.alloc(); clock_methods=m.alloc()
    m.write(base+0x59f83f8,clock); m.write(clock,clock_methods)
    m.write(clock_methods+0x48,m.stub('accept'))
    timing=m.alloc(); m.write(timing,12,4); m.write(timing+4,4,4)
    delegate_methods=m.alloc(); m.write(delegate_methods,base+0x3766448)
    noop_methods=m.alloc(); m.write(noop_methods,m.stub('noop'))
    native_keys=[]
    for bit,offset in ((0,0x480),(1,0x490)):
        key=m.alloc(0xc8); native_keys.append(key)
        m.write(grid+offset,key); m.write(key+0x10,3,4); m.write(key+0x14,bit,4)
        m.write(key+0xb8,0xffffffff,4); m.write(key+0xbc,0x101,2)
        for kind in range(5):
            slot=key+0x18+kind*0x20
            m.write(slot,delegate_methods if kind in (1,3) else noop_methods)
            m.write(slot+8,grid)
            m.write(slot+0x10,base+(0x3762978 if kind==3 else 0x3762a98))
    m.write(grid+0x30c,1,4) # Vertical list; up/down callbacks queue row steps.
    def selected(): return m.read(grid+0x440,4)
    def step(direction=0,stick=False,repeat=False,**kwargs):
        m.quantity_tick += kwargs.pop('ticks',0)
        held=kwargs.pop('held',1 if direction>0 else 2 if direction<0 else 0)
        left=kwargs.pop('left',(0,direction) if stick else (0,0))
        m.write(grid+0x3ec,0,4)
        m.input(page,held,resident=3,left=left,**kwargs)
        if repeat or direction==0:
            for key in native_keys:
                m.invoke(base+0x3c4b20c,key,m.last_input_snapshot,timing,kwargs.get('blocked',0))
            if m.read(grid+0x3ec,4): m.invoke(base+0x375fd8c,grid)
        m.invoke(base+0x3763480,grid)
    def at(index):
        callback=0x2de79fc if m.read(page)==base+0x49de8f0 else 0x3469e1c
        m.write(grid+0x380,base+callback)
        m.write(grid+0x440,index,4)
        m.write(grid+0x430,index,4); m.write(grid+0x438,margin,4)
        m.write(grid+0x434,m.read(grid+0x3cc,4)-1,4)
        m.write(grid+0x418,2,4)
        m.u.mem_write(grid+0x424,bytes(8))
        for offset in (0x400,0x40c): m.u.mem_write(grid+offset,struct.pack('<3f',0,margin*100,0))
    # The real layout supplies a scissored one-number viewport and two button
    # templates. Match that viewport instead of the earlier generic 3-row fixture.
    quantity_layout=layout_sections(layout_asset('LBtnListNumInput'))
    assert {pane_name(p) for p in quantity_layout if p[:4]==b'prt1'} >= {'L_Btn_00','L_Btn_01'}
    assert any(p[:4]==b'usd1' and b'ScissorOn' in p for p in quantity_layout)
    # Enter through the actual cAppear return address so native cyclic binding
    # and the quantity-specific padded-row delegate are prepared before input.
    request=m.alloc(0x30); m.write(request,base+0x4a2b020)
    m.u.mem_write(request+8,struct.pack('<3I',10,9,9)); m.write(request+0x16,1,1)
    m.write(request+0x18,1,1); m.write(request+0x2c,3,4)
    reset_call=m.stub('quantity_reset_call')
    m.quantity_reset_caller=base+0x346b028
    m.services[m.quantity_reset_caller]=('preview','quantity_reset_return')
    m.invoke(reset_call,grid,request); m.invoke(base+0x335b4a0,grid,0)
    m.write(grid+0x418,2,4)
    # A held control on open is disarmed, then both d-pad and stick can wrap.
    step(-1); assert selected()==9
    for page_class in (0x4a3a7d8,0x49de8f0):
        m.write(page,base+page_class)
        for stick in (False,True):
            at(9); step(); step(-1,stick); assert selected()==0
            assert m.read(sequence+0x1c2d8,4)==9  # Native quantity index = quantity-1.
            assert m.read(grid+0x3e8,1)&2 # Native cyclic neighbor binding is prepared on open.
            feedback=[e for e in m.events if e[0]=='quantity_scroll'][-1]
            assert feedback[1:]==('Scroll',scroll_binding,0,1,0x3f800000),feedback

            for _ in range(40): step(-1,stick,repeat=True)
            assert selected()==0 # A wrapped stick cannot immediately scroll to 9.
            assert m.read(grid+0x3e8,1)&2 # Input gating, not removing neighbors, stops repeat wrapping.
            at(0); step(); step(1,stick); assert selected()==9
            assert m.read(sequence+0x1c2d8,4)==0
            feedback=[e for e in m.events if e[0]=='quantity_scroll'][-1]
            assert feedback[1:]==('Scroll',scroll_binding,1,0,0x3f800000),feedback
            # Hold from the interior all the way to an endpoint and beyond.
            at(5); step()
            for _ in range(80): step(1,stick,repeat=True)
            assert selected()==0
            step(-1,stick); step(1,stick); assert selected()==0  # Reversal is not release.
            step(); step(1,stick); assert selected()==9
            for _ in range(80): step(-1,stick,repeat=True)
            assert selected()==9
            step(); step(-1,stick); assert selected()==0
    # Animate 1 <-> native maximum in a single row, including material-limited
    # selectors, then verify interpolation settles and ordinary clamping returns.
    for page_class in (0x4a3a7d8,0x49de8f0):
        m.write(page,base+page_class)
        for count in range(2,11):
            for off in (0x3cc,0x3d4,0x3d0): m.write(grid+off,count,4)
            for direction in (-1,1):
                at(count-1 if direction<0 else 0); step()
                m.events.clear(); step(direction,True)
                feedback=[e for e in m.events if e[0]=='quantity_scroll']
                assert len(feedback)==1,feedback
                assert feedback[0][1:]==('Scroll',scroll_binding,int(direction>0),
                                          int(direction<0),0x3f800000),feedback
                assert m.read(grid+0x3ec,4)==0 # No second queued step.
                positions=[struct.unpack('<3f',e[1]) for e in m.events if e[0]=='quantity_position']
                assert any(0 < abs(p[1]-margin*100) < 100 for p in positions),positions
                center=m.read(rows+margin*8)
                neighbor=m.read(rows+(margin-1 if direction<0 else margin+1)*8)
                assert m.quantity_numbers[center]==(count if direction<0 else 1)
                assert m.quantity_numbers[neighbor]==(1 if direction<0 else count), (pool,count,direction)
                assert m.read(grid+0x210)==rows and m.read(grid+0x2f8,4)==pool
                assert any(e[0]=='quantity_number' for e in m.events)
                for _ in range(80): step()
                assert m.read(grid+0x418,4)==2 and m.read(grid+0x3e8,1)&2
                assert m.read(grid+0x430,4)==selected()==(0 if direction<0 else count-1)
                # Quantity detail callback ran once; wrapping did not reopen/reset
                # the selector, which previously caused the standard click.
                assert len([e for e in m.events if e[0]=='details'])==1
    for off in (0x3cc,0x3d4,0x3d0): m.write(grid+off,10,4)
    # Pressure jitter below the press threshold is not a physical release.
    # Logical stick buttons can arrive after the raw axis, and both routes
    # must stay consumed after a wrap. Exercise the native repeat path too.
    at(9); step(); step(-1,True,repeat=True,held=0); assert selected()==0
    for y,held in ((-1,2),(-0.49,0),(-0.2,0),(-0.51,2),(-1,2))*12:
        step(-1,True,repeat=True,held=held,left=(0,y)); assert selected()==0
    step(-1,True,repeat=True,held=0x32)
    assert selected()==0 and m.read(m.last_input_row+0x3c,4)&0x30==0 # A/B pass through.
    step(repeat=True); step(-1,True,repeat=True); assert selected()==1
    at(0); step(); step(1,True,repeat=True); assert selected()==9
    for _ in range(40): step(1,True,repeat=True)
    assert selected()==9
    # Easing the stick mid-scroll must not rearm the eventual endpoint.
    at(5); step(repeat=True)
    for _ in range(80): step(1,True,repeat=True)
    assert selected()==0
    step(1,True,repeat=True,held=0,left=(0,0.4))
    step(1,True,repeat=True); assert selected()==0
    step(1,True,repeat=True,held=0,left=(1,0))
    step(1,True,repeat=True); assert selected()==0
    # A deliberate slow movement from neutral still counts as a fresh press.
    at(9); step(repeat=True)
    for y in (-0.1,-0.2,-0.4,-0.49):
        step(-1,True,repeat=True,held=0,left=(0,y)); assert selected()==9
    step(-1,True,repeat=True); assert selected()==0
    # Only a fresh endpoint wrap gets the 300 ms pause. Execute real
    # native button timers at its exact boundary, then repeat to the far end.
    pause_ticks=5_760_000
    for page_class in (0x4a3a7d8,0x49de8f0):
        m.write(page,base+page_class)
        for stick in (False,True):
            for direction in (-1,1):
                for rate in (30,60,120):
                    at(9 if direction<0 else 0); step(repeat=True)
                    step(direction,stick,repeat=True)
                    destination=0 if direction<0 else 9
                    assert selected()==destination
                    assert m.read(m.last_input_row,4)&3==0
                    assert bytes(m.u.mem_read(m.last_input_row+4,8))==bytes(8)
                    frame_ticks=19_200_000//rate
                    for _ in range(pause_ticks//frame_ticks-1):
                        step(direction,stick,repeat=True,ticks=frame_ticks)
                        assert selected()==destination
                    step(direction,stick,repeat=True,ticks=frame_ticks-1)
                    assert selected()==destination
                    step(direction,stick,repeat=True,ticks=1)
                    assert selected()==destination-direction # Native resumes immediately at 300 ms.
                    for _ in range(100):
                        step(direction,stick,repeat=True,ticks=19_200_000//rate)
                    endpoint=9 if direction<0 else 0
                    assert selected()==endpoint
                    step(direction,stick,repeat=True,ticks=19_200_000)
                    assert selected()==endpoint # Reaching the boundary while holding cannot wrap.
                    step(repeat=True); step(direction,stick,repeat=True)
                    assert selected()==destination # Release + fresh press wraps again.
    # Releasing cancels a pause; another deliberate wrap gets its own timer.
    at(9); step(repeat=True); step(-1,True,repeat=True); assert selected()==0
    step(repeat=True,ticks=pause_ticks*2//5) # Release before expiry.
    step(1,True,repeat=True); assert selected()==9
    step(1,True,repeat=True,ticks=pause_ticks*3//5); assert selected()==9 # First wrap's deadline.
    step(1,True,repeat=True,ticks=pause_ticks*2//5-1); assert selected()==9
    step(1,True,repeat=True,ticks=1); assert selected()==8
    # In-range scrolling has no extra delay, even after the previous pause.
    at(5); step(repeat=True); step(-1,True,repeat=True); assert selected()==6
    # No resume may bypass another consumer's button/axis ownership.
    at(9); step(repeat=True); step(-1,True,repeat=True)
    step(-1,True,repeat=True,ticks=pause_ticks,consumed=2,consumed_axes=1)
    assert selected()==0
    # A blocked/consumed press cannot be queued until a later unblocked update.
    for kwargs in ({'blocked':1},{'consumed':2},{'consumed_axes':1}):
        at(9); step(); step(-1,True if 'consumed_axes' in kwargs else False,**kwargs)
        assert selected()==9
        step(-1); assert selected()==9
        step(); step(-1); assert selected()==0
    at(9); step(); m.write(m.read(page+0x210)+0x54,1,1); step(-1); assert selected()==9
    m.write(m.read(page+0x210)+0x54,0,1); step(-1); assert selected()==9
    # Native reset/reopen disarms cached controls, even with reused pointers.
    step(); request=m.alloc(0x30); m.write(request,base+0x4a2b020)
    m.u.mem_write(request+8,struct.pack('<3I',10,9,9)); m.write(request+0x16,1,1)
    m.write(request+0x18,1,1); m.write(request+0x2c,3,4)
    reset_call=m.stub('quantity_reset_call')
    for caller in (0x346b028,0x2de8bf8):
        m.quantity_reset_caller=base+caller
        m.services[base+caller]=('preview','quantity_reset_return')
        m.invoke(reset_call,grid,request)
        assert m.read(request+0x17,1)==0 # Do not mutate the caller's request.
        assert m.read(grid+0x3e8,1)&2 and m.read(grid+0x438,4)==margin
        m.invoke(base+0x335b4a0,grid,0)
        assert m.read(grid+0x298,4)==2
        # Native activation's state entry settles the motion; modeled state
        # callbacks do not perform that part of the list lifecycle.
        m.write(grid+0x418,2,4)
        step(-1); assert selected()==9
        step(); step(-1); assert selected()==0
    # A reset from any other caller must retain the unmodified non-cyclic list.
    m.hook(0x3764290,grid,request); assert m.read(grid+0x3e8,1)&2==0
    m.invoke(base+0x335b4a0,grid,0)
    # Restore the scoped quantity setup before continuing input regressions.
    m.invoke(reset_call,grid,request); m.invoke(base+0x335b4a0,grid,0)
    # Honor the native range when materials limit the selector below ten.
    m.write(grid+0x3cc,4,4); m.write(grid+0x3d4,4,4); m.write(grid+0x3d0,4,4)
    at(3); step(); step(-1); assert selected()==0
    step(); step(1); assert selected()==3
    # Ignore unrelated pages, malformed snapshots, horizontal input and A/B.
    at(3); step(); old=m.read(page); m.write(page,base+0x4a3b3e0)
    step(-1); assert selected()==3; m.write(page,old)
    for held,left in ((0,(1,0)),(0x32,(0,0)),(3,(0,0)),(0,(0,float('nan')))):
        step(); m.input(page,held,resident=3,left=left); assert selected()==3
    assert any(e==('native_input',page) for e in m.events)
    print(f'PASS: quantity endpoint wrap, 300 ms pause/resume boundary at 30/60/120 Hz, release cancels/restarts pause, no delay in range or repeat wrap at reached boundary, native button repeat and quantity/material callback, native one-row wrap slide and directional cScroll feedback, delayed stick buttons, pressure jitter and neutral rearm, d-pad/stick, crafting/cooking pages, native limits, blocking and reused-page reset; system clock/raw-to-logical input/layout services modeled; pool={pool}; ASLR {base:#x}')
