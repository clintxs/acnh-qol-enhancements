"""Run compiled bag checkout with the actual native checkout body.

Save APIs, clothing application, records, and engine singletons are modeled.
The native payment/delivery loops and their arguments execute unchanged.
"""
from runtime_arm_test import Machine
from unicorn.arm64_const import *
from unicorn import UC_HOOK_MEM_INVALID
import struct

class PurchaseMachine(Machine):
    instruction_budget=5000000
    def __init__(self,base=0):
        self.money=10000;self.paid=[];self.delivered=[];self.records=[];self.gross=[];self.tickets=[]
        self.native_commit=False;self.native_collects=0
        super().__init__(base)
        self.u.hook_add(UC_HOOK_MEM_INVALID, lambda u,a,p,n,v,d: print("invalid",hex(u.reg_read(UC_ARM64_REG_PC)),hex(p)) or False)
        self.olditems=[struct.pack('<HBBI',120,0,0,77)]
        self.write(self.mod.find('8CheckoutEE7OrigRef'),base+0x311d3b0)
        # Native checkout's event result and purchase-record context.
        self.event=self.ram+0x80000;self.event_root=self.event+0x1000
        self.write(base+0x4ab2230,self.event_root);self.write(self.event_root,self.event_root+0x100)
        self.write(self.event_root+0x128,self.event)
        self.write(base+0x4ab43c0,self.event_root+0x200) # null item metadata manager, optional record pricing
        self.write(base+0x4ab49a0,self.event_root+0x300)
        self.write(base+0x4ab37e0,base+0x5486008)
        self.write(self.event+0x4e0,0,1)
        self.write(self.event+0x4d8,self.event_root+0x400)
        self.write(self.event_root+0x418,self.stub) # getter if needed
        # Guard null-bag original trampolines are modeled in base class.
    def finish(self,result=0):
        self.u.reg_write(UC_ARM64_REG_X0,result)
        self.u.reg_write(UC_ARM64_REG_PC,self.u.reg_read(UC_ARM64_REG_LR))
    def native_list(self,p,items):
        assert self.read(p,4)==0 and self.read(p+4,4)==10
        for i,item in enumerate(items):
            free=self.read(p+0x10);assert free
            self.write(p+0x10,self.read(free))
            self.u.mem_write(free,item);self.write(self.read(p+8)+i*8,free)
        self.write(p,len(items),4)
    def outfit(self):
        result=[]
        for i in range(10):
            index=self.read(self.owner+0x1c370+i*12,4)
            if index<self.read(self.owner+0x1c338,2):result.append(bytes(self.u.mem_read(self.item+index*8,8)))
        return result
    def service(self,u,pc,size,data):
        off=pc-self.base
        service=self.services.get(pc)
        if off==0x311ca40:
            u.reg_write(UC_ARM64_REG_PC,self.mod.find('7Collect8Callback'));return
        if service==('original','Collect'):
            self.native_collects+=1
            self.native_list(self.reg(1),self.olditems if self.reg(2)&1 and self.outfit() else self.outfit())
            self.finish();return
        if service==('original','Price'):
            result=sum(100+struct.unpack_from('<H',i)[0] for n,i in enumerate(self.outfit()) if not self.read(self.owner+0x1c374+n*12,1))
            self.finish(result);return
        if service==('original','GrossPrice'):
            self.finish(sum(100+struct.unpack_from('<H',i)[0] for i in self.outfit()));return
        if off==0x257abf0:self.finish(self.money);return
        if off==0x257b340:
            amount=self.reg(1);assert amount<=self.money
            self.money-=amount;self.paid.append(amount);self.finish();return
        if off==0x25772d0:self.tickets.append(self.reg(2));self.finish();return
        if off in (0x257d630,0x25738b0):
            where='storage' if off==0x257d630 else 'pockets'
            free=getattr(self,where)
            if free:
                setattr(self,where,free-1)
                self.delivered.append((where,bytes(u.mem_read(self.reg(1),8))))
            self.finish(int(free>0));return
        if off==0x311dec0:
            u.reg_write(UC_ARM64_REG_PC,self.mod.find('10GrossPrice8Callback'));return
        if off==0x3f04450:
            u.mem_write(self.reg(0),bytes([self.reg(1)&255])*self.reg(2));self.finish(self.reg(0));return
        if off==0x267d4ac:self.finish();return
        if off==0x24d74cc:
            u.reg_write(UC_ARM64_REG_PC,self.mod.find('14PurchaseRecord8Callback'));return
        if service==('original','PurchaseRecord'):
            p=self.reg(1);count=self.read(p+0xc,4)
            assert count<=16, "native record must never overrun its fixed payload"
            self.records.append((self.read(p+4,4),count,bytes(u.mem_read(p+0x10,count*8))))
            self.finish();return
        if off in (0x269b044,0x269a9f0):self.gross.append(self.reg(2));self.finish();return
        if off==0x1a67430:self.gross.append(self.reg(1));self.finish();return
        if off in (0x311c1a0,0x2443cdc,0x24d6bd4,0xadc3cc,0x2501530,0x5bd550,0x1a5b4b0,0x1a5e0b0):self.finish();return
        if off==0x24d79cc:self.finish(0xffffffffffffffff);return
        if off==0x2569430:self.finish(0);return
        super().service(u,pc,size,data)
    def commit(self,wear=0):return self.call(self.mod.find('8Checkout8Callback'),self.owner,wear)

for base in (0,0x7100000000):
    for wear in (0,1):
        for storage,pockets,expected_result in ((100,0,2),(0,40,1),(1,40,3)):
            m=PurchaseMachine(base);m.storage=storage;m.pockets=pockets
            m.write(m.owner+0x1c370,0,4) # tried-on item 117
            m.toggle() # bag same item: charged once
            m.focus(1);m.toggle();m.focus(2);m.toggle()
            wanted=(m.olditems if wear else [bytes(m.u.mem_read(m.item,8))])+[bytes(m.u.mem_read(m.item+8,8)),bytes(m.u.mem_read(m.item+16,8))]
            m.commit(wear)
            assert m.paid==[217+218+219],m.paid
            assert [i for _,i in m.delivered]==wanted,m.delivered
            assert m.read(m.event+0x12,1)==expected_result
            assert m.size()==0
            assert m.records==[(654,3,bytes(m.u.mem_read(m.item,24)))],m.records
            assert m.gross==[654]*3,m.gross
            m.commit(wear);assert len(m.paid)==1 and len(m.delivered)==3
    # Bag-only checkout, then a duplicate delivery guard.
    m=PurchaseMachine(base);m.toggle();m.focus(1);m.toggle();m.commit()
    assert m.paid==[435] and len(m.delivered)==2
    # Tailor ticket covers only the tried-on item; bag extra remains full price.
    m=PurchaseMachine(base);m.write(m.owner+0x1c370,0,4);m.write(m.owner+0x1c374,1,1)
    m.toggle();m.focus(1);m.toggle();m.commit()
    assert m.paid==[218] and m.tickets==[1] and m.gross==[435]*3
    # Capacity and funds changes between browsing and committing cause no payment.
    for failure in ('space','money','stock'):
        m=PurchaseMachine(base);m.toggle()
        if failure=='space':m.storage=m.pockets=0
        elif failure=='money':m.money=1
        else:m.write(m.item,555,2)
        m.commit();assert not m.paid and not m.delivered and m.size()==1
    # Native confirmation callback emits actual native error descriptors or
    # queues the send/wear command. Cancellation at the first prompt is inert.
    for scenario in ('send','wear','space','money','cancel'):
        m=PurchaseMachine(base);m.toggle()
        m.write(m.mod.find('6DialogEE7OrigRef'),base+0x31141cc)
        output=m.ram+0xb0000
        m.write(m.owner+0x1c4f0,4 if scenario=='cancel' else 5,4)
        if scenario=='space':m.storage=m.pockets=0
        if scenario=='money':m.money=1
        result=m.call(m.mod.find('6Dialog8Callback'),m.owner,output,int(scenario=='wear'))
        if scenario in ('send','wear'):
            assert result==0 and m.read(m.owner+0x1c318,4)==(5 if scenario=='wear' else 4)
        elif scenario in ('space','money'):
            assert result==1 and m.read(m.owner+0x1c4f0,4)==(3 if scenario=='space' else 1)
            assert m.read(output+0x28)==m.owner+0x1c4d0
        else:assert result==0 and m.read(m.owner+0x1c318,4)==0xffffffff
        assert not m.paid and not m.delivered
    # Confirming bag-only checkout skips the wear question but still runs the
    # final preflight. It queues native send; payment waits for native dispatch.
    for scenario in ('bag-only','money','space','stock','cancel','empty-bag'):
        m=PurchaseMachine(base)
        if scenario!='empty-bag':m.toggle();m.focus(1);m.toggle()
        m.write(m.mod.find('6DialogEE7OrigRef'),base+0x31141cc)
        m.write(m.owner+0x1c4f0,4,4)
        output=m.ram+0xb0000
        if scenario=='money':m.money=1
        if scenario=='space':m.storage=m.pockets=0
        if scenario=='stock':m.write(m.item,555,2)
        result=m.call(m.mod.find('6Dialog8Callback'),m.owner,output,0 if scenario=='cancel' else 1)
        assert not m.paid and not m.delivered
        if scenario=='bag-only':
            assert result==0 and m.read(m.owner+0x1c318,4)==4
            m.commit(0)
            assert m.paid==[435]
            assert [item for _,item in m.delivered]==[bytes(m.u.mem_read(m.item+i*8,8)) for i in range(2)]
        elif scenario in ('space','money'):
            assert result==1 and m.read(m.owner+0x1c4f0,4)==(3 if scenario=='space' else 1)
            assert m.read(m.owner+0x1c318,4)==0xffffffff
        elif scenario=='empty-bag':
            assert result==1 and m.read(m.owner+0x1c4f0,4)==5
            assert m.read(m.owner+0x1c318,4)==0xffffffff
        else:
            assert result==0 and m.read(m.owner+0x1c318,4)==0xffffffff
    # Every outfit category keeps the normal wear question, even if the tried-on
    # item is also bagged. Unresolved indices are not treated as empty slots.
    for slot in range(10):
        for index in (0,999):
            m=PurchaseMachine(base);m.toggle()
            m.write(m.owner+0x1c370+slot*12,index,4)
            m.write(m.mod.find('6DialogEE7OrigRef'),base+0x31141cc)
            m.write(m.owner+0x1c4f0,4,4)
            result=m.call(m.mod.find('6Dialog8Callback'),m.owner,m.ram+0xb0000,1)
            assert result==1 and m.read(m.owner+0x1c4f0,4)==5
            assert m.read(m.owner+0x1c318,4)==0xffffffff
            assert not m.paid and not m.delivered
    # Cross both native boundaries, then exercise the bag's full 256 selections.
    for count in (11,16,17,40,100,256):
        for wear in (0,1):
            m=PurchaseMachine(base);m.money=1000000;m.storage=1000;m.pockets=20
            stock_count=count+2
            m.write(m.owner+0x1c338,stock_count,2);m.write(m.owner+0x1c358,stock_count,4);m.write(m.owner+0x1c368,stock_count,4)
            for i in range(stock_count):
                m.write(m.indices+i*2,i,2);m.write(m.stock+i*8,m.item+i*8)
                m.u.mem_write(m.item+i*8,struct.pack('<HBBI',117,0,0,i*7))
            for i in range(count):m.focus(i);m.toggle()
            assert m.size()==count
            # Reproduce the reported failure: add a tried-on outfit after filling the bag.
            m.write(m.owner+0x1c370,count,4);m.write(m.owner+0x1c37c,count+1,4)
            m.commit(wear)
            expected=(m.olditems if wear else [bytes(m.u.mem_read(m.item+count*8,8)),bytes(m.u.mem_read(m.item+(count+1)*8,8))])+[bytes(m.u.mem_read(m.item+i*8,8)) for i in range(count)]
            assert m.paid==[217*(count+2)] and [i for _,i in m.delivered]==expected
            assert m.records[0][1]==min(count+2,16)
            assert m.records[0][0]==m.paid[0] and m.gross==m.paid*3
            assert m.size()==0
    # Eleven purchases split across storage and pockets, then real capacity rejection.
    m=PurchaseMachine(base);m.storage=9;m.pockets=2
    m.write(m.owner+0x1c338,12,2);m.write(m.owner+0x1c358,12,4);m.write(m.owner+0x1c368,12,4)
    for i in range(12):
        m.write(m.indices+i*2,i,2);m.write(m.stock+i*8,m.item+i*8)
        m.u.mem_write(m.item+i*8,struct.pack('<HBBI',117,0,0,i*7))
    for i in range(12):m.focus(i);m.toggle()
    assert m.size()==11 and m.read(m.owner+0x1c4f0,4)==3
    assert m.read(m.owner+0x1f0,4)==4 # Native error is requested immediately on Y.
    m.commit();assert [w for w,_ in m.delivered]==['storage']*9+['pockets']*2
    # Invalid stock cancels confirmation without falsely reporting full storage.
    m=PurchaseMachine(base);m.toggle();m.write(m.item,555,2)
    m.write(m.owner+0x1c4f0,5,4)
    output=m.ram+0xb0000
    result=m.call(m.mod.find('6Dialog8Callback'),m.owner,output,0)
    assert result==0 and m.read(m.owner+0x1c4f0,4)==5 and not m.paid
print('PASS: actual native checkout loops at two bases; exact variants, outfit overlap, wear/send, storage/pockets/mixed, tickets, records, statistics, repeat guard, preflight rejection')
