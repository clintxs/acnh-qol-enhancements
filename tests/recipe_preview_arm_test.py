"""Run the inherited compiled preview assertions using the shared native fixture.

GPU/scene/layout boundaries retain recipe_preview_fixture's modeling limits.
"""
from recipe_preview_fixture import *
from crafting_scenario_fixture import navigation_fixture, quantity_wrap_fixture, native_preview_presets

def pane_tree(content):
    panes, parents, current = [], [], None
    for section in layout_sections(content):
        if section[:4] in PANE_TYPES:
            current = pane_name(section)
            panes.append((current, parents[-1] if parents else None))
        elif section[:4] == b'pas1': parents.append(current)
        elif section[:4] == b'pae1': parents.pop()
    return panes

def verify_layout(original, patched):
    before = layout_sections(original); after = layout_sections(patched)
    oldpanes = {pane_name(s): s for s in before if s[:4] in PANE_TYPES}
    newpanes = {pane_name(s): s for s in after if s[:4] in PANE_TYPES}
    assert all(newpanes[n] == s for n, s in oldpanes.items())
    assert not newpanes['N_QolPreview_00'][8] & 1
    # DynamicCapture copies the framebuffer when encountered in pane order.
    # Capture before the app paints over the scene; display the texture later.
    tree = pane_tree(patched)
    assert tree[1] == ('N_QolCapture_00', 'RootPane'), tree[:4]
    assert tree[-1] == ('P_QolCapture_00', 'N_QolPreview_00')
    assert ('N_QolPreview_00', 'RootPane') in tree
    assert not any(parent == 'N_QolCapture_00' for _, parent in tree)
    assert not newpanes['N_QolCapture_00'][8] & 1
    assert struct.unpack_from('<ff', newpanes['N_QolCapture_00'], 44) == (-430, -64)
    old_materials=next(s for s in before if s[:4]==b'mat1')
    new_materials=next(s for s in after if s[:4]==b'mat1')
    assert material_records(new_materials)[:-1]==material_records(old_materials)
    old_textures=next(s for s in before if s[:4]==b'txl1')
    assert next(s for s in after if s[:4]==b'txl1')==old_textures
    texture_names=[old_textures[12+struct.unpack_from('<I',old_textures,12+4*i)[0]:].split(b'\0',1)[0]
                   for i in range(struct.unpack_from('<H',old_textures,8)[0])]
    new_material=material_records(new_materials)[-1]
    assert texture_names[struct.unpack_from('<H',new_material,44)[0]]==b'White_00^r'
    assert all(s in after for s in before if s[:4]!=b'mat1')
    # Check the author-defined user data with the format's entry-relative ABI.
    usd = next(after[i+1] for i,s in enumerate(after) if s[:4] == b'pan1' and pane_name(s) == 'N_QolCapture_00')
    name_offset, value_offset = struct.unpack_from('<II', usd, 12)
    assert usd[12+name_offset:].split(b'\0',1)[0] == b'DynamicCaptureOn'
    assert usd[12+value_offset:].split(b'\0',1)[0] == b'R10G10B10A2'
    # The live scene has no usable model alpha. Preserve the native workbench's
    # opaque copy: alpha zero made the whole item disappear in 0.3.38.
    assert struct.unpack_from('<i', usd, 36)[0] == 255
    capture = material_records(next(s for s in after if s[:4] == b'mat1'))[-1]
    assert struct.unpack_from('<I', capture, 28)[0] == 0x15
    assert struct.unpack_from('<I', capture, 32)[0] == 0x08040200
    assert len(capture) == 84

# This ordering comes from the native screen, whose capture has no children:
# its model image must already be present in the framebuffer at capture time.
native_tree = pane_tree(layout_asset('WorkBenchInfo'))
assert native_tree[1] == ('N_Capture_00', 'RootPane')
assert not any(parent == 'N_Capture_00' for _, parent in native_tree)
assert next(i for i, (name, _) in enumerate(native_tree) if name == 'P_Capture_00') > 2

for base in (0x7100000000, 0x7200000000):
    m = PreviewMachine(base)
    for target,action in ((0x2478c0,'allocate_component'),(0x605c50,'construct_component'),
                          (0x37e18e4,'noop'),(0x5f9fc0,'attach_component'),(0x6402e0,'entity_create'),
                          (0x665f94,'noop')):
        m.services[base+target]=('preview',action)
    allocator_slot=m.alloc(); m.write(base+0x4ab2698,allocator_slot)
    for got in (0x4ab3078,0x4ab3300,0x4ab2d88):
        m.write(base+got,base+struct.unpack_from('<Q',RAW,got)[0])
    default_item=m.alloc(); m.write(default_item,0xfffe); m.write(base+0x4ab37e0,default_item)
    params=m.alloc(0x30)
    actor=m.alloc(0x450); m.write(actor+0x230,0x12345,4); m.write(actor+0x394,1,4)
    m.invoke(base+0x192a1f0,actor,params,0)
    request=next(e for e in m.events if e[0]=='entity_create')
    assert request[1]==0x12345 and struct.unpack_from('<I',request[2],4)[0]==6
    carrier=m.alloc(0x450); m.write(carrier+0x230,0x112345,4)
    m.invoke(base+0x19362f4,carrier,params,0)
    assert m.read(carrier+0x27c,4)==0x6f and m.read(carrier+0x360,4)==0x6f
    assert m.read(carrier+0x364,4)==0x112345
    print(f'PASS: native type-42 initializer requests type 6; native type-6 initializer requests type 6F, ASLR {base:#x}')

# Classify actual 3.0.3 ItemParam rows, then preserve the native pose through
# both controller animation and root/carrier/grandchild matrix boundaries.
item_data=bytearray((ASSETS.parent/'Bcsv/ItemParam.bcsv').read_bytes())
item_count,item_stride,item_fields,item_version=struct.unpack_from('<IIHB',item_data)
item_header=28 if item_version else 12
item_offsets=dict(struct.unpack_from('<II',item_data,item_header+8*i) for i in range(item_fields))
fixed_names={'RoomWall':'内装：壁紙','RoomFloor':'内装：床板','Rug':'ラグ','Fence':'柵'}
fixed_hashes={zlib.crc32(name.encode()) for pair in fixed_names.items() for name in pair}
item_rows={}; kinds={}
for i in range(item_count):
    offset=item_header+8*item_fields+i*item_stride
    struct.pack_into('<I',item_data,offset,offset) # Runtime header back-reference.
    item_id=struct.unpack_from('<H',item_data,offset+item_offsets[0x54706054])[0]
    kind=struct.unpack_from('<I',item_data,offset+item_offsets[0x5270eb75])[0]
    item_rows[item_id]=(offset,kind); kinds.setdefault(kind,item_id)
fixed_items={item_id for item_id,(offset,kind) in item_rows.items() if kind in fixed_hashes}
assert all(any(kind in {zlib.crc32(n.encode()),zlib.crc32(j.encode())} for _,kind in item_rows.values())
           for n,j in fixed_names.items())
for base in (0x7100000000, 0x7200000000):
    m=PreviewMachine(base)
    data=base+0x30000000
    m.u.mem_map(data,(len(item_data)+4095)&~4095); m.u.mem_write(data,bytes(item_data))
    table=m.alloc(0x200); pointers=m.alloc((max(item_rows)+1)*8); slot=m.alloc()
    m.write(slot,table); m.write(base+0x4ac0428,slot)
    m.write(table+0x1d8,max(item_rows)+1,4); m.write(table+0x1e0,pointers)
    for item_id,(offset,kind) in item_rows.items(): m.write(pointers+item_id*8,data+offset)
    m.u.mem_protect(data,(len(item_data)+4095)&~4095,UC_PROT_READ)
    classify_phone,classify_page,_,_=m.phone()
    classify_recipe=m.read(m.read(classify_phone+0x8a358))
    def classify(item_id):
        m.write(classify_recipe+8,item_id)
        m.confirm(classify_page); m.frame(classify_phone); m.frame(classify_phone)
        result=m.read(m.module.find('PreviewFixedPose'),1)
        m.hook(0x2e9d538,classify_phone)
        return result
    # Every fixed item, plus a real representative of every other ItemKind.
    for item_id in sorted(fixed_items|set(kinds.values())):
        assert classify(item_id)==int(item_id in fixed_items),(item_id,item_rows[item_id])
    fixed_id=next(iter(fixed_items)); offset,kind=item_rows[fixed_id]
    row=data+offset
    for field,value,size in ((0x1d8,0,4),(0x1d8,65537,4),(0x1e0,0,8),(0x1f8,fixed_id+1,2)):
        saved=m.read(table+field,size); m.write(table+field,value,size)
        assert classify(fixed_id)==0
        m.write(table+field,saved,size)
    saved=m.read(pointers+fixed_id*8); m.write(pointers+fixed_id*8,0)
    assert classify(fixed_id)==0
    m.write(pointers+fixed_id*8,saved)
    # Malformed headers/identity and both enum languages, using a writable copy.
    copy_offset=item_header+8*item_fields
    sample=bytearray(item_data[:copy_offset])+item_data[offset:offset+item_stride]
    struct.pack_into('<I',sample,0,1); struct.pack_into('<I',sample,copy_offset,copy_offset)
    copy=m.alloc(len(sample))
    m.u.mem_write(copy,bytes(sample)); m.write(pointers+fixed_id*8,copy+copy_offset)
    for field,value,size in ((0,0,4),(0,65537,4),(4,0,4),(4,4097,4),(8,513,2),(10,2,1),
                             (copy_offset,0,4),(copy_offset,64*1024*1024+1,4),
                             (copy_offset+item_offsets[0x54706054],fixed_id+1,2)):
        saved=m.read(copy+field,size); m.write(copy+field,value,size)
        assert classify(fixed_id)==0
        m.write(copy+field,saved,size)
    for hashed in fixed_hashes:
        m.write(copy+copy_offset+item_offsets[0x5270eb75],hashed,4)
        assert classify(fixed_id)==1
    m.write(pointers+fixed_id*8,row)
    for name,japanese in fixed_names.items():
        candidates={zlib.crc32(name.encode()),zlib.crc32(japanese.encode())}
        item_id=next(i for i,(o,k) in item_rows.items() if k in candidates)
        phone,page,focus,panes=m.phone()
        recipe=m.read(m.read(phone+0x8a358)); m.write(recipe+8,item_id)
        m.confirm(page); m.frame(phone); m.frame(phone)
        assert m.read(m.module.find('PreviewFixedPose'),1)==1
        assert f'native fixed pose=1 item={item_id}' in m.text
        pose=bytes(m.u.mem_read(m.controller+0x80,12))
        for right in ((1,0),(-1,0),(0,1),(0,-1),(1,1)):
            m.input(page,0,right=right); m.frame(phone)
            assert bytes(m.u.mem_read(m.controller+0x80,12))==pose
        # Supply distinct native poses on every hierarchy level. The hook must
        # pass them through verbatim, retaining native base angles/position/scale.
        for owner_kind,parent in ((0x42,0),(6,0x12345),(0x6f,0x112345)):
            owner=m.alloc(0x400); component=m.alloc(0x2c8)
            m.u.mem_write(component,bytes(m.u.mem_read(m.component,0x2c8)))
            m.write(component+0x48,owner); m.write(owner+0x15c,owner_kind,4)
            m.write(owner+0x230,0x12345 if owner_kind==0x42 else 0x212345,4)
            m.write(owner+0x234,parent,4)
            m.u.mem_write(component+0x2b0,struct.pack('<6I',0x08000000,0x10000000,0x02000000,
                                                   0x30000000,0x40000000,0x50000000))
            native=bytes(m.u.mem_read(component,0x2c8))
            assert m.drawable_matrix(component,protected=True)==m.drawable_matrix(component)
            assert bytes(m.u.mem_read(component,0x2c8))==native
        # Native parameter animation must also retain its supplied angles.
        supplied=m.alloc(); expected=struct.pack('<3I',0x12340000,0x45670000,0x789a0000)
        m.u.mem_write(supplied,expected)
        assert m.hook(0x32b9e08,m.controller,supplied)==1
        assert bytes(m.u.mem_read(m.component+0x2bc,12))==expected
        m.input(page,0x20); m.frame(phone)
        assert not m.read(m.module.find('PreviewFixedPose'),1)
        assert not m.read(m.module.find('PreviewHandle'),4)
        assert m.read(panes['L_BtnList_00']+0x58,1)&1
        m.hook(0x2e9d538,phone)
    # A normal item opened after a fixed preview must regain rotation.
    item_id=next(i for i,(o,k) in item_rows.items() if k==zlib.crc32(b'Ftr'))
    phone,page,focus,panes=m.phone(); recipe=m.read(m.read(phone+0x8a358)); m.write(recipe+8,item_id)
    m.confirm(page); m.frame(phone); m.frame(phone)
    assert not m.read(m.module.find('PreviewFixedPose'),1)
    before=bytes(m.u.mem_read(m.controller+0x80,12))
    m.input(page,0,right=(1,1)); m.frame(phone)
    assert bytes(m.u.mem_read(m.controller+0x80,12))!=before
    m.hook(0x2e9d538,phone)
    print(f'PASS: {len(fixed_items)} real walls/floors/rugs/fences fixed; every other kind sampled; native controller/carrier/drawable poses retained, right-stick ignored, B and rotating-item reopen; ASLR {base:#x}')

for base in (0x7100000000, 0x7200000000):
    m = PreviewMachine(base)
    verify_layout(MENU, m.patch(MENU)); ptr = m.build_pointer
    assert m.patch(MENU) and m.build_pointer == ptr
    wide = bytearray(MENU)
    # A modified display height and moved grid must survive byte for byte.
    struct.pack_into('<f', wide, 20+16, 1200)
    grid = next(s for s in layout_sections(MENU) if s[:4] == b'prt1' and pane_name(s) == 'L_BtnList_00')
    struct.pack_into('<f', wide, MENU.index(grid)+48, 47.5)
    verify_layout(bytes(wide), m.patch(bytes(wide)))
    other = layout_asset('WorkBenchInfo'); assert m.patch(other) == other
    damaged = bytearray(MENU); struct.pack_into('<I', damaged, 24, 0xffffffff)
    assert m.patch(bytes(damaged)) == damaged
    phone, page, row, panes = m.phone()
    assert 'input_row=0 focus_bindings=2' in m.text
    assert struct.unpack_from('<Q',RAW,0x49e7f10)[0]==0x3c58d24  # Actual MenuRecipePage input method.
    snapshot = bytes(m.u.mem_read(m.parameters+0x2c, 40))
    # A held while the app opens must be released first. Focus/legacy press
    # events alone must never queue a preview; no row-5 event is synthesized.
    m.input(page,0x10); m.frame(phone); assert not m.model_events
    for kind in (0,1,2,4,5): m.button(page,row,kind); m.frame(phone)
    assert not m.model_events
    m.confirm(page); m.frame(phone); assert not m.model_events  # Focus left the recipe.
    m.button(page,m.alloc(),0); m.confirm(page); m.frame(phone); assert not m.model_events
    m.button(page,row,0)
    assert m.read(phone+0x218,4)==0  # Actual native mapping, not a fake selection write.
    input_context=m.read(page+0x210)
    # Blocked/consumed A, inactive phone, pending transitions and empty rows
    # all preserve native input and leave the renderer untouched.
    for blocked,consumed in ((1,0),(0,0x10)):
        m.input(page,0); m.input(page,0x10,blocked=blocked,consumed=consumed); m.frame(phone)
        assert not m.model_events
    m.write(input_context+0x54,1,1); m.confirm(page); m.frame(phone); assert not m.model_events
    m.write(input_context+0x54,0,1)
    app=m.alloc(0x500); masks=m.alloc(); m.write(base+0x59f83f0,app); m.write(app+0x418,masks)
    m.write(input_context+0x58,4,4); m.write(masks+0x10,4,4)
    m.confirm(page); m.frame(phone); assert not m.model_events
    m.write(masks+0x10,0,4)
    m.write(phone+0x1b0,2,4); m.confirm(page); m.frame(phone); assert not m.model_events
    m.write(phone+0x1b0,3,4); m.write(phone+0x1f0,1,4)
    m.confirm(page); m.frame(phone); assert not m.model_events
    m.write(phone+0x1f0,0xffffffff,4)
    assert m.confirm(page)==0x10
    assert not m.model_events  # Work is queued, not run from the input stack.
    before=list(m.events); m.input(page,1); assert m.events==before
    m.frame(phone); m.frame(phone)
    assert m.read(m.controller+0x50,4)==2 and m.read(m.controller+0x10,4)==1
    assert m.invoke(m.read(m.read(phone)+0x200),phone)==2
    request=next(e[1] for e in m.events if e[0]=='spawn')
    assert struct.unpack('<IIIHHQ',request)==(0,0x44550088,0x00112233,0xfffe,0,0)
    assert m.read(panes['L_BtnList_00']+0x58,1)==0xa2
    assert m.read(panes['N_QolPreview_00']+0x58,1)==0xa3
    assert m.read(panes['N_QolCapture_00']+0x58,1)==0xa3
    assert m.read(panes['N_Recipe_00']+0x58,1)==0xa3
    # Native item layer clear reaches the GPU with the requested cream color;
    # other layers, target indices and depth/stencil must pass through intact.
    cream=struct.pack('<4f',1.5950871,1.3879482,.8751216,1)
    black=struct.pack('<4f',0,0,0,1)
    m.clear(); assert m.gpu_colors[-1]==(0x12345678,0,cream,15)
    native_draw_flags=m.layer_mask
    assert native_draw_flags&1
    m.clear(layer=1); assert m.gpu_colors[-1][2]==black
    assert m.layer_mask==native_draw_flags
    options=m.alloc(0x28); m.u.mem_write(options,bytes(range(40)))
    original_options=bytes(m.u.mem_read(options,40))
    m.clear(options=options); assert m.gpu_colors[-1][2]==cream
    assert bytes(m.u.mem_read(options,40))==original_options
    assert m.gpu_depths[-1]==(0x3f000000,0x12345678,1,7,0)
    assert bytes(m.u.mem_read(m.layers[2]+0x9c,16))==black
    # Right-stick queues the transform in UI phase 0; native late dispatch
    # resolves the registry and writes it in entity phase 1.
    # Preserve the item's initial pose/roll, materials and position.
    materials=bytes(m.u.mem_read(panes['N_Recipe_00'],0x200))
    position=bytes(m.u.mem_read(m.controller+0x78,8))
    angles=lambda: struct.unpack('<3I',bytes(m.u.mem_read(m.controller+0x80,12)))
    pose=angles()
    # Reproduce the old failure: direct entity rotation is rejected during
    # the app update, even with a valid loaded handle and ready renderer.
    m.write(m.model_entity+0x398,0,1)
    m.write(m.entity_manager+0x288,0,4)
    assert m.invoke(base+0x32b9e08,m.controller,m.controller+0x80)==0
    assert m.read(m.model_entity+0x398,1)==0
    m.write(m.entity_manager+0x288,1,4)
    assert m.invoke(base+0x32b9e08,m.controller,m.controller+0x80)==1
    assert m.read(m.model_entity+0x398,1)==0  # Material-publication gate is separate from rotation.
    for right in ((0,0),(.1,-.1),(float('nan'),float('inf'))):
        m.input(page,0,right=right); m.frame(phone); assert angles()==pose
    m.input(page,0,left=(1,1)); m.frame(phone); assert angles()==pose
    m.input(page,0,right=(1,1),consumed_axes=2); m.frame(phone); assert angles()==pose
    m.input(page,0,right=(1,1),blocked=1); m.frame(phone); assert angles()==pose
    m.write(input_context+0x54,1,1)
    m.input(page,0,right=(1,1)); m.frame(phone); assert angles()==pose
    m.write(input_context+0x54,0,1)
    m.input(page,0,right=(1,0)); assert m.read(m.last_input_row+0x40,4)==3
    assert angles()==pose  # Applied outside the input callback.
    # Re-dispatch the same consumed snapshot before the frame; it must not
    # erase the first callback's queued movement.
    duplicate=m.alloc(); m.write(duplicate+8,8,4)
    m.write(duplicate+0x10,m.last_input_row)
    m.hook(0x3c58d24,page,duplicate,0)
    m.frame(phone); changed=angles()
    assert m.read(m.model_entity+0x398,1)==1  # Native material-publication gate.
    assert not m.read(m.controller+0x74,1)&1
    assert changed[0]==pose[0] and changed[2]==pose[2]
    assert abs(((changed[1]-pose[1])&0xffffffff)/2**32*360-6)<.0001
    # Cancellation after a valid sample drains queued rotation in production,
    # unlike a second unblocked dispatch of the consumed snapshot above.
    m.input(page,0,right=(1,1)); m.hook(0x3c58d24,page,0,0)
    m.frame(phone); assert angles()==changed
    m.input(page,0,right=(1,1))
    m.write(duplicate+0x10,m.last_input_row)
    m.hook(0x3c58d24,page,duplicate,1)
    m.frame(phone); assert angles()==changed
    m.input(page,0,right=(0,1)); m.frame(phone)
    tilted=angles()
    assert abs(((changed[0]-tilted[0])&0xffffffff)/2**32*360-6)<.0001
    assert tilted[1:]==changed[1:]
    m.input(page,0,right=(0,-1)); m.frame(phone); assert angles()==changed
    assert bytes(m.u.mem_read(m.component+0x2bc,12))==bytes(m.u.mem_read(m.controller+0x80,12))
    rendered=m.drawable_matrix()
    # Reproduce a successful late update being overwritten by native parameter
    # animation; then execute the same native body through the setter hook.
    m.native_parameter_update(protected=False)
    assert m.drawable_matrix()!=rendered
    m.native_parameter_update(protected=True)
    assert m.drawable_matrix()==rendered
    # A different owner or model mode must retain the native animated pose.
    owner=m.module.find('PreviewController')
    for unowned in (True,False):
        m.write(owner,m.controller+0x1000 if unowned else m.controller)
        m.write(m.controller+0x50,2 if unowned else 7,4)
        m.native_parameter_update()
        assert m.drawable_matrix()!=rendered
        m.write(owner,m.controller); m.write(m.controller+0x50,2,4)
        m.native_parameter_update()
        assert m.drawable_matrix()==rendered
    # Real hierarchy: controller type 42 -> carrier type 6 -> drawable type 6F.
    # The old immediate-parent scope misses the displayed mesh completely.
    child=m.alloc(0x400); child_component=m.alloc(0x2c8)
    m.u.mem_write(child_component,bytes(m.u.mem_read(m.component,0x2c8)))
    m.write(child_component+0x48,child)
    m.write(child+0x230,0x212345,4); m.write(child+0x234,0x112345,4)
    m.write(child+0x15c,0x6f,4)
    m.u.mem_write(child_component+0x2bc,struct.pack('<3I',*pose))
    stale=bytes(m.u.mem_read(child_component,0x2c8))
    assert m.drawable_matrix(child_component)!=rendered
    # With no carrier publication this is precisely the previous scope: the
    # root and direct-parent comparisons both miss, despite successful input.
    carriers=m.module.find('PreviewCarriers')
    m.u.mem_write(carriers,bytes(16))
    assert m.drawable_matrix(child_component,protected=True)==m.drawable_matrix(child_component)
    m.hook(0x32b9fcc,m.controller)
    assert m.drawable_matrix(child_component,protected=True)==rendered
    assert bytes(m.u.mem_read(child_component,0x2c8))==stale
    m.write(child+0x234,0x98765,4)
    assert m.drawable_matrix(child_component,protected=True)==m.drawable_matrix(child_component)
    m.write(child+0x234,0x112345,4)
    for offset, value, size in ((0x15c, 6, 4), (0x234, 0x112344, 4)):
        original=m.read(child+offset,size); m.write(child+offset,value,size)
        assert m.drawable_matrix(child_component,protected=True)==m.drawable_matrix(child_component)
        m.write(child+offset,original,size)
    assert m.read(carriers,4)==0x112345
    for offset,value,size in ((0x230,0x12344,4),(0x248,2,4),(0x15c,6,4),(0x394,5,4),(0x274,0,4)):
        original=m.read(m.model_entity+offset,size); m.write(m.model_entity+offset,value,size)
        m.hook(0x32b9fcc,m.controller)
        assert m.read(carriers,4)==0
        assert m.drawable_matrix(child_component,protected=True)==m.drawable_matrix(child_component)
        m.write(m.model_entity+offset,original,size)
        m.hook(0x32b9fcc,m.controller)
        assert m.drawable_matrix(child_component,protected=True)==rendered
    handle=m.read(m.controller+0x6c,4)
    m.write(m.controller+0x6c,0,4); m.hook(0x32b9fcc,m.controller)
    assert bytes(m.u.mem_read(carriers,16))==bytes(16)
    m.write(m.controller+0x6c,handle,4); m.hook(0x32b9fcc,m.controller)
    assert m.drawable_matrix(child_component,protected=True)==rendered
    # All four carriers are supported; generation bits are compared intact.
    m.write(m.model_entity+0x394,4,4)
    for i in range(4): m.write(m.model_entity+0x274+4*i,0x112345+i,4)
    m.hook(0x32b9fcc,m.controller)
    for i in range(4):
        m.write(child+0x234,0x112345+i,4)
        assert m.drawable_matrix(child_component,protected=True)==rendered
    m.write(child+0x234,0x112345,4)
    m.frame(phone); assert angles()==changed  # No snapshot must not keep rotating.
    # Both directions, full turn wrap, bounded tilt and loaded/owned guards.
    m.input(page,0,right=(-1,0)); m.frame(phone); assert angles()==pose
    for _ in range(60): m.input(page,0,right=(1,0)); m.frame(phone)
    assert abs(((angles()[1]-pose[1]+2**31)&0xffffffff)-2**31)<512
    for _ in range(30): m.input(page,0,right=(0,1)); m.frame(phone)
    upper=angles(); m.input(page,0,right=(0,1)); m.frame(phone); assert angles()==upper
    for _ in range(60): m.input(page,0,right=(0,-1)); m.frame(phone)
    lower=angles(); assert lower[0]!=upper[0] and lower[2]==pose[2]
    m.input(page,0,right=(0,-1)); m.frame(phone); assert angles()==lower
    for off,value,size in ((0x74,2,1),(0x74,4,1),(0x6c,0,4),(0x50,7,4),(0x10,0,4)):
        saved=m.read(m.controller+off,size); m.write(m.controller+off,value,size)
        m.input(page,0,right=(1,1)); m.hook(0x2e9fbb0,phone); assert angles()==lower
        m.write(m.controller+off,saved,size)
    m.write(m.model_entity+0x230,0x54321,4)
    entity_pose=bytes(m.u.mem_read(m.component+0x2bc,12))
    m.input(page,0,right=(1,1)); m.frame(phone)
    assert bytes(m.u.mem_read(m.component+0x2bc,12))==entity_pose
    assert m.read(m.controller+0x74,1)&1  # Stale handle cannot apply or clear pending work.
    m.write(m.model_entity+0x230,0x12345,4)
    m.frame(phone); assert not m.read(m.controller+0x74,1)&1
    lower=angles()
    m.write(m.context+0x30,m.alloc(0x1200))
    m.input(page,0,right=(1,1)); m.hook(0x2e9fbb0,phone); assert angles()==lower
    m.write(m.context+0x30,m.controller)
    assert bytes(m.u.mem_read(m.controller+0x78,8))==position
    assert bytes(m.u.mem_read(panes['N_Recipe_00'],0x200))==materials
    before=list(m.events); m.input(page,2); assert m.events==before
    assert m.input(page,0x20,right=(1,1))==0x20; m.frame(phone)
    assert ('release',2) in m.events
    assert m.read(m.module.find('PreviewHandle'),4)==0
    assert bytes(m.u.mem_read(carriers,16))==bytes(16)
    assert m.drawable_matrix(child_component,protected=True)==m.drawable_matrix(child_component)
    m.clear(); assert m.gpu_colors[-1][2]==black
    assert m.read(m.controller+0x50,4)==26 and m.read(m.controller+0x10,4)==3
    assert m.invoke(m.read(m.read(phone)+0x200),phone)==26
    assert m.read(panes['L_BtnList_00']+0x58,1)==0xa3
    assert m.read(panes['N_QolPreview_00']+0x58,1)==0xa2
    assert m.read(panes['N_QolCapture_00']+0x58,1)==0xa2
    assert bytes(m.u.mem_read(m.parameters+0x2c,40))==snapshot and m.read(m.parameters+0x12,1)==1
    before=list(m.events); m.input(page,0x20); assert m.events==before
    m.input(page,0); m.input(page,0); assert m.events[-1]==('native_input',page)
    m.hook(0x2e9d538,phone); m.confirm(page); m.frame(phone); m.frame(phone)
    assert sum(e[0]=='spawn' for e in m.events)==2
    assert angles()==pose  # Reopening starts from the item's native pose.
    m.hook(0x2e9d538,phone); assert m.read(m.controller+0x6c,4)==0
    assert m.read(panes['N_QolCapture_00']+0x58,1)==0xa2
    print(f'PASS: native animation overwrite reproduced and prevented; grandchild drawable pose corrected without mutation and scoped HDR GPU clear; right stick only; dead zone, turn/wrap, bounded tilt, load/owner/handle guards, unchanged position/materials and reopen, ASLR {base:#x}')
    phone2,page2,row2,panes2=m.phone(1,surface=False)
    before=m.model_events; m.confirm(page2); m.frame(phone2); assert m.model_events==before
    unbound,unbound_page,unbound_row,unbound_panes=m.phone(3)
    m.write(unbound_panes['P_QolCapture_00']+0x100,0); m.hook(0x2e9cde0,unbound)
    m.button(unbound_page,unbound_row,0)
    before=m.model_events; m.confirm(unbound_page); m.frame(unbound); assert m.model_events==before
    routed,routed_page,routed_row,routed_panes=m.phone(5)
    hint=m.read(routed_page+0x570); key=m.read(hint+0xa0); before=m.model_events
    for kind,row_index in ((3,0),(5,8)):
        m.write(key+0x14,kind,4); m.write(key+0x10,row_index,4)
        m.hook(0x2e9cde0,routed); m.button(routed_page,routed_row,0)
        m.confirm(routed_page); m.frame(routed); assert m.model_events==before
    m.write(routed_page+0x570,0); m.hook(0x2e9cde0,routed)
    m.confirm(routed_page); m.frame(routed); assert m.model_events==before
    m.write(routed_page+0x570,hint); m.write(key+0x10,4,4)
    routed_grid=m.read(routed_page+0x580); second_row=m.alloc(); visible_rows=m.alloc()
    m.write(visible_rows,routed_row); m.write(visible_rows+8,second_row)
    m.write(routed_grid+0x210,visible_rows); m.write(routed_grid+0x208,2,4)
    m.write(routed_grid+0x430,1,4); m.write(routed_grid+0x3cc,6,4)
    recipes=m.alloc(); recipe=m.alloc(0xd0)
    for i in range(6): m.write(recipes+i*8,recipe)
    m.write(recipe+8,0x00112233445500bb)
    m.write(routed+0x8a350,6,4); m.write(routed+0x8a358,recipes)
    m.button(routed_page,second_row,0)
    assert m.read(routed+0x218,4)==5  # Second visible row after a four-item scroll.
    m.confirm(routed_page,resident=4); m.frame(routed); m.frame(routed)
    assert m.read(m.controller+0x6c,4)==0x12345
    assert struct.unpack('<IIIHHQ',m.model_events[-1][1])[1:3]==(0x445500bb,0x00112233)
    routed_pose=angles()
    m.input(routed_page,0,resident=0,right=(1,1)); m.frame(routed); assert angles()==routed_pose
    m.input(routed_page,0,resident=4,right=(1,1)); m.frame(routed); assert angles()!=routed_pose
    m.input(routed_page,0x20,resident=0); m.frame(routed); assert m.read(m.controller+0x6c,4)==0x12345
    assert m.input(routed_page,0x20,resident=4)==0x20; m.frame(routed)
    assert m.read(m.controller+0x6c,4)==0
    m.input(routed_page,0,resident=4)
    print('PASS: native base frame preserves temporary item mode and restores phone mode 26/state 3; snapshot A without press dispatch; native focus mapping/leave, held-A entry, blocked/consumed A, native shortcut routing and B return')
    bad_phone,bad_page,bad_row,bad_panes=m.phone(4)
    holder=m.read(bad_page+0x3f8); instance=m.read(holder)
    accessor=m.read(instance+0x58); native=m.read(accessor+8); original_root=m.read(native+0x18)
    old_root=m.read(instance+0x18); text=m.alloc(); m.u.mem_write(text,b'N_QolPreview_00\0')
    from unicorn import UcError
    for i,arg in enumerate((old_root,text,1)): m.u.reg_write(UC_ARM64_REG_X0+i,arg)
    try: m.call(base+0x27d1d0)
    except UcError:
        assert m.u.reg_read(UC_ARM64_REG_PC)==base+0x27d21c
        print('PASS: old wrapper cast reproduces invalid child traversal at native FindPane')
    else: raise AssertionError('old wrapper cast should fault in native pane lookup')
    before=m.model_events
    for location,value in ((native+0x18,old_root),(instance+0x58,0),(accessor+8,0),(original_root+0x28,0)):
        previous=m.read(location); m.write(location,value); m.hook(0x2e9cde0,bad_phone)
        m.button(bad_page,bad_row,0); m.confirm(bad_page); m.frame(bad_phone)
        assert m.model_events==before
        m.write(location,previous)
    phone3,page3,row3,panes3=m.phone(2)
    m.write(m.controller+0x50,7,4); before=m.model_events
    m.confirm(page3); m.frame(phone3); assert m.model_events==before
    m.write(m.controller+0x50,26,4); m.write(m.controller+0x10,3,4)
    for occupied in (0x6c,0x58,0x60):
        m.write(m.controller+occupied,1,4 if occupied==0x6c else 8)
        m.confirm(page3); m.frame(phone3); assert m.model_events==before
        m.write(m.controller+occupied,0,4 if occupied==0x6c else 8)
    m.fail_spawn=True
    m.confirm(page3); m.frame(phone3); m.frame(phone3)
    assert m.read(panes3['L_BtnList_00']+0x58,1)==0xa3
    assert m.read(panes3['N_QolPreview_00']+0x58,1)==0xa2
    assert m.read(panes3['N_QolCapture_00']+0x58,1)==0xa2
    assert m.read(m.controller+0x50,4)==26 and m.read(m.controller+0x10,4)==3
    # Unavailable layer metadata leaves an otherwise working preview usable;
    # the clear hook passes through rather than touching an unknown layer.
    m.fail_spawn=False
    fallback,fallback_page,fallback_row,fallback_panes=m.phone(6)
    m.write(m.layer_manager+0x20,2,4)
    m.confirm(fallback_page); m.frame(fallback); m.frame(fallback)
    assert m.read(m.controller+0x6c,4)==0x12345
    m.clear(); assert m.gpu_colors[-1][2]==black
    m.write(m.layer_manager+0x20,3,4)
    m.hook(0x2e9d538,fallback)
    m.clear(); assert m.gpu_colors[-1][2]==black

    # The genuinely idle controller path remains supported. Check restoration
    # before the outer app frame legitimately acquires its usual mode again.
    m.input(page3,0); m.fail_spawn=False
    m.write(m.controller+0x50,0xffffffff,4); m.write(m.controller+0x10,0,4)
    m.confirm(page3); m.frame(phone3); m.frame(phone3)
    assert 'renderer acquired; previous_mode=-1 previous_state=0' in m.text
    m.input(page3,0x20); m.hook(0x2e9fbb0,phone3)
    assert m.read(m.controller+0x50,4)==0xffffffff and m.read(m.controller+0x10,4)==0
    m.frame(phone3); assert m.read(m.controller+0x50,4)==26
    # Execute native Pane lookup against a real-shaped intrusive pane tree.
    root=m.alloc(); child=m.alloc(); table=m.alloc(); text=m.alloc()
    m.u.mem_write(root+0xb0,b'RootPane\0'); m.u.mem_write(child+0xb0,b'N_QolPreview_00\0')
    m.u.mem_write(text,b'N_QolPreview_00\0')
    m.write(root+0x28,child+8); m.write(child+8,root+0x20); m.write(child+16,root+0x20)
    m.write(child+0x28,child+0x20); m.write(child,table)
    m.write(table+0xc0,m.stub('child_find'))
    assert m.invoke(base+0x27d1d0,root,text,1)==child
    m.u.mem_write(text,b'Missing\0'); assert m.invoke(base+0x27d1d0,root,text,1)==0
    # Native item-model creation consumes the same 24-byte request used by
    # the compiled integration. Only scene allocation and parameter queries
    # are modeled. Item copying/type/flags and the no-background path are native.
    del m.services[base+0x32bd990]
    for a,action in ((0x32bb4ec,'noop'),(0x150cc10,'zero'),(0x6402e0,'entity_create'),(0x3f04470,'copy')):
        m.services[base+a]=('preview',action)
    strings=m.alloc(); empty=m.alloc(); m.u.mem_write(empty,b'\0')
    m.write(strings+0x18,m.stub('noop'))
    m.write(base+0x5643418,strings); m.write(base+0x5643420,empty)
    for got in (0x4ab2f70,0x4ab2bc0): m.write(base+got,strings-16)
    entity_slot=m.alloc(); entity=m.alloc(); camera_slot=m.alloc()
    m.write(entity_slot,entity); m.write(base+0x4ab27c0,entity_slot); m.write(base+0x4ab2200,camera_slot)
    params_table=m.alloc(0x200); m.write(params_table+0x168,m.stub('zero')); m.write(m.controller+0x7d0,params_table)
    m.write(m.controller+0x50,2,4); m.write(m.controller+0x6c,0,4)
    native_request=m.alloc(); m.u.mem_write(native_request,struct.pack('<IIIHHQ',0,0x44550088,0x00112233,0xfffe,0,0))
    m.invoke(base+0x32bd990,m.controller,native_request)
    assert m.read(m.controller+0x6c,4)==0x12345 and m.read(m.controller+0x70,4)==4
    assert m.read(m.controller+0x8c)==0x0011223344550088
    assert m.read(m.controller+0x60)==0 and m.read(m.controller+0x74,1)&3==3
    assert struct.unpack_from('<I',m.entity_request,4)[0]==0x42
    # Native release has no background-page dependency. An absent scene
    # manager models scene teardown; native code still clears its own handle.
    del m.services[base+0x32ba5e4]
    default_item=m.alloc(); m.write(default_item,0xfffe); m.write(base+0x4ab37e0,default_item)
    m.write(entity_slot,0); m.write(m.controller+0x10,1,4)
    m.invoke(base+0x32ba5e4,m.controller,2)
    assert m.read(m.controller+0x6c,4)==0 and m.read(m.controller+0x70,4)==0
    assert m.read(m.controller+0x8c)==0xfffe and m.read(m.controller+0x10,4)==0
    print(f'PASS: compiled resource patch preserves native/modified layout; focus dispatch alone cannot preview; snapshot input/B consumption, direct model ABI, parameter restoration, repeated open, deactivation and unavailable renderer, ASLR {base:#x}')

    # Parsing alone cannot prove model visibility: zero preserves the source
    # alpha, but the live 3D pass does not supply usable alpha for the item.
    # Execute both parser branches and require 255 in the authored layout above.
    m.services[base+0x27e610]=('preview','capture_data')
    m.capture_alpha=m.alloc(); m.write(m.capture_alpha+4,12,4)
    flags=m.alloc()
    for alpha,expected in ((255,2),(0,0)):
        m.write(m.capture_alpha+12,alpha,4); m.write(flags,0,1)
        assert m.invoke(base+0x3a2dad4,0,0,0,flags)==0
        assert m.read(flags,1)==expected
    print(f'PASS: native capture alpha parser; authored layout requires opaque model capture, ASLR {base:#x}')

# Exercise native state transitions and camera reference counts, not only
# modeled state-field writes. Delegate glue below supplies the fixture owner;
# native state callbacks, ChangeState and model release execute unchanged.
for base in (0x7100000000, 0x7200000000):
    m=PreviewMachine(base)
    del m.services[base+0x3dbb160]; del m.services[base+0x32ba5e4]
    entries=m.alloc(4*0x40); methods=m.alloc()
    enter=m.alloc(); leave=m.alloc()
    m.u.mem_write(enter,struct.pack('<III',0xf9400810,0xf9400400,0xd61f0200))
    m.u.mem_write(leave,struct.pack('<III',0xf9401010,0xf9400400,0xd61f0200))
    m.write(methods,enter); m.write(methods+0x10,leave)
    noop=m.stub('noop')
    for state,begin,end in ((0,base+0x32b8218,noop),(1,base+0x32b8270,base+0x32b85d0),
                            (2,noop,noop),(3,base+0x32b8924,base+0x32b89d8)):
        row=entries+state*0x40
        for off,value in ((0,methods),(8,m.controller),(0x10,begin),(0x20,end)): m.write(row+off,value)
    m.write(m.controller+0x30,4,4); m.write(m.controller+0x38,entries)
    host_slot=m.alloc(); host=m.alloc(0x400); scene=m.alloc()
    m.write(host_slot,host); m.write(host+0x330,scene); m.write(scene+8,4,4)
    m.write(base+0x4ab2720,host_slot); m.write(base+0x59ed670,host)
    camera=m.alloc(); counters=m.alloc()
    m.write(camera+0x68,counters); m.write(counters+0x3c,1,4) # Phone state 3 already entered.
    m.write(base+0x5131900,camera)
    for native in (0x3f03880,0x3f038c0): m.services[base+native]=('preview','noop')
    entity_slot=m.alloc(); m.write(base+0x4ab27c0,entity_slot)
    default_item=m.alloc(); m.write(default_item,0xfffe); m.write(base+0x4ab37e0,default_item)
    phone,page,row,panes=m.phone()
    assert m.invoke(m.read(base+0x49e8378),m.alloc(0x200))==26 # Unbound phone is native.
    for deactivate in (False,False,True):
        m.confirm(page); m.frame(phone); m.frame(phone)
        assert m.read(m.controller+0x50,4)==2 and m.read(m.controller+0x10,4)==1
        assert m.read(m.controller+0x69,1)==1 and m.read(counters+0x3c,4)==1
        if deactivate: m.hook(0x2e9d538,phone)
        else: m.input(page,0x20); m.frame(phone); m.input(page,0)
        assert m.read(m.controller+0x50,4)==26 and m.read(m.controller+0x10,4)==3
        assert m.read(m.controller+0x6c,4)==0 and m.read(m.controller+0x69,1)==0
        assert m.read(counters+0x3c,4)==1
    # Original mode getter demonstrably takes the renderer back after the
    # preview frame. This is the native outer-frame behavior old tests missed.
    m.confirm(page); m.frame(phone)
    m.write(m.read(phone)+0x200,base+0x2ea0754)
    m.frame(phone)
    assert m.read(m.controller+0x50,4)==26 and m.read(m.controller+0x10,4)==3
    print(f'PASS: native phone/preview camera counts balance across B/reopen/deactivation; original mode getter reproduces reclamation, ASLR {base:#x}')

# Diagnostics must describe failures as well as successful fixture updates;
# they must not turn a rejected transform into a success or alter unowned calls.
for base in (0x7100000000, 0x7200000000):
    m=PreviewMachine(base)
    phone,page,row,panes=m.phone()
    m.hook(0x32b9fcc,m.controller)
    assert m.read(m.module.find('TransformCalls'),4)==0
    m.confirm(page); m.frame(phone); m.frame(phone)
    m.write(m.controller+0x74,1,1)
    m.write(m.entity_manager+0x288,0,4)
    for _ in range(12): m.invoke(base+0x2b0e20c)
    assert m.read(m.controller+0x74,1)==1 and m.read(m.model_entity+0x398,1)==0
    lines=[s for s in m.logs if 'DIY preview: native transform;' in s]
    assert len(lines)==8 and all('flags=1->1' in s and 'entity_phase=0' in s for s in lines)
    m.write(m.entity_manager+0x288,1,4); m.invoke(base+0x2b0e20c)
    assert m.read(m.controller+0x74,1)==0 and m.read(m.model_entity+0x398,1)==1
    assert m.read(m.module.find('TransformApplied'),4)==1
    for _ in range(12): m.clear()
    m.clear(layer=1)
    lines=[s for s in m.logs if 'DIY preview: HDR background applied' in s]
    assert len(lines)==1
    assert m.read(m.module.find('BackgroundApplied'),4)==12
    assert all(m.read(m.module.find('LayerDrawScopes')+i*32)==0 for i in range(8))
    assert m.read(m.module.find('ClearCalls'),4)==13 and m.read(m.module.find('ClearMatches'),4)==12
    m.input(page,0x20); m.frame(phone)
    assert 'pending_attempts=13 applied=1 layer_draws=13 matched_layers=12' in m.text
    calls=m.read(m.module.find('TransformCalls'),4)
    m.invoke(base+0x2b0e20c); m.clear()
    assert m.read(m.module.find('TransformCalls'),4)==calls
    assert m.read(m.module.find('BackgroundApplied'),4)==12
    assert all(m.read(m.module.find('LayerDrawScopes')+i*32)==0 for i in range(8))
    assert m.read(m.module.find('ClearCalls'),4)==13
    print(f'PASS: bounded render diagnostics observe native rejection/success and clear matches without forcing updates; scope ends before release, ASLR {base:#x}')

for base in (0x7100000000, 0x7200000000):
    m=PreviewMachine(base)
    phone,page,row,panes=m.phone(hotel=True)
    banner=panes['N_TradeBox_00']; background=panes['P_TradeBoxBase_00']
    surface=panes['N_QolPreview_00']; parent=panes['N_Contents_00']
    root=m.read(parent+0x18)
    original_links=bytes(m.u.mem_read(banner+8,24))
    original_position=bytes(m.u.mem_read(banner+0x30,12))
    original_scale=bytes(m.u.mem_read(banner+0x48,8))
    def native_banner_check(current):
        if current==phone:
            assert bytes(m.u.mem_read(banner+8,24))==original_links
            assert bytes(m.u.mem_read(banner+0x30,12))==original_position
    m.native_banner_check=native_banner_check
    def check_corner(width=850,height=750,box_width=366,box_height=100,scale=(1,1)):
        assert m.read(banner+0x18)==surface
        # Background center is -30 in the banner, which remains native.
        x,y,z=struct.unpack('<3f',m.u.mem_read(banner+0x30,12))
        assert abs(x+(-30+box_width/2)*scale[0]-(width/2-16))<1e-4
        assert abs(y-box_height/2*scale[1]-(-height/2+16))<1e-4
        assert z==7 and bytes(m.u.mem_read(banner+0x48,8))==original_scale
        order=m.draw_order(root,panes)
        assert order.index('P_QolCapture_00')<order.index('P_TradeBoxBase_00')<order.index('T_TradeBoxNum_00')
        assert m.read(surface+0x20)==banner+8  # Banner is the last drawn child.
        assert m.read(banner+0x58,1)&0x10  # Native matrices marked dirty.
    m.confirm(page); m.frame(phone); m.frame(phone)
    check_corner()
    assert bytes(m.u.mem_read(panes['T_TradeBoxNum_00']+0x100,3))==b'12\0'
    # Localized/wider content and aspect-mod dimensions remain right/bottom
    # aligned after another native update; counts/styles are never replaced.
    m.u.mem_write(background+0x50,struct.pack('<2f',600,100))
    m.u.mem_write(surface+0x50,struct.pack('<2f',940,840))
    m.u.mem_write(panes['T_TradeBoxNum_00']+0x100,b'999\0')
    m.frame(phone); check_corner(940,840,600)
    assert bytes(m.u.mem_read(panes['T_TradeBoxNum_00']+0x100,4))==b'999\0'
    m.input(page,0x20); m.frame(phone)
    native_banner_check(phone)
    assert not m.read(surface+0x58,1)&1 and m.read(banner+0x58,1)&1
    order=m.draw_order(root,panes)
    assert 'P_TradeBoxBase_00' in order and 'P_QolCapture_00' not in order
    m.input(page,0); m.confirm(page); m.frame(phone); m.frame(phone)
    check_corner(940,840,600)
    m.hook(0x2e9d538,phone); native_banner_check(phone)
    # Native visibility remains authoritative for non-request recipes.
    m.write(banner+0x58,m.read(banner+0x58,1)&~1,1)
    m.confirm(page); m.frame(phone); m.frame(phone)
    assert not m.read(banner+0x58,1)&1
    assert 'P_TradeBoxBase_00' not in m.draw_order(root,panes)
    m.hook(0x2e9d538,phone); native_banner_check(phone)
    # Failed model creation or unsupported banner geometry does not alter
    # the native hierarchy/placement, and does not stop the preview lifecycle.
    m.fail_spawn=True; m.confirm(page); m.frame(phone); m.frame(phone)
    native_banner_check(phone)
    m.fail_spawn=False
    for geometry in ('rotation','nan','broken_links'):
        m.input(page,0)
        if geometry=='rotation': m.u.mem_write(background+0x44,struct.pack('<f',15))
        elif geometry=='nan': m.u.mem_write(background+0x50,struct.pack('<f',float('nan')))
        else: m.write(banner+8,0)
        m.native_banner_check=None
        m.confirm(page); m.frame(phone); m.frame(phone)
        assert m.read(surface+0x58,1)&1 and m.read(banner+0x18)==parent
        assert bytes(m.u.mem_read(banner+0x30,12))==original_position
        m.hook(0x2e9d538,phone)
        m.u.mem_write(banner+8,original_links)
        m.u.mem_write(background+0x44,struct.pack('<f',0))
        m.u.mem_write(background+0x50,struct.pack('<f',600))
    print(f'PASS: Hotel banner drawn after capture at bottom-right; dynamic widths/aspect dimensions, native count/visibility/updates, B/reopen/deactivation restoration and failure guards; ASLR {base:#x}')


def scene_pose_fixture(base):
    m=PreviewMachine(base); phone,page,row,panes=m.phone()
    m.confirm(page); m.frame(phone); m.frame(phone)
    root=m.model_entity
    m.write(base+0x59e7790,m.alloc())  # Native thread singleton, allocator boundary modeled.
    registry=m.read(m.entity_manager+0x1c0); entries=m.read(registry+8)
    carrier=m.alloc(0x450); component=m.alloc(0x300); methods=m.alloc(0x180)
    m.write(carrier,methods); m.write(methods+0x20,m.stub('accept'))
    for off,value in ((0x150,1),(0x15c,6),(0x230,0x112345),(0x234,0x12345)):
        m.write(carrier+off,value,4)
    m.write(carrier+0x68,component); m.write(entries+8,carrier)
    m.u.mem_write(component,bytes(m.u.mem_read(m.component,0x2c8)))
    m.write(component+0x48,carrier)
    component_methods=m.alloc(0x180); m.write(component,component_methods)
    m.write(component_methods+0x150,base+0x5f7b4c)
    m.write(component_methods+0xf8,base+0x5f7b18)
    # The actual item-effect update reads primary Euler directly, not the
    # extra view angles. Execute it through its effect-publication boundary.
    effect_state=m.alloc(0x340); m.write(effect_state+0x48,carrier)
    m.services[base+0x3738950]=('preview','noop')
    # Reproduce 0.3.51: secondary angles rotate drawing, while native
    # effect orientation remains zero because primary XYZ was unchanged.
    m.u.mem_write(component+0x2b0,bytes(12))
    m.u.mem_write(component+0x2bc,struct.pack('<3I',0,0x40000000,0))
    m.invoke(base+0x6904c0,effect_state)
    assert bytes(m.u.mem_read(effect_state+0x310,12))==bytes(12)
    # Native steady state after item/category framing, with no new request.
    default_item=m.alloc(); m.write(default_item,0xfffe); m.write(base+0x4ab37e0,default_item)
    sentinel=m.alloc(); m.write(sentinel,0xfffe,2); m.write(base+0x4ab3170,sentinel)
    m.write(root+0x39c,0,4); m.write(root+0x3a0,0xfffe,2)
    m.write(root+0x360,m.read(m.parameters+0x2c))
    origin=struct.unpack('<3I',m.u.mem_read(m.controller+0x80,12))
    m.u.mem_write(component+0x2bc,bytes(12))
    m.hook(0x192a590,root)
    assert bytes(m.u.mem_read(component+0x2b0,24))==bytes(24)
    drawable=m.alloc(0x400); mesh=m.alloc(0x300); mesh_methods=m.alloc(0x180)
    m.write(drawable+0x230,0x212345,4); m.write(drawable+0x234,0x112345,4)
    m.write(drawable+0x15c,0x6f,4); m.write(entries+16,drawable)
    m.write(drawable+0x68,mesh)
    m.write(mesh,mesh_methods); m.write(mesh+0x48,drawable)
    m.write(mesh_methods+0xf8,base+0x5f7b18)
    for off in (0x108,0x110,0x118): m.write(mesh_methods+off,m.stub('noop'))
    m.u.mem_write(component+0x2a0,struct.pack('<3f',1,1,1))
    # Execute native effect attachment composition; skeleton evaluation supplies
    # a bone world matrix derived from the real drawable component, without
    # the final drawing override. This reproduces the prior mesh/effect split.
    effect=m.alloc(0x450); model=m.alloc(0x280); wrappers=m.alloc(); wrapper=m.alloc(); bone=m.alloc(); bone_methods=m.alloc()
    m.write(model+0x38,wrappers); m.write(wrappers,wrapper); m.write(wrapper,bone)
    m.write(bone,bone_methods); m.write(bone_methods+0x70,m.stub('bone_world'))
    m.u.mem_write(model+0x268,struct.pack('<3f',1,1,1))
    m.u.mem_write(effect+0x3a4,struct.pack('<3f',1,1,1))
    local=(1,0,0,3,0,1,0,5,0,0,1,7)
    m.u.mem_write(effect+0x344,struct.pack('<12f',*local))
    output=m.alloc(48)
    previous=None
    for right in ((0,0),(1,0),(0,1),(-1,-1)):
        m.input(page,0,right=right); m.frame(phone)
        m.hook(0x192a590,root)
        expected=struct.pack('<3I',*((angle-initial)&0xffffffff for angle,initial in
                             zip(struct.unpack('<3I',m.u.mem_read(m.controller+0x80,12)),origin)))
        assert bytes(m.u.mem_read(component+0x2b0,12))==expected
        assert bytes(m.u.mem_read(component+0x2bc,12))==bytes(12)
        m.invoke(base+0x6904c0,effect_state)
        radians=struct.unpack('<3f',m.u.mem_read(effect_state+0x310,12))
        assert all(abs(a-b/2**32*2*math.pi)<1e-5 for a,b in zip(radians,struct.unpack('<3I',expected)))
        m.invoke(base+0x19282fc,mesh)  # Actual carrier -> drawable pose copy.
        assert bytes(m.u.mem_read(mesh+0x2b0,12))==expected
        assert bytes(m.u.mem_read(mesh+0x2bc,12))==bytes(12)
        m.write(effect_state+0x48,drawable); m.invoke(base+0x6904c0,effect_state)
        assert struct.unpack('<3f',m.u.mem_read(effect_state+0x310,12))==radians
        m.write(effect_state+0x48,carrier)
        m.bone_world=m.drawable_matrix(mesh)
        assert m.bone_world==m.drawable_matrix(mesh,protected=True)
        matrix=struct.unpack('<12f',m.bone_world)
        m.u.reg_write(UC_ARM64_REG_X8,output); m.invoke(base+0x5fc344,effect,model)
        attached=struct.unpack('<12f',m.u.mem_read(output,48))
        point=tuple(sum(matrix[i*4+j]*local[j*4+3] for j in range(3))+matrix[i*4+3] for i in range(3))
        assert all(abs(attached[i*4+3]-point[i])<1e-4 for i in range(3))
        if previous is not None: assert point!=previous
        previous=point
    # UI input may arrive after the scene tick. Drawing must keep the same
    # stored pose as bones until the next native scene update, avoiding drift.
    before=m.drawable_matrix(mesh)
    m.input(page,0,right=(1,1)); m.frame(phone)
    assert m.drawable_matrix(mesh,protected=True)==before
    m.hook(0x192a590,root); m.invoke(base+0x19282fc,mesh)
    assert m.drawable_matrix(mesh,protected=True)!=before
    saved=bytes(m.u.mem_read(component,0x2c8))
    for location,value,size in ((root+0x230,0x12344,4),(root+0x248,2,4),
                               (root+0x394,5,4),(carrier+0x230,0x112344,4),
                               (carrier+0x234,0x12344,4),(carrier+0x15c,0x6f,4),
                               (carrier+0x248,2,4),(component+0x48,0,8),
                               (m.module.find('PreviewRequestedFixedPose'),1,1),(component_methods+0x150,0,8)):
        old=m.read(location,size); m.write(location,value,size)
        m.u.mem_write(component+0x2b0,bytes(24)); before=bytes(m.u.mem_read(component,0x2c8))
        m.hook(0x192a590,root); assert bytes(m.u.mem_read(component,0x2c8))==before
        m.write(location,old,size); m.u.mem_write(component,saved)
    m.hook(0x2e9d538,phone)
    m.u.mem_write(component+0x2b0,bytes(24)); m.hook(0x192a590,root)
    assert bytes(m.u.mem_read(component+0x2b0,24))==bytes(24)
    print(f'PASS: primary carrier/drawable pose and native item-effect Euler publication, native bone attachment composition under yaw/tilt, generation/parent/type/dead/fixed/close guards; skeleton evaluation modeled; ASLR {base:#x}')

for base in (0x7100000000,0x7200000000): scene_pose_fixture(base)


def initial_pose_fixture(base):
    m=PreviewMachine(base); phone,page,_,_=m.phone()
    # Verify the test item against the real Log pack's resource row.
    items=(ASSETS.parent/'Bcsv/ItemParam.bcsv').read_bytes()
    count,stride,fields,version=struct.unpack_from('<IIHB',items)
    header=28 if version else 12; start=header+8*fields
    offsets=dict(struct.unpack_from('<II',items,header+8*i) for i in range(fields))
    row=start+(items.index(b'BagBackpackWood0')-start)//stride*stride
    log_pack=struct.unpack_from('<H',items,row+offsets[0x54706054])[0]
    assert log_pack==7498 and struct.unpack_from('<I',items,row+offsets[0x5270eb75])[0]==zlib.crc32(b'Bag')
    recipe=m.read(m.read(phone+0x8a358)); m.write(recipe+8,log_pack)
    m.confirm(page); m.frame(phone); m.frame(phone)
    root=m.model_entity
    m.write(base+0x59e7790,m.alloc())
    registry=m.read(m.entity_manager+0x1c0); entries=m.read(registry+8)
    carrier=m.alloc(0x450); component=m.alloc(0x300); methods=m.alloc(0x180)
    for off,value in ((0x15c,6),(0x230,0x112345),(0x234,0x12345)):
        m.write(carrier+off,value,4)
    m.write(carrier+0x68,component); m.write(entries+8,carrier)
    m.u.mem_write(component,bytes(m.u.mem_read(m.component,0x2c8)))
    m.write(component,methods); m.write(component+0x48,carrier)
    m.write(methods+0x150,base+0x5f7b4c); m.write(methods+0xf8,base+0x5f7b18)
    default=m.alloc(); m.write(default,0xfffe); m.write(base+0x4ab37e0,default)
    sentinel=m.alloc(); m.write(sentinel,0xfffe,2); m.write(base+0x4ab3170,sentinel)
    m.write(root+0x39c,0,4); m.write(root+0x3a0,0xfffe,2); m.write(root+0x360,log_pack)
    presets=native_preview_presets()|{(90,20,0),(-90,20,0),(0,20,90),(0,20,-90),(90,20,90)}
    epoch=m.module.find('PreviewPoseEpoch')
    origin=bytes(m.u.mem_read(m.controller+0x80,12))
    def setup(degrees):
        m.u.mem_write(m.controller+0x80,origin)
        native_angles=struct.pack('<3I',*(int(d/360*2**32)&0xffffffff for d in degrees))
        m.u.mem_write(component+0x2b0,bytes(12)); m.u.mem_write(component+0x2bc,native_angles)
        m.u.mem_write(component+0x2a0,struct.pack('<3f',1,1,1))
        m.write(epoch,m.read(epoch,4)+1,4)
        m.write(m.module.find('PreviewAngleXY'),int.from_bytes(origin[:8],'little'))
        m.write(m.module.find('PreviewAngleZ'),int.from_bytes(origin[8:],'little'),4)
    def matrix(): return struct.unpack('<12f',m.drawable_matrix(component))
    for preset in sorted(presets):
        setup(preset)
        native=matrix(); m.hook(0x192a590,root)
        assert max(abs(a-b) for a,b in zip(matrix(),native))<0.001,(preset,matrix(),native)
        assert bytes(m.u.mem_read(component+0x2bc,12))==bytes(12)
        stable=bytes(m.u.mem_read(component+0x2b0,12))
        for _ in range(3):
            m.hook(0x192a590,root)
            assert bytes(m.u.mem_read(component+0x2b0,12))==stable  # No accumulating preset.
    setup((15,-130,0)); native=matrix(); m.hook(0x192a590,root)
    initial=struct.unpack('<3I',m.u.mem_read(component+0x2b0,12))
    effect=m.alloc(0x340); m.write(effect+0x48,carrier)
    m.services[base+0x3738950]=('preview','noop')
    for right in ((1,0),(0,1),(-1,0),(0,-1)):
        m.input(page,0,right=right); m.frame(phone); m.hook(0x192a590,root)
        current=struct.unpack('<3I',m.u.mem_read(m.controller+0x80,12))
        wanted=tuple((a+b-c)&0xffffffff for a,b,c in zip(initial,current,struct.unpack('<3I',origin)))
        assert struct.unpack('<3I',m.u.mem_read(component+0x2b0,12))==wanted
        m.invoke(base+0x6904c0,effect)
        radians=struct.unpack('<3f',m.u.mem_read(effect+0x310,12))
        assert all(abs(a-b/2**32*2*math.pi)<1e-5 for a,b in zip(radians,wanted))
    assert max(abs(a-b) for a,b in zip(matrix(),native))<0.001
    # Loading and a stale previous item must not become the next recipe's base.
    m.write(m.module.find('UpdatePreviewActor','OrigRef'),m.stub('noop'))
    for location,value,size in ((root+0x39c,1,4),(root+0x39c,2,4),(root+0x360,log_pack+1,8)):
        old=m.read(location,size); m.write(location,value,size)
        before=bytes(m.u.mem_read(component,0x2c8)); m.hook(0x192a590,root)
        assert bytes(m.u.mem_read(component,0x2c8))==before
        m.write(location,old,size)
    # Reproduce a loading-phase reset of the still-visible previous carrier.
    # The displayed mesh/effects retain their last pose until native framing
    # has completed for the requested item, including a free -> fixed change.
    m.input(page,0,right=(1,1)); m.frame(phone); m.hook(0x192a590,root)
    displayed=matrix(); primary=bytes(m.u.mem_read(component+0x2b0,12))
    m.write(m.module.find('PreviewItem'),log_pack+1)
    m.write(epoch,m.read(epoch,4)+1,4)
    m.write(m.module.find('PreviewRequestedFixedPose'),1,1)
    m.write(root+0x39c,1,4); m.reset_component=component
    m.write(m.module.find('UpdatePreviewActor','OrigRef'),m.stub('reset_previous_pose'))
    m.hook(0x192a590,root)
    assert matrix()==displayed and bytes(m.u.mem_read(component+0x2b0,12))==primary
    assert m.read(m.module.find('PreviewFixedPose'),1)==0
    # Once framed, fixed targets keep the new native matrix, with no old tilt.
    m.write(root+0x39c,0,4); m.write(root+0x360,log_pack+1)
    m.u.mem_write(component+0x2b0,bytes(12)); m.u.mem_write(component+0x2bc,struct.pack('<3I',0,0x60000000,0))
    m.write(m.module.find('UpdatePreviewActor','OrigRef'),m.stub('noop'))
    next_pose=matrix(); m.hook(0x192a590,root)
    assert matrix()==next_pose and matrix()!=displayed
    assert m.read(m.module.find('PreviewFixedPose'),1)==1
    m.write(root+0x360,log_pack)
    # Reopening resets the epoch even if a modeled allocator reuses the handle.
    old_epoch=m.read(epoch,4); m.hook(0x2e9d538,phone)
    m.confirm(page); m.frame(phone); m.frame(phone); assert m.read(epoch,4)!=old_epoch
    m.u.mem_write(component+0x2b0,bytes(12)); m.u.mem_write(component+0x2bc,struct.pack('<3I',0,0x80000000,0))
    m.hook(0x192a590,root)
    assert m.read(component+0x2b4,4)==0x80000000
    print(f'PASS: {len(presets)} native/pole preset matrices retained, Log pack resource identity, cached origin without accumulation, yaw/tilt and particle radians relative to preset, loading/item guards and reopen epoch; parameter selection modeled; ASLR {base:#x}')

for base in (0x7100000000,0x7200000000): initial_pose_fixture(base)

for base in (0x7100000000,0x7200000000): navigation_fixture(base)


for base in (0x7100000000,0x7200000000):
    for pool in (5,15): quantity_wrap_fixture(base,pool)

def workbench_tilt_fixture(base):
    m=PreviewMachine(base)
    root=m.model_entity
    page=m.alloc(0x800); instance=m.alloc(); wrappers=m.alloc()
    # Native WorkCraftInfo construction selects this concrete page class.
    # Its virtual input callback is the shared dispatcher we hook.
    page_vtable=struct.unpack_from('<Q',RAW,0x4aca340)[0]+16
    assert page_vtable==0x4a3b3e0
    assert struct.unpack_from('<Q',RAW,page_vtable+0x50)[0]==0x3c58d24
    m.write(page,base+page_vtable)
    m.write(page+0x3f0,1,4); m.write(page+0x3f8,wrappers); m.write(wrappers,instance)
    sequence=m.alloc(0x1c210); m.write(sequence+0x1c1f8,page)
    assert m.invoke(base+0x346e990,sequence)==page
    # Run the actual visibility getter. +60 holds a nested loading icon,
    # not the top-level wrapper; the old ownership test always rejected it.
    m.workbench_instance=instance; m.workbench_loading_icon=m.alloc()
    m.services[base+0x2acb9a0]=('preview','loading_icon')
    loading=m.invoke(base+0x346e9c0,sequence)
    assert loading==m.workbench_loading_icon and loading!=instance
    input_context=m.alloc(); m.write(page+0x210,input_context)
    hint=m.alloc(); key=m.alloc(); m.write(page+0x570,hint); m.write(hint+0xa0,key)
    m.write(key+0x10,4,4)  # Test a nonzero resident row.
    # 034951BC indexes background pages at context +40, not controller +50.
    # Exercise an accepted item profile that differs from the old hardcoded 3.
    m.write(m.controller+0x60,loading); m.write(m.controller+0x50,6,4)
    m.write(m.controller+0x10,1,4); m.write(m.controller+0x6c,0x12345,4)
    item=7498; m.write(m.controller+0x94,item); m.write(root+0x360,item)
    m.write(root+0x174,4,4)  # Profile 6 maps to root mode 4 (0408E92C).
    m.write(root+0x39c,0,4); m.write(root+0x3a0,0xfffe,2)
    default=m.alloc(); m.write(default,0xfffe); m.write(base+0x4ab37e0,default)
    sentinel=m.alloc(); m.write(sentinel,0xfffe,2); m.write(base+0x4ab3170,sentinel)
    m.write(base+0x59e7790,m.alloc())
    entries=m.read(m.read(m.entity_manager+0x1c0)+8)
    child=m.alloc(0x450); component=m.alloc(0x300); methods=m.alloc(0x180)
    m.write(entries+8,child); m.write(child+0x68,component)
    for off,value in ((0x230,0x112345),(0x234,0x12345),(0x15c,6)): m.write(child+off,value,4)
    m.write(component,methods); m.write(component+0x48,child)
    m.write(methods+0x150,base+0x5f7b4c); m.write(methods+0xf8,base+0x5f7b18)
    m.u.mem_write(component+0x2a0,struct.pack('<3f',1,1,1))
    m.u.mem_write(component+0x2bc,struct.pack('<3I',0,0x80000000,0))
    original=m.drawable_matrix(component)
    def input(y=0,**kwargs): m.input(page,0x30,resident=4,right=(0.8,y),**kwargs)
    def tick(): m.hook(0x192a590,root)
    angle=m.module.find('WorkbenchTiltAngle')
    input(); tick(); assert m.drawable_matrix(component)==original
    input(1); tick(); assert m.drawable_matrix(component)!=original
    assert any(e==('native_input',page) for e in m.events)
    assert m.read(m.last_input_row+0x3c,4)==0  # A/B and X remain native.
    assert m.read(m.last_input_row+0x40,4)==0
    tilted=bytes(m.u.mem_read(component+0x2b0,24))
    for _ in range(12): tick(); assert bytes(m.u.mem_read(component+0x2b0,24))==tilted
    # Native horizontal primary rotation between ticks survives undo/reapply.
    m.write(component+0x2b4,(m.read(component+0x2b4,4)+0x08000000)&0xffffffff,4)
    tick(); assert bytes(m.u.mem_read(component+0x2b0,24))!=tilted
    effect=m.alloc(0x340); m.write(effect+0x48,child)
    m.services[base+0x3738950]=('preview','noop'); m.invoke(base+0x6904c0,effect)
    wanted=struct.unpack('<3I',m.u.mem_read(component+0x2b0,12))
    assert all(abs(a-b/2**32*2*math.pi)<1e-5 for a,b in zip(struct.unpack('<3f',m.u.mem_read(effect+0x310,12)),wanted))
    for y,kwargs in ((1,{'blocked':1}),(1,{'consumed_axes':2}),(float('nan'),{}),(0.1,{})):
        before=m.read(angle,4); input(y,**kwargs); tick(); assert m.read(angle,4)==before
    for _ in range(30): input(1); tick()
    assert abs((m.read(angle,4)-(1<<32))/2**32*360+60)<0.001
    for _ in range(60): input(-1); tick()
    assert abs(m.read(angle,4)/2**32*360-60)<0.001
    # The outgoing carrier keeps its displayed tilt throughout loading.
    displayed=m.drawable_matrix(component); before=m.read(angle,4)
    m.write(m.controller+0x74,4,1); input(1); tick(); assert m.read(angle,4)==before
    assert m.drawable_matrix(component)==displayed
    # A native parameter reset during loading must not expose the default pose.
    m.u.mem_write(component+0x2b0,bytes(24)); tick()
    assert m.drawable_matrix(component)==displayed
    m.write(m.controller+0x74,0,1); m.write(m.controller+0x94,item+1)
    input(); tick(); assert m.read(angle,4)==0
    for location,value,size in ((m.controller+0x50,26,4),(page,base+0x4a3b778,8),(m.controller+0x6c,0,4)):
        old=m.read(location,size); m.write(location,value,size)
        before=m.read(angle,4); input(1); tick(); assert m.read(angle,4)==before, (hex(location),value,before,m.read(angle,4))
        m.write(location,old,size)
    # Accept the native item-profile mask only on the concrete workbench page.
    # Never interpret background index 3 as the controller's current profile.
    accepted=[mode for mode in range(18) if (0x32dec >> mode)&1]
    for mode in accepted:
        m.write(m.controller+0x50,mode,4)
        input(1); tick(); assert m.read(angle,4)!=0, mode
    m.write(m.controller+0x50,6,4)
    input(); tick()
    probes=[line for line in m.logs if 'workbench tilt: input probe;' in line]
    assert probes and any('mode=6 ' in line for line in probes)
    before=len(probes)
    for _ in range(20): input(1)
    assert len([line for line in m.logs if 'workbench tilt: input probe;' in line])==before
    # Read real runtime BCSV kinds; all four fixed categories stay untouched.
    rows=m.alloc(); table=m.alloc(0x220); slot=m.alloc(); m.write(slot,table); m.write(base+0x4ac0428,slot)
    m.write(table+0x1f8,item,2); m.write(table+0x1d8,1,4); m.write(table+0x1e0,rows)
    header=m.alloc(64); row=header+28
    m.write(header,1,4); m.write(header+4,12,4); m.write(header+8,2,2)
    m.write(header+12,0x5270eb75,4); m.write(header+16,4,4)
    m.write(header+20,0x54706054,4); m.write(header+24,8,4)
    m.write(row,28,4); m.write(row+8,item,2); m.write(rows,row)
    m.write(root+0x360,item); m.write(m.controller+0x94,item)
    for kind in ('RoomWall','RoomFloor','Rug','Fence'):
        m.write(row+4,zlib.crc32(kind.encode()),4)
        input(1); tick(); saved=bytes(m.u.mem_read(component+0x2b0,24))
        input(1); tick(); assert bytes(m.u.mem_read(component+0x2b0,24))==saved
    print(f'PASS: native workbench page/visibility getters and native-selected profile binding, resident input/native dispatch retained, tilt limits/dead zone/blocking, stable primary pose/effect angles, native yaw changes, pending-item reset and fixed categories; layout allocation and yaw dispatch modeled; ASLR {base:#x}')

for base in (0x7100000000,0x7200000000): workbench_tilt_fixture(base)

for guard in GUARDS:
    m=Machine(0x7100000000,{'crafting':DEFAULT})
    m.write(m.base+guard,0xdeadbeef,4); m.boot(); m.activate()
    assert all(m.base+a not in m.hooks for a in HOOKS if a not in (0x3c58d24,0x192a590))
    assert 'DIY preview: DISABLED' in m.text
print('PASS: all DIY fingerprints reject DIY-specific hooks; independent workbench hook may remain')
m=Machine(0x7100000000,{'pockets':DEFAULT,'crafting':DEFAULT})
m.write(m.base+0x32bd990,0xdeadbeef,4); m.boot(); m.activate()
assert m.base+0x3c58d24 in m.hooks and m.base+0x2e9cde0 not in m.hooks
assert 'QUICK_STORE_READY' in m.text and 'DIY preview: DISABLED' in m.text
print('PASS: Pockets owns shared page input when the DIY integration declines installation')

# A changed mode slot must decline atomically before any DIY hook is installed.
m=Machine(0x7100000000,{'crafting':DEFAULT})
m.write(m.base+0x49e8378,m.base+0x2ea075c); m.boot(); m.activate()
assert all(m.base+a not in m.hooks for a in HOOKS if a not in (0x3c58d24,0x192a590))
assert 'renderer mode slot mismatch' in m.text
print('PASS: renderer mode vtable conflict leaves the DIY integration disabled')

# Execute the native phone footer builder, descriptor copying and pending ->
# displayed transfer. Only RTTI, pane lookup and string preparation are modeled.
for base in (0x7100000000, 0x7200000000):
    m = PreviewMachine(base)
    phone, page, row, panes = m.phone(resident=3)
    m.u.mem_write(base+0x2e9c670, RAW[0x2e9c670:0x2e9c670+20])
    m.write(m.module.find('BuildPhoneRecipeHints', 'OrigRef'), base+0x2e9c670)
    footer, methods, guard = m.alloc(0x300), m.alloc(), m.alloc()
    m.write(m.context+0x58, footer); m.write(footer, methods)
    m.write(methods, m.stub('accept')); m.write(guard, 1, 1)
    m.write(base+0x4ac7280, guard); m.write(base+0x4ac7278, m.alloc())
    for got in (0x4ab2020, 0x4ab2bc0, 0x4ab2f70):
        vt = m.alloc(0x40); m.write(vt+0x18, m.stub('zero')); m.write(base+got, vt-16)
    active_player = m.alloc(); m.write(base+0x4ab32e0, active_player)
    for offset, key in ((0x55cb998, b'sort'), (0x55cb9a8, b'close'), (0x55cb9b8, b'favorite')):
        text = m.alloc(); m.u.mem_write(text, key+b'\0')
        m.write(base+offset, m.read(base+0x4ab2bc0)+16); m.write(base+offset+8, text)
    m.services[base+0x3f04470] = ('preview', 'copy')
    m.services[base+0x2de2de0] = ('preview', 'footer_priority')
    m.services[base+0x2ab21c8] = ('preview', 'footer_grid')
    m.footer_grid = m.read(page+0x580); m.write(m.footer_grid+0x2cc, 7, 4)
    m.write(footer+0x1b0, 2, 4); m.write(footer+0x1f0, 0xffffffffffffffff)
    for start in (0x218, 0x248):
        entries, pool = m.alloc(), m.alloc(5*0x88)
        m.write(footer+start+4, 5, 4); m.write(footer+start+8, entries)
        m.write(footer+start+16, pool)
        for i in range(5): m.write(pool+i*0x88, pool+(i+1)*0x88 if i<4 else 0)
    def hints(start):
        entries = m.read(footer+start+8)
        records = [m.read(entries+i*8) for i in range(m.read(footer+start,4))]
        return [(m.read(p,4), m.string(m.read(p+0x18)), m.read(p+0x68,1)) for p in records]
    def flush():
        m.invoke(base+0x2de3e80, footer)
        assert not m.read(footer+0x218,4)
        return hints(0x248)
    native = [(6,'sort',3), (5,'close',3), (7,'favorite',3)]
    select = (4,'1201',3)
    m.hook(0x2e9c670, phone, 0)
    assert hints(0x218) == native+[select], hints(0x218)
    assert flush() == native+[select]
    # Category focus has no Select; returning to the grid restores it.
    m.hook(0x2e9c670, phone, 1); assert flush() == [(5,'close',3)]
    m.hook(0x2e9c670, phone, 0); assert flush() == native+[select]
    # A opens the preview, B returns to the grid; native hints are retained.
    m.confirm(page,resident=0); m.frame(phone)
    assert flush() == native
    m.frame(phone); m.input(page,0,resident=0)
    m.input(page,0x20,resident=0); m.frame(phone); m.frame(phone)
    assert flush() == native+[select]
    # If a batch is already pending, defer the rebuild until native consumes
    # it. The next update must neither duplicate hints nor lose the refresh.
    m.input(page,0,resident=0); m.hook(0x2e9c670,phone,0)
    m.confirm(page,resident=0); m.frame(phone)
    assert hints(0x218) == native+[select]
    flush(); m.frame(phone); assert flush() == native
    # A full footer cannot overrun its record pool or displace native hints.
    m.input(page,0,resident=0); m.input(page,0x20,resident=0)
    m.frame(phone); m.frame(phone); flush()
    m.write(footer+0x21c,3,4)
    m.hook(0x2e9c670,phone,0); assert flush() == native
    m.write(footer+0x21c,5,4)
    m.write(phone+0x8a350,0,4)
    m.hook(0x2e9c670,phone,0); assert flush() == native
    m.write(phone+0x8a350,1,4); m.write(phone+0x218,1,4)
    m.hook(0x2e9c670,phone,0); assert flush() == native
    m.write(phone+0x218,0,4)
    # Renderer contention leaves the grid visible and restores Select.
    m.write(m.controller+0x50,4,4)
    m.input(page,0); m.confirm(page); m.frame(phone); assert flush() == native
    m.frame(phone); assert flush() == native+[select]
print('PASS: native DIY A/Select footer, category/empty-list scope, preview open/close, renderer contention, pending batches and capacity at two bases')

# Starting work belongs to the acquired renderer context, not its raw cached
# address. Losing any part of that lease must cancel before model creation.
for base in (0x7100000000, 0x7200000000):
    for replacement in ('controller', 'parameters', 'mode'):
        m=PreviewMachine(base)
        phone,page,row,panes=m.phone()
        flags=m.read(panes['L_BtnList_00']+0x58,1)
        m.confirm(page);m.frame(phone)
        assert not any(event[0]=='spawn' for event in m.events)
        if replacement=='controller':
            foreign=m.alloc(0x1200);m.u.mem_write(foreign,b'\x5a'*0x1200)
            m.write(m.context+0x30,foreign)
        elif replacement=='parameters':
            foreign=m.alloc(0x2200);m.u.mem_write(foreign,b'\x5a'*0x2200)
            m.write(m.context+0x28,foreign)
        else:
            foreign=m.controller;m.write(foreign+0x50,7,4)
        before=bytes(m.u.mem_read(foreign,0x400))
        m.hook(0x2e9fbb0,phone)
        assert not any(event[0]=='spawn' for event in m.events)
        assert bytes(m.u.mem_read(foreign,0x400))==before
        assert m.read(m.module.find('PreviewHandle'),4)==0
        assert m.read(panes['L_BtnList_00']+0x58,1)==flags
        assert not m.read(panes['N_QolPreview_00']+0x58,1)&1
print('PASS: starting preview cancels on controller/parameter/mode replacement before model creation, preserves foreign context bytes and restores owned UI at two bases')
