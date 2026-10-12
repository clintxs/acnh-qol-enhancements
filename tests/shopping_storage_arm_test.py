"""Compiled storage routing and original 3.0.3 actor dispatch/query/action bodies.

Synthetic residents only. Native dispatchers supply the current subflow name,
initialize reusable commands, and execute BellCountDown synchronously. Engine
parameter binding/cash services are modeled; native savings encryption runs.
"""
import struct
from shopping_card_arm_test import CardMachine, RAW
from unicorn.arm64_const import *

FLOW = 'SNPC_rco_10_Common_ExpandHouseStorage'
PRICES = (500000, 700000, 900000, 1200000, 1500000, 1800000)

class StorageMachine(CardMachine):
    def __init__(self, base):
        super().__init__(base)
        self.write(self.info + 0x38, 0, 4)  # Resident Services, no shop routing
        # Test real native dispatch, not an imitation of command timing.
        for offset, name in ((0x60f4f0,'StorageCommand'), (0x60fb10,'StorageAffordability'),
                             (0xc97260,'StoragePayment'), (0x60f340,'StorageEventDestroyed')):
            self.u.mem_write(base+offset, RAW[offset:offset+20])
            self.callbacks[offset] = self.module.find('shopping',name,'Callback')
            self.write(self.module.find('shopping',name,'OrigRef'), base+offset)
        del self.services[base+0x5bd550]  # run the actual native name hash
        self.area = self.ram+0x1b0000
        self.action = self.area+0x1000; self.check = self.area+0x2000
        self.args = self.area+0x3000; self.chart = self.area+0x3100
        self.name = self.area+0x3200; self.command_text = self.area+0x3300
        self.flow_name = self.area+0x3400; self.flow_text = self.area+0x3500
        self.res_name = self.area+0x3600; self.table = self.area+0x4000
        self.query_table = self.area+0x4100; self.methods = self.area+0x4200
        self.executor = self.area+0x4300; self.context = self.area+0x4400
        self.noop = base+0x9d9494
        self.write(self.methods+0x18,self.noop)
        for name,text in ((self.name,self.command_text),(self.flow_name,self.flow_text)):
            self.write(name,self.methods); self.write(name+8,text)
        self.u.mem_write(self.flow_text,b'SNPC_rco_00_Common\0')  # unrelated ROOT is intentional
        self.write(self.chart,self.res_name)
        self.write(self.args+0x10,self.chart); self.write(self.args+0x38,self.chart)
        self.write(self.event+0x288,self.executor); self.write(self.executor+8,self.context)
        self.write(self.context+0xb8,1,4)  # skip debug formatting
        for action,table in ((self.action,self.table),(self.check,self.query_table)):
            self.write(action,table);self.write(action+8,self.event)
            for offset in (0x28,0x38):self.write(table+offset,self.noop)
        self.write(self.table+0x30,base+0x9dc7a4)  # actual immediate-action discriminator
        self.write(self.table+0x58,self.callbacks[0xc97260])
        self.write(self.query_table+0x30,base+0xf7ccfc)
        self.write(self.event+0x2c8,1,4);self.write(self.event+0x2d0,self.area+0x4500)
        self.write(self.area+0x4500,self.action)
        self.write(self.event+0x2d8,1,4);self.write(self.event+0x2e0,self.area+0x4600)
        self.write(self.area+0x4600,self.check)
        # The native event destructor has engine-owned allocations; model only
        # its return while executing the compiled pre-teardown cleanup hook.
        self.stub = base+0x1500; self.services[self.stub] = ('destroy_storage',None)
        self.write(self.module.find('StorageEventDestroyed','OrigRef'),self.stub)
        self.storage_payments = 0
    def service(self,u,pc,size,data):
        if getattr(self,'stub',None)==pc: self.finish(); return
        if getattr(self,'base',0)+0xc97260==pc:
            self.storage_payments+=1
        super().service(u,pc,size,data)
    def command(self,name,flow=FLOW):
        self.u.mem_write(self.command_text,name.encode()+b'\0')
        self.u.mem_write(self.res_name,struct.pack('<H',len(flow))+flow.encode()+b'\0')
        h=self.invoke(self.base+0x5bd550,self.command_text,0)
        self.write(self.action+0x10,h,4);self.write(self.check+0x10,h,4)
    def funds(self,player,savings,wallet,bags):
        self.write(self.event+0x2a8,player,1)
        current=self.balance(player)
        if current:self.invoke(self.base+0x257fb30,self.player,current)
        self.deposit(savings,player);self.cash[player]={'wallet':wallet,'bags':list(bags)}
    def total(self,player):
        return self.balance(player)+self.wallet+sum(self.bags)
    def affordable(self,price,flow=FLOW):
        self.command('HasBellCount',flow);self.write(self.check+0xd0,price,4)
        return self.invoke(self.callbacks[0x60fb10],self.event,self.name,self.args,self.flow_name)
    def dispatch(self,name='BellCountDown',price=0,flow=FLOW,subtract=True):
        self.command(name,flow);self.write(self.action+0xd8,price,4)
        self.write(self.action+0x100,int(subtract),1)
        # Keep harmless display actions from invoking the native debit body.
        self.write(self.table+0x58,self.callbacks[0xc97260] if name=='BellCountDown' else self.noop)
        self.invoke(self.callbacks[0x60f4f0],self.event,self.name,self.args,self.flow_name,self.area+0x4700)
    def visible(self,player):
        slot=self.module.find('storageSessions')+player*16
        return self.read(slot+8,1)

for base in (0,0x7100000000):
    m=StorageMachine(base)
    for player in range(8):
        for price in PRICES:
            for total in (price-1,price,price+77777):
                m.funds(player,total,0,[])
                m.dispatch('UIMoneyAppear')
                assert m.affordable(price)==int(total<price)
                before=m.total(player)
                # Failed preflight/cancel never dispatches a charge.
                if total>=price:
                    m.dispatch(price=price)
                    assert m.total(player)==before-price
                else:assert m.total(player)==before
                assert m.visible(player)
                m.dispatch('UIMoneyDisappear');assert not m.visible(player)
            # Same price, but savings alone cannot pay: preserve wallet first.
            m.funds(player,price-200000,99000,[99000,99000,99000])
            m.dispatch('UIMoneyAppear')
            assert m.affordable(price)==0
            before=m.total(player);m.dispatch(price=price)
            assert m.balance(player)==0
            assert m.wallet_debits[-1]==(player,299000)
            assert m.cash_refunds[-1]==(player,99000)
            assert m.total(player)==before-price
            m.dispatch('UIMoneyDisappear')
            m.dispatch('OtherAction');assert not m.visible(player)
        m.funds(player,2000000,77777,[])
        assert m.affordable(1800000,'SNPC_rco_10_Common_MoveHouse')==1
        assert m.affordable(1800000,FLOW+'_Other')==1
        assert m.affordable(1800000)==0
        # Scope has ended after the query; generic callers still see cash only.
        assert m.route(0x257abf0,0xf7cd18,m.player)==77777
        m.dispatch('UIMoneyAppear');assert m.visible(player)
        m.dispatch('UIMoneyDisappear');assert not m.visible(player)
        before=m.balance(player)
        m.dispatch(price=1000,flow='SNPC_rco_10_Common_MoveHouse')
        assert m.balance(player)==before and m.wallet==76777
        m.dispatch(price=1000,subtract=False)
        assert m.balance(player)==before and m.wallet==77777
        m.dispatch('UIMoneyAppear');assert m.visible(player)
        m.dispatch('OtherAction',flow='SNPC_rco_00_Common');assert not m.visible(player)
        m.dispatch('UIMoneyAppear');assert m.visible(player)
        m.invoke(m.callbacks[0x60f340],m.event);assert not m.visible(player)
    # Invalid residents fall back to the native query without card money.
    m.write(m.event+0x2a8,255,1);assert m.affordable(1800000)==1
    print('PASS: storage native dispatch timing, all six prices/eight residents, mixed funds, one-short, cancellation, scope cleanup and other fees; base',hex(base))
