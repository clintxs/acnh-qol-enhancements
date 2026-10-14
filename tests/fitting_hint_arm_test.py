"""Native fitting footer construction and compiled INI caption routing.

Runs original append/copy, message initialization/update/destruction at two
bases. Filesystem, type checks, glyph priority and final text setter are modeled;
this does not render a GPU frame or establish long-caption visual fit.
"""
import struct
from core_arm_test import Machine, RAW
from unicorn.arm64_const import *

KEY=b'fitting_room.add_to_bag'
PATH='content:/qol-enhancements/messages/fitting-room.ini'
class HintMachine(Machine):
    def __init__(self,base,content=None,locale=1):
        super().__init__(base,{})
        if content is not None: self.files[PATH]=content
        self.locale=locale;self.texts=[];self.text_events=[]
        self.owner=self.ram+0x1000;self.page=self.ram+0x30000
        self.footer=self.ram+0x31000;self.entries=self.ram+0x32000;self.pool=self.ram+0x33000
        self.app=self.ram+0x34000;self.global_=self.ram+0x35000;self.vtable=self.ram+0x36000
        self.safe=self.ram+0x37000;self.typefn=self.ram+0x38000;self.safe_fn=self.typefn+16
        self.guard=self.typefn+32;self.right_type=True
        self.write(base+0x4ab2230,self.global_);self.write(self.global_,self.app);self.write(self.app+0x58,self.footer)
        self.write(base+0x4ac7280,self.guard);self.write(self.guard,1,1)
        self.write(base+0x4ac7278,self.guard+16)
        self.write(self.footer,self.vtable);self.write(self.vtable,self.typefn)
        self.write(base+0x4ab2bc0,self.safe-16);self.write(self.safe+24,self.safe_fn)
        self.write(self.owner,base+0x4a10620);self.write(self.owner+0x210,self.page)
        self.write(self.owner+0x1f8,2,1)
        for i,button in enumerate((5,10,7)):
            obj=self.page+0x6000+i*0x1000;unit=obj+0x200
            self.write(self.page+0x570+i*8,obj);self.write(obj+0xa0,unit)
            self.write(unit+0x14,button,4);self.write(unit+0xbc,1,1)
        self.write(self.footer+0x1b0,2,4);self.write(self.footer+0x1f0,0xffffffff,4);self.write(self.footer+0x1f4,0xffffffff,4)
        self.write(self.footer+0x220,self.entries)
        self.ticket=False
        self.write(self.module.find('InitializeHintCaption','OrigRef'),base+0x1aea210)
        self.write(self.module.find('HintProviders'),self.module.find('BagCaption'))
        self.install_adapter()
    def service(self,u,pc,size,data):
        if not hasattr(self,'typefn'):
            return super().service(u,pc,size,data)
        result=None
        if pc==self.typefn: result=int(self.right_type)
        elif pc==self.safe_fn: result=0
        elif pc==self.base+0x31152b8: result=int(self.ticket)
        elif pc==self.base+0x3f04470:
            u.mem_write(self.reg(0),bytes(u.mem_read(self.reg(1),self.reg(2))));result=self.reg(0)
        elif pc==self.base+0x2de2de0: result=self.reg(0)
        elif pc==self.base+0x37cefc4: result=self.locale
        elif pc==self.typefn+0x100:
            assert self.read(self.reg(1)+8,4)==9
            assert self.read(self.reg(1))==self.safe
            self.text_events.append('prepare');result=0
        elif pc==self.typefn+0x110:
            assert self.text_events[-1]=='set'
            self.text_events.append('notify');result=0
        elif pc in (self.base+0x3c1b588,self.base+0x3c47b90):
            assert self.text_events[-1]=='prepare'
            self.text_events.append('set')
            self.texts.append(bytes(u.mem_read(self.reg(1),self.reg(2)*2)).decode('utf-16-le'));result=0
        if result is not None:
            u.reg_write(UC_ARM64_REG_X0,result);u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR));return
        super().service(u,pc,size,data)
    def invoke(self,pc,*args):
        for i,arg in enumerate(args):self.u.reg_write(UC_ARM64_REG_X0+i,arg)
        self.call(pc)
    def install_adapter(self):
        # Match the register save/callback/displaced-instruction contract.
        start=self.base+0x6100000;self.u.mem_map(start,4096);code=[0xd10403ff]
        def branch(src,dst,link=False):
            delta=(dst-src)//4;assert -(1<<25)<=delta<(1<<25)
            return (0x94000000 if link else 0x14000000)|(delta&0x3ffffff)
        for r in range(0,30,2):code.append(0xa9000000|((r//2)*2<<15)|((r+1)<<10)|(31<<5)|r)
        code.extend((0xf9007bfe,0x910003e0))
        code.append(branch(start+4*len(code),self.module.find('AppendBagFooterHint','Callback'),True))
        for r in range(0,30,2):code.append(0xa9400000|((r//2)*2<<15)|((r+1)<<10)|(31<<5)|r)
        code.extend((0xf9407bfe,0x910403ff,struct.unpack_from('<I',RAW,0x3114f28)[0]))
        code.append(branch(start+4*len(code),self.base+0x3114f2c))
        self.u.mem_write(start,struct.pack('<'+'I'*len(code),*code))
        self.write(self.base+0x3114f28,branch(self.base+0x3114f28,start),4)
    def reset(self,capacity=5):
        self.write(self.owner+0x1c4f4,0,4)
        self.write(self.footer+0x218,0,4);self.write(self.footer+0x21c,capacity,4)
        self.write(self.footer+0x228,self.pool);self.write(self.footer+0x239,0,1)
        for i in range(5):
            self.u.mem_write(self.pool+i*0x88,bytes(0x88))
            self.write(self.pool+i*0x88,self.pool+(i+1)*0x88 if i<4 else 0)
    def rows(self):
        return [self.read(self.entries+i*8) for i in range(self.read(self.footer+0x218,4))]
    def label(self,row):return self.string(self.read(row+0x18))
    def caption(self,key=KEY,group=False,bad_adapter=False,bad_prepare=False,bad_payload=False):
        msg=self.ram+0x40000;binding=self.ram+0x41000;adapter=binding+0x100;methods=adapter+0x100
        bank=self.ram+0x43000;token=bank+0x100
        self.u.mem_write(msg,bytes(0x400))
        for offset in (0x2e0,0x338):
            self.write(msg+offset,self.safe);self.write(msg+offset+8,msg+offset+20);self.write(msg+offset+16,64,4)
        self.write(bank,self.safe);self.write(bank+8,bank+0x20);self.u.mem_write(bank+0x20,b'LayoutMsg\0')
        self.write(token,self.safe);self.write(token+8,token+0x20);self.u.mem_write(token+0x20,key+b'\0')
        if key in (b'qol_pockets.drop_release',b'qol_pockets.hold_wear'):
            regs=self.ram+0x44000
            self.write(regs+16,bank);self.write(regs+24,token)
            self.invoke(self.module.find('ResolveStorageHintCaption','Callback'),regs)
            assert self.read(regs+16)==bank and self.read(regs+24)==token # Explicit INI override.
        self.write(binding+0x48,adapter);self.write(adapter+8,binding+0x900);self.write(adapter+0x18,methods)
        # Run REAL 03c7dd80: native notification must enqueue the dependent
        # binding used for background sizing. Only virtual engine calls are
        # stubs, so omitting NotifyTextChanged reproduces the 0.4.2 failure.
        interface=binding+0xa00;binding_vtable=interface+0x100
        event=binding+0xc00;queue=binding+0xd00
        self.write(adapter,interface);self.write(interface+0x18,0 if bad_prepare else self.typefn+0x100)
        if bad_payload: self.write(adapter+8,0)
        self.write(binding,binding_vtable);self.write(binding_vtable+0x20,self.typefn+0x110)
        self.write(binding+0x50,event);self.write(binding+0x58,0,2)
        self.u.mem_write(event,bytes(0x60));self.u.mem_write(queue,bytes(0x60))
        self.write(event+0x38,queue);self.write(queue+0x54,0x10,4)
        self.write(self.base+0x4ac0878,self.safe-16)
        self.text_events.clear()
        self.write(methods+0x5e0,self.base+(0 if bad_adapter else 0x3c47b90 if group else 0x3c1b588))
        self.invoke(self.module.find('InitializeHintCaption','Callback'),msg,binding,bank,token)
        if key in (KEY, b'qol_pockets.drop_release', b'qol_pockets.hold_wear'):
            assert self.read(msg+0xc,4)==12
            if not (bad_adapter or bad_prepare or bad_payload):
                assert self.text_events==['prepare','set','notify']
                assert self.read(queue+0x40)==event+0x10 and self.read(queue+0x50,4)==1
            self.invoke(self.base+0x1aea9c8,msg) # Must not attempt MSBT lookup.
        else: assert self.read(msg+0xc,4)==1
        if bad_adapter or bad_prepare or bad_payload or key not in (KEY,b'qol_pockets.drop_release',b'qol_pockets.hold_wear'):
            assert self.text_events==[] and self.read(queue+0x50,4)==0
        # Native cleanup sees zero allocation ownership; mod owns no parser buffers.
        self.invoke(self.base+0x1ae9eb8,msg)

for base in (0x7100000000,0x7200000000):
    m=HintMachine(base)
    for ticket in (False,True):
        m.ticket=ticket;m.reset()
        m.invoke(base+0x3114c3c,m.owner,0 if ticket else 0xffffffff)
        rows=m.rows();labels=[m.label(r) for r in rows]
        assert labels.count(KEY.decode())==1 and len(rows)==(5 if ticket else 4),labels
        bag=next(r for r in rows if m.label(r)==KEY.decode())
        assert m.read(bag,4)==7 and m.read(bag+0x68,1)==2
        ticket_input=m.read(m.read(m.page+0x580)+0xa0)
        assert m.read(ticket_input+0x14,4)==6
        if ticket:
            ticket_hint=rows[-1]
            assert m.read(ticket_hint,4)==6 and m.read(ticket_hint+0x68,1)==2
        # Same native hint mode skips the builder and does not append a duplicate.
        m.invoke(base+0x3114c3c,m.owner,0 if ticket else 0xffffffff)
        assert m.rows()==rows
    m.ticket=False
    for capacity in (0,1,2,3,4):
        m.reset(capacity);m.invoke(base+0x3114c3c,m.owner,0xffffffff)
        assert KEY.decode() not in [m.label(r) for r in m.rows()]
    m.reset();m.right_type=False;m.invoke(base+0x3114c3c,m.owner,0xffffffff);assert not m.rows()
    print(f'PASS: native fitting footer, normal/ticket modes, Y/player, duplicate/capacity/type guards at {base:#x}')
    cases=[(None,1,'Bag'),(b'[USen]\nfitting_room.add_to_bag=Bag it\n',9,'Bag it'),
           ('[USen]\nfitting_room.add_to_bag=Bag it\n[EUfr]\nfitting_room.add_to_bag=Ajouter au panier\n'.encode(),7,'Ajouter au panier'),
           (b'[USen]\nfitting_room.add_to_bag=\xff\n',1,'Bag'),
           (b'[USen]\nfitting_room.add_to_bag=One\nfitting_room.add_to_bag=Two\n',1,'Bag'),
           (b'x'*16385,1,'Bag')]
    for content,locale,expected in cases:
        m=HintMachine(base,content,locale)
        m.caption();m.caption(group=True)
        assert m.texts==[expected,expected],m.texts
        assert m.opened==[PATH] and len(m.closed)==int(content is not None)
        for key in (b'1201',b'qol_pockets.put_in_storage',KEY+b'_other'):
            m.caption(key=key);assert m.texts==[expected,expected]
        m.caption(bad_adapter=True);assert m.texts==[expected,expected]
    print(f'PASS: compiled UTF-8 catalog, locale/English fallbacks, malformed/oversize files, once-only I/O, ordinary keys and native message lifecycle at {base:#x}')

# Decode every actual shipped locale through the compiled parser and setter.
from pathlib import Path
import configparser
content=(Path(__file__).resolve().parents[1]/'modules/Fitting Room/romfs/qol-enhancements/messages/fitting-room.ini').read_bytes()
ini=configparser.ConfigParser(interpolation=None);ini.read_string(content.decode('utf-8'))
locales=('JPja','USen','USes','USfr','USpt','EUen','EUes','EUfr','EUde','EUit','EUpt','EUnl','EUru','KRko','CNzh','TWzh')
m=HintMachine(0x7100000000,content)
for index,locale in enumerate(locales):
    m.locale=index;m.caption()
    assert m.texts[-1]==ini[locale]['fitting_room.add_to_bag'],locale
assert m.opened==[PATH]
print('PASS: all 16 shipped locale captions through compiled UTF-8 parser and native-message lifecycle; one file read')

# The shared hook must work with Pockets alone and with both providers, without
# attempting to load Fitting Room text for a Pockets token (or vice versa).
for both in (False, True):
    m=HintMachine(0x7100000000)
    providers=m.module.find('HintProviders')
    m.write(providers,m.module.find('BagCaption') if both else 0)
    m.write(providers+8,m.module.find('UseItemCaption'))
    m.write(providers+16,m.module.find('DropItemCaption'))
    path='content:/qol-enhancements/messages/pockets.ini'
    m.files[path]=b'[USen]\nqol_pockets.drop_release=Drop / Release\nqol_pockets.hold_wear=Equip\n'
    m.caption(key=b'qol_pockets.drop_release'); m.caption(key=b'qol_pockets.hold_wear',group=True)
    assert m.texts==['Drop / Release','Equip'] and m.opened==[path]
    if both:
        m.caption(); assert m.texts[-1]=='Bag' and m.opened==[path,PATH]
    m.caption(key=b'1201')
print('PASS: shared caption hook, Pockets-only and combined providers, independent once-only files')
