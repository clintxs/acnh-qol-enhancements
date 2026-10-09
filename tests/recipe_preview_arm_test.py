"""Renderer-only spike: execute compiled hooks and native button dispatch.

The real 3.0.3 delegate translation/dispatch and model request/release bodies
run in ARM64. GPU rendering, scene allocation, camera state changes and layout
construction are explicitly modeled; these checks cannot establish live visuals.
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
GUARDS = HOOKS + (0x32bd990, 0x32ba5e4, 0x3dbb160, 0x27d1d0, 0x3a17390, 0x3a174f0)

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
        for a, action in ((0x27d1d0, 'find'), (0x3dbb160, 'state'),
                          (0x32bd990, 'spawn'), (0x32ba5e4, 'release'), (0x3a17390, 'typed'), (0x3a174f0, 'typed')):
            self.services[base + a] = ('preview', action)
        self.controller = self.alloc(0x1200)
        self.parameters = self.alloc(0x2200)
        self.context = self.alloc()
        self.write(self.context + 0x30, self.controller)
        self.write(self.context + 0x28, self.parameters)
        slot = self.alloc(); self.write(slot, self.context); self.write(base + 0x4ab2230, slot)
        self.write(self.controller + 0x50, 0xffffffff, 4)
        self.u.mem_write(self.parameters + 0x2c, bytes(range(40)))
        self.write(self.parameters + 0x12, 1, 1)
        for off in (0, 8, 16):
            self.write(base + 0x49e8408 + off, base + struct.unpack_from('<Q', RAW, 0x49e8408 + off)[0])
        self.dispatch = self.alloc(0x200)
        for off, target in ((0x1a8, self.stub('source')), (0x1b8, base + 0x375ab74),
                            (0x1c0, self.stub('noop')), (0x1c8, self.stub('accept')), (0x1d0, self.stub('noop'))):
            self.write(self.dispatch + off, target)

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
        if action == 'find': result = self.panes.get((self.reg(0), self.string(self.reg(1))), 0)
        elif action == 'state':
            self.events.append(('state', self.reg(1)))
            self.write(self.reg(0) + 8, self.reg(1), 4)
        elif action == 'spawn':
            request = bytes(u.mem_read(self.reg(1), 24)); self.events.append(('spawn', request))
            if not self.fail_spawn: self.write(self.reg(0) + 0x6c, 0x12345, 4)
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
        elif action == 'source': result = 7
        elif action == 'equals': result = int(self.string(self.reg(0)) == self.string(self.reg(1)))
        elif action == 'child_find':
            u.reg_write(UC_ARM64_REG_X2, 1); u.reg_write(UC_ARM64_REG_PC, self.base+0x27d1d0); return
        elif action == 'entity_create':
            self.entity_request = bytes(u.mem_read(self.reg(1), 0x30)); result = 0x12345
        elif action == 'zero': result = 0
        elif action == 'copy':
            u.mem_write(self.reg(0),bytes(u.mem_read(self.reg(1),self.reg(2)))); result=self.reg(0)
        elif action == 'accept': result = 1
        elif action in ('noop', 'bind', 'frame', 'deactivate'): pass
        else: raise AssertionError(action)
        u.reg_write(UC_ARM64_REG_X0, result); u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
    def phone(self, resident=0, surface=True):
        phone = self.alloc(0xf6380); page = self.alloc(0x600); grid = self.alloc(0x600)
        self.write(phone + 0x1f8, resident, 1); self.write(phone + 0x210, page)
        self.write(phone + 0x1b0, 3, 4); self.write(phone + 0x1f0, 0xffffffffffffffff)
        self.write(page, self.dispatch); self.write(page + 0x580, grid)
        holder = self.alloc(); native = self.alloc(); root = self.alloc()
        self.write(page + 0x3f8, holder); self.write(holder, native); self.write(native + 0x18, root)
        panes = {}
        for name in ('N_QolPreview_00', 'N_QolCapture_00', 'P_QolCapture_00', 'L_BtnList_00', 'L_ScrollBar_00', 'L_Cursor_00', 'N_Recipe_00'):
            pane = self.alloc(0x200); panes[name] = pane
            self.write(pane + 0x58, 0xa2 if name == 'N_QolPreview_00' else 0xa3, 1)
            if surface or name != 'N_QolPreview_00': self.panes[(root, name)] = pane
        self.write(panes['P_QolCapture_00']+0x100,panes['N_QolCapture_00'])
        row = self.alloc(); rows = self.alloc(); self.write(rows, row)
        self.write(grid + 0x208, 1, 4); self.write(grid + 0x210, rows)
        hint = self.alloc(); key = self.alloc(); self.write(page + 0x570, hint); self.write(hint + 0xa0, key)
        self.write(key + 0x14, 3, 4); self.write(key + 0x10, resident, 4)
        entries = self.alloc(35 * 0x20)
        self.write(page + 0x430, 1, 4); self.write(page + 0x434, 35, 4); self.write(page + 0x438, entries)
        for kind in range(35):
            self.write(entries + kind * 32, self.base + 0x49e8408)
            self.write(entries + kind * 32 + 8, phone)
            self.write(entries + kind * 32 + 16, self.base + (0x2e99d94 if kind == 5 else 0x2e99d90))
        recipes = self.alloc(); recipe = self.alloc(0xd0)
        self.write(recipes, recipe); self.write(recipe + 8, 0x0011223344550088)
        self.write(phone + 0x8a350, 1, 4); self.write(phone + 0x8a358, recipes)
        self.hook(0x2e9cde0, phone)
        return phone, page, row, panes
    def button(self, page, row, kind):
        event = self.alloc(0x20); self.write(event, kind, 4); self.write(event + 0x18, row)
        self.invoke(self.base + 0x375aa40, page, event)
    def frame(self, phone): self.hook(0x2e9fbb0, phone)
    def input(self, page, held, resident=0, blocked=0, consumed=0):
        snapshot = self.alloc(); rows = self.alloc(8 * 0x48)
        self.write(snapshot + 8, 8, 4); self.write(snapshot + 0x10, rows)
        self.write(rows + resident * 0x48, held, 4); self.write(rows + resident * 0x48 + 0x3c, consumed, 4)
        self.hook(0x3c58d24, page, snapshot, blocked)
        return self.read(rows + resident * 0x48 + 0x3c, 4)
    def patch(self, content):
        pointer = self.alloc(len(content)); self.u.mem_write(pointer, content)
        self.hook(0x267d64, 0, 0, 0, 0, 0, 0, pointer, 0)
        assert bytes(self.u.mem_read(pointer, len(content))) == content
        return self.built

def verify_layout(original, patched):
    before = layout_sections(original); after = layout_sections(patched)
    oldpanes = {pane_name(s): s for s in before if s[:4] in PANE_TYPES}
    newpanes = {pane_name(s): s for s in after if s[:4] in PANE_TYPES}
    assert all(newpanes[n] == s for n, s in oldpanes.items())
    assert not newpanes['N_QolPreview_00'][8] & 1
    assert newpanes['N_QolCapture_00'][8] & 1
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
    capture = material_records(next(s for s in after if s[:4] == b'mat1'))[-1]
    assert struct.unpack_from('<I', capture, 28)[0] == 0x15
    assert struct.unpack_from('<I', capture, 32)[0] == 0x08040200
    assert len(capture) == 84

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
    snapshot = bytes(m.u.mem_read(m.parameters+0x2c, 40))
    for kind in (0, 1, 2, 4): m.button(page, row, kind); m.frame(phone)
    assert not m.events and m.read(m.controller + 0x50, 4) == 0xffffffff
    m.button(page, m.alloc(), 5); m.frame(phone); assert not m.events
    m.button(page, row, 5); assert not m.events  # Queue after native dispatch.
    m.input(page, 1); assert not m.events  # Navigation suppressed immediately.
    m.frame(phone); m.frame(phone)
    request = next(e[1] for e in m.events if e[0] == 'spawn')
    assert struct.unpack('<IIIHHQ', request) == (0, 0x44550088, 0x00112233, 0xfffe, 0, 0)
    assert m.read(panes['L_BtnList_00']+0x58,1) == 0xa2
    assert m.read(panes['N_QolPreview_00']+0x58,1) == 0xa3
    assert m.read(panes['N_Recipe_00']+0x58,1) == 0xa3
    before = list(m.events); m.input(page, 2); assert m.events == before
    assert m.input(page, 0x20) == 0x20; m.frame(phone)
    assert ('release',2) in m.events
    assert m.read(panes['L_BtnList_00']+0x58,1) == 0xa3
    assert m.read(panes['N_QolPreview_00']+0x58,1) == 0xa2
    assert bytes(m.u.mem_read(m.parameters+0x2c,40)) == snapshot and m.read(m.parameters+0x12,1) == 1
    before = list(m.events); m.input(page,0x20); assert m.events == before
    m.input(page,0); m.input(page,0); assert m.events[-1] == ('native_input', page)
    # Native deactivation retains the existing app layout. Reopening can preview again.
    m.hook(0x2e9d538,phone); m.button(page,row,5); m.frame(phone); m.frame(phone)
    assert sum(e[0]=='spawn' for e in m.events) == 2
    m.hook(0x2e9d538,phone); assert m.read(m.controller+0x6c,4) == 0
    # Resource or renderer failure keeps the ordinary grid, without borrowed pages.
    phone2,page2,row2,panes2 = m.phone(1,surface=False)
    before=list(m.events); m.button(page2,row2,5); m.frame(phone2); assert m.events == before
    unbound,unbound_page,unbound_row,unbound_panes = m.phone(3)
    m.write(unbound_panes['P_QolCapture_00']+0x100,0)
    m.hook(0x2e9cde0,unbound)
    before=list(m.events); m.button(unbound_page,unbound_row,5); m.frame(unbound)
    assert m.events == before  # Named panes alone do not prove capture binding.
    phone3,page3,row3,panes3 = m.phone(2)
    m.write(m.controller+0x50,7,4); before=list(m.events)
    m.button(page3,row3,5); m.frame(phone3); assert m.events == before
    m.write(m.controller+0x50,0xffffffff,4); m.fail_spawn=True
    m.button(page3,row3,5); m.frame(phone3); m.frame(phone3)
    assert m.read(panes3['L_BtnList_00']+0x58,1) == 0xa3
    assert m.read(panes3['N_QolPreview_00']+0x58,1) == 0xa2
    # Execute native Pane lookup against a real-shaped intrusive pane tree.
    del m.services[base+0x27d1d0]
    m.services[base+0x2606c0]=('preview','equals')
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
    print(f'PASS: compiled resource patch preserves native/modified layout; native press dispatch excludes focus-leave; scoped input/B consumption, direct model ABI, parameter restoration, repeated open, deactivation and unavailable renderer, ASLR {base:#x}')

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
