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
import math
from core_arm_test import Machine, RAW, DEFAULT
from unicorn.arm64_const import *
from unicorn import UC_PROT_READ
import zlib
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from build_catalog_badge_layout import resources, layout_sections, pane_name, PANE_TYPES, material_records

ASSETS = Path(__file__).resolve().parents[2] / 'vanilla-3.0.3/Layout'
def layout_asset(name):
    r = resources(zstd.decompress((ASSETS / (name + '.Nin_NX_NVN.zs')).read_bytes()))
    return next(v for k, v in r.items() if k.endswith('/' + name + '.bflyt'))
MENU = layout_asset('MenuRecipe')
HOOKS = (0x2e9c670, 0x267d64, 0x2e9cde0, 0x2e9fbb0, 0x2e9d538, 0x3c58d24, 0x39392b0, 0x32b9fcc, 0x32b9e08, 0x6021a4, 0x39f63f0, 0x192a590)
GUARDS = HOOKS + (0x2de2b00,0x32bd990, 0x32ba5e4, 0x3dbb160, 0x27d1d0, 0x3a17390, 0x3a174f0, 0x3c66298, 0x2e93798, 0x2e96a80, 0x3c4b880, 0x2ea0754, 0x2b0c810, 0x32ba000, 0x3c4bac4, 0x2b0e20c, 0x32b9750, 0x1513a48, 0x32be2b0, 0x32ba034, 0x192a1f0, 0x19362f4)
GUARDS += (0x27bfb4, 0x27bfe4, 0x27d550, 0x27e208)
GUARDS += (0x32ba168, 0x3764290, 0x335b4a0, 0x3763e14, 0x3765104, 0x3c49700, 0x3c49710, 0x19282fc, 0x5f7b4c, 0x690504, 0x3f03370)

class PreviewMachine(Machine):
    def __init__(self, base):
        self.next = base + 0x10210000
        self.events = []
        self.panes = {}
        self.fail_spawn = False
        # Also execute the shared dispatcher with Pockets/Fitting Room installed.
        files = {'pockets': DEFAULT, 'fitting-room': DEFAULT} if base == 0x7200000000 else {}
        files['crafting'] = DEFAULT
        super().__init__(base, files)
        self.u.mem_map(self.ram + 0x200000, 0x800000)
        self.boot(); self.activate()
        self.u.mem_write(base+0x3764290,RAW[0x3764290:0x3764290+20])
        self.write(self.module.find('ResetQuantityGrid','OrigRef'),base+0x3764290)
        self.write(base+0x4ac0428,0)  # No live ItemParam resource until explicitly supplied.
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
        self.write(base + 0x4ac7280, 0)  # No footer unless a fixture supplies one.
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
        registry=self.alloc(); entries=self.alloc(); entity=self.alloc(0x400)
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
        item_guard=self.alloc(); self.write(item_guard,1,1); self.write(base+0x4abfcf8,item_guard)
        self.write(base+0x4abfcf0,self.alloc())
        self.write(entity+0x15c,0x42,4)
        self.write(entity+0x394,1,4)
        self.write(entity+0x274,0x112345,4)
        self.u.mem_write(base+0x192a590,RAW[0x192a590:0x192a590+20])
        self.write(self.module.find('UpdatePreviewActor','OrigRef'),base+0x192a590)
        self.write(entity+0x174,4,4); self.write(entity+0x39c,3,4)
        self.model_entity=entity
        self.entity_manager=manager
        component_methods=self.alloc(0x108); self.write(self.component,component_methods)
        self.write(component_methods+0x100,self.stub('accept'))  # Native position write boundary.
        # The fixture explicitly schedules a late transform pump in phase 1.
        # The live app's scheduling/phase is NOT established by this fixture.
        # Execute the observer and its original native pump, redirecting only
        # the late-dispatch tail branch to stand in for Exlaunch's trampoline.
        self.u.mem_write(base+0x32b9fcc,RAW[0x32b9fcc:0x32b9fcc+20])
        self.write(self.module.find('TraceModelTransform','OrigRef'),base+0x32b9fcc)
        self.u.mem_write(base+0x32b9e08,RAW[0x32b9e08:0x32b9e08+20])
        self.write(self.module.find('SetPreviewRotation','OrigRef'),base+0x32b9e08)
        for call in (0x32ba008,0x32b9754):
            delta=(self.hooks[base+0x32b9e08]-(base+call))//4
            self.write(base+call,0x94000000|(delta&0x3ffffff),4)
        tail=base+0x2b0e21c
        delta=(self.hooks[base+0x32b9fcc]-tail)//4
        assert -(1<<25)<=delta<(1<<25)
        self.write(tail,0x14000000|(delta&0x3ffffff),4)
        self.write(self.controller+0x69,1,1)
        self.layer_manager=self.alloc(); self.layer_array=self.alloc()
        self.layers=[self.alloc(0xa80) for _ in range(3)]
        self.write(self.layer_manager+0x20,3,4); self.write(self.layer_manager+0x28,self.layer_array)
        for i,layer in enumerate(self.layers):
            self.write(self.layer_array+i*8,layer)
            self.u.mem_write(layer+0x9c,struct.pack('<4f',0,0,0,1))
        layer_slot=self.alloc(); self.write(layer_slot,self.layer_manager); self.write(base+0x4abdb78,layer_slot)
        # Execute native clear through the hook's original, through to its NVN
        # color/depth calls. Only viewport setup and the GPU calls are modeled.
        self.u.mem_write(base+0x38025b0,RAW[0x38025b0:0x38025b0+20])
        self.u.mem_write(base+0x39392b0,RAW[0x39392b0:0x39392b0+20])
        self.write(self.module.find('DrawPreviewLayer','OrigRef'),base+0x39392b0)
        self.write(self.module.find('DrawPreviewContents','OrigRef'),self.stub('layer_color'))
        call=base+0x39393dc
        delta=(self.hooks[base+0x39f63f0]-call)//4
        self.write(call,0x94000000|(delta&0x3ffffff),4)
        self.services[base+0x39393e0]=('preview','finish_layer')
        self.u.mem_write(base+0x6021a4,RAW[0x6021a4:0x6021a4+20])
        self.write(self.module.find('BuildPreviewMatrix','OrigRef'),base+0x6021a4)
        self.write(self.component+0x48,self.model_entity)
        tail=base+0x19283f8
        delta=(self.hooks[base+0x6021a4]-tail)//4
        self.write(tail,0x14000000|(delta&0x3ffffff),4)
        self.layer_renderer=self.alloc(0x250); layer_state=self.alloc()
        self.write(self.layer_renderer+0x248,layer_state)
        self.write(layer_state+8,1,4); self.write(layer_state+0x10,self.alloc(0x23c0))
        self.layer_context=self.alloc(0x3d8); self.write(self.layer_context+0x18,self.alloc())
        self.write(self.layer_context+0x318,self.alloc())  # Native color-clear request.
        # Native matrix builder uses the game's Angle32 sine/cosine table.
        self.write(base+0x4ab2d00,base+struct.unpack_from('<Q',RAW,0x4ab2d00)[0])
        self.matrix=self.alloc(48)
        self.services[base+0x32cf210]=('preview','noop')
        self.services[base+0x32b74f0]=('preview','animation_pose')
        self.services[base+0x3f03370]=('preview','atan2')  # SDK math import boundary.
        text_methods=self.alloc(); self.write(text_methods+0x18,self.stub('noop'))
        self.write(base+0x5643418,text_methods); self.write(base+0x5643420,self.alloc())
        self.services[base+0x3f04470]=('preview','copy')
        strings=self.alloc(); self.write(base+0x4ab2f70,strings); self.write(base+0x4ab2bc0,strings)

        self.services[base+0x37e77e4]=('preview','noop')
        clock_slot=self.alloc(); self.write(clock_slot,1000000000); self.write(base+0x4ab29d0,clock_slot)
        color_slot=self.alloc(); self.write(color_slot,self.stub('gpu_clear'))
        self.write(base+0x4acfd90,color_slot)
        depth_slot=self.alloc(); self.write(depth_slot,self.stub('gpu_depth'))
        self.write(base+0x4acf298,depth_slot)
        self.clear_buffer=self.alloc(); texture=self.alloc(0xc0)
        self.write(self.clear_buffer+0x20,texture); self.write(self.clear_buffer+0x28,texture)
        self.write(self.clear_buffer+0x60,texture)
        self.clear_command=self.alloc(); self.write(self.clear_command+0xb8,0x12345678)
        self.gpu_colors=[]; self.gpu_depths=[]
        self.quantity_tick=0
        self.services[self.module.find('svcGetSystemTick')]=('preview','quantity_tick')

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
        if action == 'quantity_tick':
            result = self.quantity_tick
        elif action == 'atan2':
            y=struct.unpack('<f',struct.pack('<I',u.reg_read(UC_ARM64_REG_S0)))[0]
            x=struct.unpack('<f',struct.pack('<I',u.reg_read(UC_ARM64_REG_S1)))[0]
            u.reg_write(UC_ARM64_REG_S0,struct.unpack('<I',struct.pack('<f',math.atan2(y,x)))[0])
        elif action == 'reset_previous_pose':
            u.mem_write(self.reset_component+0x2b0,bytes(12))
            u.mem_write(self.reset_component+0x2bc,struct.pack('<3I',0,0x20000000,0))
        elif action == 'state':
            self.events.append(('state', self.reg(1)))
            self.write(self.reg(0) + 8, self.reg(1), 4)
        elif action == 'spawn':
            request = bytes(u.mem_read(self.reg(1), 24)); self.events.append(('spawn', request))
            if not self.fail_spawn:
                self.write(self.reg(0) + 0x6c, 0x12345, 4)
                self.write(self.reg(0) + 0x70, 4, 4)
                self.write(self.reg(0) + 0x9c, 0xfffe, 2)
                u.mem_write(self.reg(0)+0x80,struct.pack('<3I',0x01000000,0x20000000,0x03000000))
        elif action == 'release':
            self.events.append(('release', self.reg(1)))
            self.write(self.reg(0) + 0x6c, 0, 4)
            self.write(self.reg(0) + 0x50, 0xffffffff, 4)
            self.write(self.reg(0) + 0x10, 0, 4)
            self.write(self.reg(0) + 0x74, 0, 1)
        elif action == 'build':
            pointer = self.reg(6)
            n = self.read(pointer + 12, 4)
            self.built = bytes(u.mem_read(pointer, n)); self.build_pointer = pointer
            result = 1
        elif action == 'bone_world':
            u.mem_write(self.reg(1),self.bone_world)  # Skeleton evaluation boundary.
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
        elif action == 'footer_grid': result = self.footer_grid
        elif action == 'footer_priority': result = self.reg(0)
        elif action == 'loading_icon':
            assert self.reg(0) == self.workbench_instance + 0x38
            assert self.string(self.read(self.reg(1)+8)) == '/L_LoadingIcon_00'
            result = self.workbench_loading_icon
        elif action == 'child_find':
            u.reg_write(UC_ARM64_REG_X2, 1); u.reg_write(UC_ARM64_REG_PC, self.base+0x27d1d0); return
        elif action == 'entity_create':
            self.entity_request = bytes(u.mem_read(self.reg(1), 0x30))
            self.events.append(("entity_create", self.reg(2), self.entity_request))
            result = 0x12345
        elif action == 'allocate_component': result = self.alloc(self.reg(0))
        elif action == 'construct_component':
            name=self.alloc(0x40)
            self.write(self.reg(0)+0x60,name); self.write(self.reg(0)+0x68,0x40,4)
        elif action == 'attach_component':
            self.write(self.reg(1)+0x48,self.reg(0))
            if not self.read(self.reg(0)+0x68): self.write(self.reg(0)+0x68,self.reg(1))
        elif action == 'layer_color':
            self.layer_color=bytes(u.mem_read(self.reg(4),16))
            self.layer_mask=self.reg(3)
        elif action == 'finish_layer':
            u.reg_write(UC_ARM64_REG_PC,self.base+0x3939528); return
        elif action == 'animation_pose':
            u.mem_write(self.reg(0),struct.pack('<2fI3If',0,0,0x3f800000,0x01000000,0x20000000,0x03000000,5))
        elif action == 'quantity_reset_call':
            u.reg_write(UC_ARM64_REG_LR,self.quantity_reset_caller)
            u.reg_write(UC_ARM64_REG_PC,self.hooks[self.base+0x3764290]); return
        elif action == 'quantity_reset_return':
            u.reg_write(UC_ARM64_REG_PC,self.ram+0x100); return
        elif action == 'quantity_scroll':
            grid=self.reg(0)
            self.events.append(('quantity_scroll',self.string(self.read(self.reg(1)+8)),
                                self.reg(2),self.read(self.read(grid+0x468)+0x40,4),
                                self.read(grid+0x418,4),self.read(grid+0x428,4)))
        elif action == 'quantity_number':
            self.quantity_numbers[self.reg(0)]=self.reg(1)
            self.events.append(('quantity_number',self.reg(0),self.reg(1)))
        elif action == 'quantity_position':
            self.events.append(('quantity_position',bytes(u.mem_read(self.reg(1),12))))
        elif action == 'grid_enter': self.events.append(('grid_enter', self.reg(2)))
        elif action == 'grid_leave': self.events.append(('grid_leave', self.reg(2)))
        elif action == 'change_item':
            self.events.append(('change_item', bytes(u.mem_read(self.reg(1),0x30))))
        elif action == 'details': self.events.append(('details', self.reg(1)))
        elif action == 'zero': result = 0
        elif action == 'copy':
            u.mem_write(self.reg(0),bytes(u.mem_read(self.reg(1),self.reg(2)))); result=self.reg(0)
        elif action == 'accept': result = 1
        elif action == 'render_thread': result = self.render_thread
        elif action == 'gpu_clear':
            self.gpu_colors.append((self.reg(0),self.reg(1),bytes(u.mem_read(self.reg(2),16)),self.reg(3)))
        elif action == 'gpu_depth':
            self.gpu_depths.append((u.reg_read(UC_ARM64_REG_S0),self.reg(0),self.reg(1),self.reg(2),self.reg(3)))
        elif action == 'capture_data':
            result = self.capture_alpha if self.string(self.reg(1)) == 'CaptureBGAlpha' else 0
        elif action == 'pane_draw': self.drawn.append(self.string(self.reg(0)+0xb0))
        elif action in ('frame', 'deactivate'):
            if getattr(self,'native_banner_check',None): self.native_banner_check(self.reg(0))
        elif action in ('noop', 'bind'): pass
        else: raise AssertionError(action)
        u.reg_write(UC_ARM64_REG_X0, result); u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
    def phone(self, resident=0, surface=True, hotel=False):
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
        names = ('N_QolPreview_00', 'N_QolCapture_00', 'P_QolCapture_00', 'L_BtnList_00', 'L_ScrollBar_00', 'L_Cursor_00', 'N_Recipe_00')
        if hotel: names += ('N_Contents_00', 'N_TradeBox_00', 'N_Align_02', 'P_TradeBoxBase_00', 'T_TradeBoxNum_00')
        for name in names:
            pane = self.alloc(0x200); panes[name] = pane
            self.write(pane + 0x58, 0xa2 if name in ('N_QolPreview_00', 'N_QolCapture_00') else 0xa3, 1)
            self.u.mem_write(pane+0xb0,name.encode()+b'\0'); self.write(pane,self.pane_methods)
            self.write(pane+0x20,pane+0x20); self.write(pane+0x28,pane+0x20)
            self.u.mem_write(pane+0x48,struct.pack('<4f',1,1,850,750))
            if surface or name != 'N_QolPreview_00': self.panes[(root, name)] = pane
        parents = {name: root for name in names if (root,name) in self.panes}
        if surface: parents['P_QolCapture_00'] = panes['N_QolPreview_00']
        if hotel:
            for child,parent in (('N_TradeBox_00','N_Contents_00'),('N_Align_02','N_TradeBox_00'),
                                 ('P_TradeBoxBase_00','N_Align_02'),('T_TradeBoxNum_00','P_TradeBoxBase_00')):
                parents[child] = panes[parent]
            self.u.mem_write(panes['N_TradeBox_00']+0x30,struct.pack('<3f',90,-402,7))
            self.u.mem_write(panes['N_Align_02']+0x30,struct.pack('<3f',30,0,0))
            self.u.mem_write(panes['P_TradeBoxBase_00']+0x30,struct.pack('<3f',-60,0,0))
            self.u.mem_write(panes['P_TradeBoxBase_00']+0x50,struct.pack('<2f',366,100))
            self.u.mem_write(panes['T_TradeBoxNum_00']+0x100,b'12\0')
        for parent in (root,*panes.values()):
            children = [panes[name] for name,p in parents.items() if p==parent]
            for i,pane in enumerate(children):
                self.write(pane+0x18,parent)
                self.write(pane+8,children[i-1]+8 if i else parent+0x20)
                self.write(pane+16,children[i+1]+8 if i+1<len(children) else parent+0x20)
            self.write(parent+0x20,children[-1]+8 if children else parent+0x20)
            self.write(parent+0x28,children[0]+8 if children else parent+0x20)
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
    def draw_order(self, root, panes):
        # Real native recursive Pane drawing determines order/visibility;
        # only individual GPU pane submissions are recorded at the boundary.
        self.write(self.pane_methods+0xa0,self.base+0x27e140)
        self.write(self.pane_methods+0xa8,self.stub('pane_draw'))
        for pane in (root,*panes.values()):
            self.write(pane+0x5a,255,1); self.write(pane+0x5c,16,1)
        self.write(root+0x58,0xa3,1)
        self.drawn=[]
        self.invoke(self.base+0x27e140,root,0,0)
        return self.drawn
    @property
    def model_events(self): return [e for e in self.events if e[0]!='native_input']
    def confirm(self, page, resident=0):
        self.input(page,0,resident=resident)
        return self.input(page,0x10,resident=resident)
    def button(self, page, row, kind):
        event = self.alloc(0x20); self.write(event, kind, 4); self.write(event + 0x18, row)
        self.invoke(self.base + 0x375aa40, page, event)
    def frame(self, phone):
        self.write(self.entity_manager+0x288,0,4)
        self.invoke(self.base+0x2b0c720,phone)
        self.write(self.entity_manager+0x288,1,4)
        self.invoke(self.base+0x2b0e20c)  # Actual native deferred transform dispatch.
    def clear(self, layer=2, options=None):
        self.write(self.layer_context+0x3d0,self.layers[layer])
        self.hook(0x39392b0,self.layer_renderer,self.clear_command,self.layer_context,options or 0)
        # The actual native layer function copied/converted the color to its
        # stack. Continue at the GPU clear boundary with those exact bytes.
        color=self.alloc(); self.u.mem_write(color,self.layer_color)
        self.u.reg_write(UC_ARM64_REG_S0,0x3f000000)
        self.invoke(self.base+0x38025b0,self.clear_buffer,self.clear_command,0,3,color,7,self.alloc(),1)
    def native_parameter_update(self, protected=True):
        states=self.alloc(128)
        self.write(self.controller+0x30,2,4); self.write(self.controller+0x38,states)
        self.write(self.controller+0x338,1,1)
        call=self.base+0x32b9754
        delta=((self.hooks[self.base+0x32b9e08] if protected else self.base+0x32b9e08)-call)//4
        self.write(call,0x94000000|(delta&0x3ffffff),4)
        self.u.ctl_remove_cache(call,call+4)
        self.invoke(self.base+0x32b9510,self.controller)
        self.write(self.controller+0x338,0,1)
    def drawable_matrix(self, component=None, protected=False):
        self.u.reg_write(UC_ARM64_REG_X8,self.matrix)
        if protected: self.invoke(self.base+0x19283f0,component or self.component)
        else: self.invoke(self.base+0x6021a4,component or self.component)
        return bytes(self.u.mem_read(self.matrix,48))
    def input(self, page, held, resident=0, blocked=0, consumed=0, left=(0,0), right=(0,0), consumed_axes=0):
        snapshot = self.alloc(); rows = self.alloc(8 * 0x48)
        self.last_input_snapshot = snapshot
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
