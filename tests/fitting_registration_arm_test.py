"""Production boot/activation -> installed fitting input, lifecycle and caption.

The native installation, original query/trampoline and UI services are modeled.
No policy or provider is assigned to private state after production activation.
Inherited fitting tests separately execute the real input/checkout/animation ARM.
"""
from runtime_arm_test import Machine as Runtime
from core_arm_test import Machine as Core, DEFAULT
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_PC, UC_ARM64_REG_LR

PATH='content:/qol-enhancements/messages/fitting-room.ini'
TARGETS={0x31187d0:'Begin',0x3114484:'Destroy',0x3114c3c:'Focus',0x3115920:'Row',
         0x311e180:'Price',0x311a538:'Play',0x311d3b0:'Checkout',0x311ca40:'Collect',
         0x31141cc:'Dialog',0x311dec0:'GrossPrice',0x24d74cc:'PurchaseRecord'}
class Registered(Runtime):
    string=Core.string
    alias_write=Core.alias_write
    def __init__(self,base,enabled):
        self.logs=[];self.hooks={};self.native_input_calls=0;self.quotes=0;self.space_reads=0
        self.files={'content:/qol-enhancements/config/core.ini':b'log_debug=true\n'}
        if enabled:self.files['content:/qol-enhancements/config/fitting-room.ini']=DEFAULT
        self.files[PATH]=b'[USen]\nfitting_room.add_to_bag=Wardrobe\n'
        self.handles={};self.opened=[];self.closed=[]
        self.open_error=self.read_error=self.size_error=False;self.custom_fault=None
        super().__init__(base)
        self.module=self.mod
        self.write(base+0x4a10398,base+0x3c58d24)
        self.write(base+0x49cd3f8,base+0x3c58d24)
        self.alias=base+0x20000000;self.u.mem_map(self.alias,0x4725000)
        storage=self.mod.find('patch','s_Storage');self.write(storage,base);self.write(storage+8,self.alias)
        for name in ('armDCacheFlush','armICacheInvalidate'):self.services[self.mod.find(name)]=('cache',name)
        self.u.hook_add(UC_HOOK_MEM_WRITE,self.alias_write,begin=self.alias,end=self.alias+0x4725000-1)
        self.call(self.mod.find('exl_main'));self.call(self.mod.find('AfterContentMount','Callback'))
        assert not self.handles and any('BOOT_OK' in text for text in self.logs),(self.handles,self.logs)
    def finish(self,result=0):
        self.u.reg_write(UC_ARM64_REG_X0,result)
        self.u.reg_write(UC_ARM64_REG_PC,self.u.reg_read(UC_ARM64_REG_LR))
    def service(self,u,pc,size,data):
        entry=self.services.get(pc)
        if entry and entry[0]=='install':
            target=self.reg(0);callback=self.reg(1)
            assert target not in self.hooks
            self.hooks[target]=callback;u.mem_write(target,b'\xff'*20)
            name=TARGETS.get(target-self.base)
            originals=[address for address,value in self.services.items() if value==('original',name)] if name else []
            assert not name or len(originals)==1,(name,originals)
            self.finish(originals[0] if originals else self.stub);return
        if pc in self.hooks and pc-self.base in TARGETS:
            u.reg_write(UC_ARM64_REG_PC,self.hooks[pc]);return
        if pc==self.base+0x3c58d24:
            self.native_input_calls+=1;self.finish();return
        if pc==self.base+0x37cefc4:self.finish(1);return
        if pc-self.base in (0x257c230,0x25731d0):self.space_reads+=1
        if entry==('original','Price') and self.reg(0)!=self.owner+0x1c330:
            item=self.read(self.read(self.reg(0)+0x10));self.quotes+=1
            self.finish(100+self.read(item,2));return
        if entry and (entry[0] in ('initialize','cache','log') or
                      entry[0]=='import' and any(name in entry[1] for name in ('OpenFile','GetFileSize','ReadFile','CloseFile'))):
            Core.service(self,u,pc,size,data);return
        Runtime.service(self,u,pc,size,data)
    def press(self,buttons):
        self.write(self.rows,buttons,4);self.write(self.rows+0x3c,0,4)
        self.call(self.read(self.base+0x4a10398),self.page,self.snapshot,0)
    def caption(self):
        pointer=self.read(self.mod.find('HintProviders'));assert pointer
        text=self.call(pointer);result=[]
        while True:
            unit=self.read(text+2*len(result),2)
            if not unit:return ''.join(chr(v) for v in result)
            result.append(unit);assert len(result)<128

for base in (0x7100000000,0x7200000000):
    for enabled in (False,True):
        m=Registered(base,enabled)
        fitting_input=m.read(base+0x4a10398);choice_input=m.read(base+0x49cd3f8)
        assert (fitting_input!=base+0x3c58d24)==enabled
        assert (choice_input!=base+0x3c58d24)==enabled
        if not enabled:
            assert m.read(m.mod.find('HintProviders'))==0
            m.press(0);m.press(0x80);assert m.size()==0 and m.native_input_calls==2
            continue
        assert fitting_input==m.mod.find('able_bag','L5InputE')
        assert choice_input==m.mod.find('L15WearChoiceInput')
        m.call(base+0x31187d0,m.owner);m.call(base+0x3114c3c,m.owner,0)
        m.press(0x80);assert m.size()==0
        m.press(0);m.press(0x80);assert m.size()==1
        assert bytes(m.u.mem_read(m.bag,8))==bytes(m.u.mem_read(m.item,8))
        assert m.read(m.rows+0x3c,4)&0x80 and m.native_input_calls==3
        m.press(0x80);assert m.size()==1 and m.native_input_calls==4
        queries=(m.quotes,m.space_reads);m.storage=m.pockets=0
        m.press(0);m.press(0x80);assert m.size()==0 and (m.quotes,m.space_reads)==queries
        m.press(0);m.press(0x80);assert m.size()==0
        assert m.read(m.owner+0x1c4f0,4)==3 and m.read(m.owner+0x1f0,4)==4
        assert m.caption()=='Wardrobe' and m.caption()=='Wardrobe' and m.opened.count(PATH)==1
        m.call(base+0x3114484,m.owner);assert m.read(m.session)==0 and m.size()==0
        before=m.native_input_calls;m.press(0);m.press(0x80)
        assert m.size()==0 and m.native_input_calls==before+2
print('PASS: production boot/activation, installed fitting/choice vtables, lifecycle callbacks, fresh Y/exact item, native-query/removal/error order, once-only original input, owned captions and disabled behavior at two load bases')
