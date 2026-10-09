"""Renderer-only spike: execute compiled hooks and native button dispatch.

The real 3.0.3 delegate translation/dispatch and model request/release bodies
run in ARM64. GPU rendering, scene allocation and layout construction are
modeled; native camera state callbacks run in the lifecycle fixture. These
checks cannot establish live visuals.
"""
from pathlib import Path
from compression import zstd
import sys
import struct
from core_arm_test import Machine, RAW, DEFAULT
from unicorn.arm64_const import *
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from build_catalog_badge_layout import resources, layout_sections, pane_name, PANE_TYPES, material_records

ASSETS = Path(__file__).resolve().parents[2] / 'vanilla-3.0.3/Layout'
def layout_asset(name):
    r = resources(zstd.decompress((ASSETS / (name + '.Nin_NX_NVN.zs')).read_bytes()))
    return next(v for k, v in r.items() if k.endswith('/' + name + '.bflyt'))
MENU = layout_asset('MenuRecipe')
HOOKS = (0x267d64, 0x2e9cde0, 0x2e9fbb0, 0x2e9d538, 0x3c58d24)
GUARDS = HOOKS + (0x32bd990, 0x32ba5e4, 0x3dbb160, 0x27d1d0, 0x3a17390, 0x3a174f0, 0x3c66298, 0x2e93798, 0x2e96a80, 0x3c4b880, 0x2ea0754, 0x2b0c810, 0x32b9e08, 0x32ba000, 0x3c4bac4)

class PreviewMachine(Machine):
    def __init__(self, base):
        self.next = base + 0x10210000
        self.events = []
        self.panes = {}
        self.fail_spawn = False
        # Also execute the shared dispatcher with Pockets/Fitting Room installed.
        files = {'pockets': DEFAULT, 'fitting-room': DEFAULT} if base == 0x7200000000 else {}
        super().__init__(base, files)
        self.u.mem_map(self.ram + 0x200000, 0x800000)
        self.boot(); self.activate()
        assert all(base + a in self.hooks for a in HOOKS)
        for name, action in [('BuildRecipeLayout', 'build'), ('BindPhoneRecipes', 'bind'),
                             ('UpdatePhoneRecipes', 'frame'), ('DeactivatePhoneRecipes', 'deactivate'),
                             ('SharedPageInput', 'input')]:
            self.write(self.module.find(name, 'OrigRef'), self.stub(action))
        for a, action in ((0x3dbb160, 'state'),
                          (0x32bd990, 'spawn'), (0x32ba5e4, 'release'), (0x3a17390, 'typed'), (0x3a174f0, 'typed')):
            self.services[base + a] = ('preview', action)
        self.pane_methods=self.alloc()
        self.write(self.pane_methods+0xc0,self.stub('child_find'))
        self.controller = self.alloc(0x1200)
        self.parameters = self.alloc(0x2200)
        self.context = self.alloc()
        self.write(self.context + 0x30, self.controller)
        self.write(self.context + 0x28, self.parameters)
        slot = self.alloc(); self.write(slot, self.context); self.write(base + 0x4ab2230, slot)
        # Native DIY app reserves mode 26/state 3 even without a model.
        self.write(self.controller + 0x50, 26, 4)
        self.write(self.controller + 0x10, 3, 4)
        self.write(base + 0x5639190, self.context)
        callback=self.alloc(); self.write(callback+0x10,self.stub('zero'))
        self.write(self.controller+0x1190,callback)
        self.u.mem_write(self.parameters + 0x2c, bytes(range(40)))
        self.write(self.parameters + 0x12, 1, 1)
        for off in (0, 8, 16):
            self.write(base + 0x49e8408 + off, base + struct.unpack_from('<Q', RAW, 0x49e8408 + off)[0])
        self.dispatch = self.alloc(0x200)
        for off, target in ((0x1a8, self.stub('source')), (0x1b8, base + 0x375ab74),
                            (0x1c0, self.stub('noop')), (0x1c8, self.stub('accept')), (0x1d0, self.stub('noop'))):
            self.write(self.dispatch + off, target)
        # Run the app's real shortcut setup; only shortcut allocation is modeled.
        self.services[base+0x375ad38] = ('preview', 'shortcut')
        self.write(base+0x4ac51b0,base+0x5549100)
        self.write(base+0x5549100,1,1)  # Native RTTI singleton already initialized.
        self.write(base+0x55ca7b8,1,1)
        self.write(base+0x55ca7b0,1,1); self.write(base+0x55ca7b4,1,1)
        self.services[base+0x2e96b80] = ('preview','noop')  # Material view update, after native index mapping.
        # Execute native SetModelRotation against a real-shaped entity registry;
        # it validates the live handle and writes Angle32 XYZ at component +2bc.
        manager_slot=self.alloc(); manager=self.alloc(0x600)
        registry=self.alloc(); entries=self.alloc(); entity=self.alloc(0x300)
        entity_methods=self.alloc(); self.component=self.alloc(0x300)
        self.write(manager_slot,manager); self.write(base+0x4ab27c0,manager_slot)
        self.write(manager+0x1c0,registry); self.write(registry+8,entries)
        self.write(entries,entity); self.write(entity,entity_methods)
        self.write(entity_methods+0x20,self.stub('accept'))
        self.write(entity+0x150,1,4); self.write(entity+0x230,0x12345,4)
        self.write(entity+0x68,self.component)
        # The native entity manager also checks its render-thread context.
        self.write(manager+0x288,1,4)
        self.render_thread=self.alloc(); thread_methods=self.alloc()
        self.write(self.render_thread,thread_methods)
        self.write(thread_methods+0x60,self.stub('zero'))
        thread_context=self.alloc(); thread_slot=self.alloc()
        self.write(thread_slot,thread_context); self.write(base+0x4ab3350,thread_slot)
        self.services[base+0x3f033a0]=('preview','render_thread')
        guard=self.alloc(); self.write(guard,1,1); self.write(base+0x4ab2e60,guard)
        self.write(base+0x4ab2e58,self.alloc())
        self.model_entity=entity

    def alloc(self, n=0x100):
        p = self.next; self.next += (n + 15) & ~15; return p
    def stub(self, action):
        p = self.alloc(0x10); self.services[p] = ('preview', action); return p
    def invoke(self, address, *args):
        for i, v in enumerate(args): self.u.reg_write(UC_ARM64_REG_X0 + i, v)
        try: self.call(address)
        except Exception:
            print('ARM fault', hex(self.u.reg_read(UC_ARM64_REG_PC)), hex(self.u.reg_read(UC_ARM64_REG_LR)), [hex(self.reg(i)) for i in range(8)])
            raise
        return self.reg(0)
    def hook(self, address, *args): return self.invoke(self.hooks[self.base + address], *args)
    def service(self, u, pc, size, data):
        if pc not in self.services or self.services[pc][0] != 'preview':
            return super().service(u, pc, size, data)
        action = self.services[pc][1]; result = 0
        if action == 'state':
            self.events.append(('state', self.reg(1)))
            self.write(self.reg(0) + 8, self.reg(1), 4)
        elif action == 'spawn':
            request = bytes(u.mem_read(self.reg(1), 24)); self.events.append(('spawn', request))
            if not self.fail_spawn:
                self.write(self.reg(0) + 0x6c, 0x12345, 4)
                u.mem_write(self.reg(0)+0x80,struct.pack('<3I',0x01000000,0x20000000,0x03000000))
        elif action == 'release':
            self.events.append(('release', self.reg(1)))
            self.write(self.reg(0) + 0x6c, 0, 4)
            self.write(self.reg(0) + 0x50, 0xffffffff, 4)
            self.write(self.reg(0) + 0x10, 0, 4)
        elif action == 'build':
            pointer = self.reg(6)
            n = self.read(pointer + 12, 4)
            self.built = bytes(u.mem_read(pointer, n)); self.build_pointer = pointer
            result = 1
        elif action == 'input': self.events.append(('native_input', self.reg(0)))
        elif action == 'typed': result = self.reg(0)
        elif action == 'layout_access':
            self.write(self.reg(1),self.read(self.reg(0)+8))
        elif action == 'shortcut':
            hint=self.alloc(); key=self.alloc()
            self.write(hint+0xa0,key)
            self.write(key+0x14,self.reg(2),4); self.write(key+0x10,self.reg(3),4)
            result=hint
        elif action == 'source': result = 7
        elif action == 'child_find':
            u.reg_write(UC_ARM64_REG_X2, 1); u.reg_write(UC_ARM64_REG_PC, self.base+0x27d1d0); return
        elif action == 'entity_create':
            self.entity_request = bytes(u.mem_read(self.reg(1), 0x30)); result = 0x12345
        elif action == 'zero': result = 0
        elif action == 'copy':
            u.mem_write(self.reg(0),bytes(u.mem_read(self.reg(1),self.reg(2)))); result=self.reg(0)
        elif action == 'accept': result = 1
        elif action == 'render_thread': result = self.render_thread
        elif action == 'capture_data':
            result = self.capture_alpha if self.string(self.reg(1)) == 'CaptureBGAlpha' else 0
        elif action in ('noop', 'bind', 'frame', 'deactivate'): pass
        else: raise AssertionError(action)
        u.reg_write(UC_ARM64_REG_X0, result); u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
    def phone(self, resident=0, surface=True):
        phone = self.alloc(0xf6380); page = self.alloc(0x600); grid = self.alloc(0x600)
        methods=self.alloc(0x208); self.write(phone,methods)
        self.write(methods+0x1a0,self.hooks[self.base+0x2e9fbb0])
        self.write(methods+0x200,self.read(self.base+0x49e8378))
        self.write(methods+0xe0,self.stub('accept'))
        self.write(phone + 0x1f8, resident, 1); self.write(phone + 0x210, page)
        self.write(phone + 0x1b0, 3, 4); self.write(phone + 0x1f0, 0xffffffffffffffff)
        self.write(page, self.dispatch); self.write(page + 0x580, grid)
        holder = self.alloc(); instance = self.alloc(0x300); native = self.alloc(); root = self.alloc(0x200)
        accessor=self.alloc(); methods=self.alloc()
        self.write(page+0x3f8,holder); self.write(holder,instance)
        # Wrapper +18 contains a SafeString table, as in the crash, not a Pane.
        self.write(instance+0x18,self.base+0x47f9770)
        self.write(instance+0x58,accessor); self.write(accessor,methods)
        self.write(accessor+8,native); self.write(methods+0x28,self.stub('layout_access'))
        self.write(native+0x18,root); self.u.mem_write(root+0xb0,b'RootPane\0')
        self.write(root,self.pane_methods); self.write(root+0x20,root+0x20); self.write(root+0x28,root+0x20)
        panes = {}
        for name in ('N_QolPreview_00', 'N_QolCapture_00', 'P_QolCapture_00', 'L_BtnList_00', 'L_ScrollBar_00', 'L_Cursor_00', 'N_Recipe_00'):
            pane = self.alloc(0x200); panes[name] = pane
            self.write(pane + 0x58, 0xa2 if name in ('N_QolPreview_00', 'N_QolCapture_00') else 0xa3, 1)
            self.u.mem_write(pane+0xb0,name.encode()+b'\0'); self.write(pane,self.pane_methods)
            self.write(pane+0x20,pane+0x20); self.write(pane+0x28,pane+0x20)
            if surface or name != 'N_QolPreview_00': self.panes[(root, name)] = pane
        children=[v for (r,n),v in self.panes.items() if r==root]
        for i,pane in enumerate(children):
            self.write(pane+8,children[i-1]+8 if i else root+0x20)
            self.write(pane+16,children[i+1]+8 if i+1<len(children) else root+0x20)
        self.write(root+0x20,children[-1]+8); self.write(root+0x28,children[0]+8)
        self.write(panes['P_QolCapture_00']+0x100,panes['N_QolCapture_00'])
        row = self.alloc(); rows = self.alloc(); self.write(rows, row)
        self.write(grid + 0x208, 1, 4); self.write(grid + 0x210, rows)
        self.write(grid+0x2e8,4,4); self.write(grid+0x3cc,1,4)
        self.write(page+0x210,self.alloc())  # Unblocked native input context.
        setup=self.alloc(); self.invoke(self.base+0x2e936ec,page,setup)
        hint=self.read(page+0x570); key=self.read(hint+0xa0)
        assert self.read(key+0x14,4)==5 and self.read(key+0x10,4)==0
        assert self.read(self.read(self.read(page+0x578)+0xa0)+0x14,4)==6
        entries = self.alloc(35 * 0x20)
        self.write(page + 0x430, 1, 4); self.write(page + 0x434, 35, 4); self.write(page + 0x438, entries)
        for kind in range(35):
            self.write(entries + kind * 32, self.base + 0x49e8408)
            self.write(entries + kind * 32 + 8, phone)
            self.write(entries + kind * 32 + 16, self.base + (0x2e96a80 if kind == 0 else 0x2e99d94 if kind == 5 else 0x2e99d90))
        recipes = self.alloc(); recipe = self.alloc(0xd0)
        self.write(recipes, recipe); self.write(recipe + 8, 0x0011223344550088)
        self.write(phone + 0x8a350, 1, 4); self.write(phone + 0x8a358, recipes)
        self.hook(0x2e9cde0, phone)
        self.button(page,row,0)
        return phone, page, row, panes
    @property
    def model_events(self): return [e for e in self.events if e[0]!='native_input']
    def confirm(self, page, resident=0):
        self.input(page,0,resident=resident)
        return self.input(page,0x10,resident=resident)
    def button(self, page, row, kind):
        event = self.alloc(0x20); self.write(event, kind, 4); self.write(event + 0x18, row)
        self.invoke(self.base + 0x375aa40, page, event)
    def frame(self, phone): self.invoke(self.base+0x2b0c720,phone)
    def input(self, page, held, resident=0, blocked=0, consumed=0, left=(0,0), right=(0,0), consumed_axes=0):
        snapshot = self.alloc(); rows = self.alloc(8 * 0x48)
        self.write(snapshot + 8, 8, 4); self.write(snapshot + 0x10, rows)
        self.write(rows + resident * 0x48, held, 4); self.write(rows + resident * 0x48 + 0x3c, consumed, 4)
        self.last_input_row=rows + resident * 0x48
        self.u.mem_write(self.last_input_row+4,struct.pack('<4f',*left,*right))
        self.write(self.last_input_row+0x40,consumed_axes,4)
        self.hook(0x3c58d24, page, snapshot, blocked)
        return self.read(rows + resident * 0x48 + 0x3c, 4)
    def patch(self, content):
        pointer = self.alloc(len(content)); self.u.mem_write(pointer, content)
        self.hook(0x267d64, 0, 0, 0, 0, 0, 0, pointer, 0)
        assert bytes(self.u.mem_read(pointer, len(content))) == content
        return self.built

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
    for tag in (b'mat1', b'txl1'):
        old = next(s for s in before if s[:4] == tag); new = next(s for s in after if s[:4] == tag)
        count = struct.unpack_from('<H', old, 8)[0]
        assert struct.unpack_from('<H', new, 8)[0] == count + 1
        if tag == b'mat1': assert material_records(new)[:-1] == material_records(old)
        else:
            def names(s, n):
                return [s[12+struct.unpack_from('<I', s, 12+4*i)[0]:].split(b'\0',1)[0] for i in range(n)]
            assert names(new, count) == names(old, count)
            assert names(new, count+1)[-1] == b'Capture^t'
    assert all(s in after for s in before if s[:4] not in (b'mat1', b'txl1'))
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
    # Right-stick rotation executes the native registry lookup and rotation
    # write. Preserve the item's initial pose/roll, materials and position.
    materials=bytes(m.u.mem_read(panes['N_Recipe_00'],0x200))
    position=bytes(m.u.mem_read(m.controller+0x78,8))
    angles=lambda: struct.unpack('<3I',bytes(m.u.mem_read(m.controller+0x80,12)))
    pose=angles()
    for right in ((0,0),(.1,-.1),(float('nan'),float('inf'))):
        m.input(page,0,right=right); m.frame(phone); assert angles()==pose
    m.input(page,0,left=(1,1)); m.frame(phone); assert angles()==pose
    m.input(page,0,right=(1,1),consumed_axes=2); m.frame(phone); assert angles()==pose
    m.input(page,0,right=(1,1),blocked=1); m.frame(phone); assert angles()==pose
    m.write(input_context+0x54,1,1)
    m.input(page,0,right=(1,1)); m.frame(phone); assert angles()==pose
    m.write(input_context+0x54,0,1)
    m.input(page,0,right=(1,0)); assert m.read(m.last_input_row+0x40,4)==2
    assert angles()==pose  # Applied outside the input callback.
    m.frame(phone); changed=angles()
    assert changed[0]==pose[0] and changed[2]==pose[2]
    assert abs(((pose[1]-changed[1])&0xffffffff)/2**32*360-3)<.0001
    assert bytes(m.u.mem_read(m.component+0x2bc,12))==bytes(m.u.mem_read(m.controller+0x80,12))
    m.frame(phone); assert angles()==changed  # No snapshot must not keep rotating.
    # Both directions, full turn wrap, bounded tilt and loaded/owned guards.
    m.input(page,0,right=(-1,0)); m.frame(phone); assert angles()==pose
    for _ in range(120): m.input(page,0,right=(1,0)); m.frame(phone)
    assert abs(((angles()[1]-pose[1]+2**31)&0xffffffff)-2**31)<512
    for _ in range(30): m.input(page,0,right=(0,1)); m.frame(phone)
    upper=angles(); m.input(page,0,right=(0,1)); m.frame(phone); assert angles()==upper
    for _ in range(60): m.input(page,0,right=(0,-1)); m.frame(phone)
    lower=angles(); assert lower[0]!=upper[0] and lower[2]==pose[2]
    m.input(page,0,right=(0,-1)); m.frame(phone); assert angles()==lower
    for off,value,size in ((0x74,1,1),(0x6c,0,4),(0x50,7,4),(0x10,0,4)):
        saved=m.read(m.controller+off,size); m.write(m.controller+off,value,size)
        m.input(page,0,right=(1,1)); m.hook(0x2e9fbb0,phone); assert angles()==lower
        m.write(m.controller+off,saved,size)
    m.write(m.model_entity+0x230,0x54321,4)
    m.input(page,0,right=(1,1)); m.frame(phone); assert angles()==lower
    m.write(m.model_entity+0x230,0x12345,4)
    m.write(m.context+0x30,m.alloc(0x1200))
    m.input(page,0,right=(1,1)); m.hook(0x2e9fbb0,phone); assert angles()==lower
    m.write(m.context+0x30,m.controller)
    assert bytes(m.u.mem_read(m.controller+0x78,8))==position
    assert bytes(m.u.mem_read(panes['N_Recipe_00'],0x200))==materials
    before=list(m.events); m.input(page,2); assert m.events==before
    assert m.input(page,0x20,right=(1,1))==0x20; m.frame(phone)
    assert ('release',2) in m.events
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
    print(f'PASS: native Angle32 writes from right stick only; dead zone, turn/wrap, bounded tilt, load/owner/handle guards, unchanged position/materials and reopen, ASLR {base:#x}')
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

for guard in GUARDS:
    m=Machine(0x7100000000,{})
    m.write(m.base+guard,0xdeadbeef,4); m.boot(); m.activate()
    assert all(m.base+a not in m.hooks for a in HOOKS)
    assert 'DIY preview: DISABLED' in m.text
print('PASS: all renderer/layout/input fingerprints reject the entire integration atomically')
m=Machine(0x7100000000,{'pockets':DEFAULT})
m.write(m.base+0x32bd990,0xdeadbeef,4); m.boot(); m.activate()
assert m.base+0x3c58d24 in m.hooks and m.base+0x2e9cde0 not in m.hooks
assert 'QUICK_STORE_READY' in m.text and 'DIY preview: DISABLED' in m.text
print('PASS: Pockets owns shared page input when the DIY integration declines installation')

# A changed mode slot must decline atomically before any DIY hook is installed.
m=Machine(0x7100000000,{})
m.write(m.base+0x49e8378,m.base+0x2ea075c); m.boot(); m.activate()
assert all(m.base+a not in m.hooks for a in HOOKS)
assert 'renderer mode slot mismatch' in m.text
print('PASS: renderer mode vtable conflict leaves the DIY integration disabled')
