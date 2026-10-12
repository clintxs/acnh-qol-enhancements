"""Compiled storage routing and original 3.0.3 actor dispatch/query/action bodies.

Synthetic residents only. Native dispatchers supply the current subflow name,
initialize reusable commands, and execute BellCountDown synchronously. Engine
parameter binding/cash services are modeled; native savings encryption runs.
"""
import struct
from pathlib import Path
from shopping_card_arm_test import CardMachine, RAW
from unicorn.arm64_const import *
from unicorn import UC_HOOK_CODE

FLOW = 'SNPC_rco_10_Common_ExpandHouseStorage'
PRICES = (500000, 700000, 900000, 1200000, 1500000, 1800000)
REGISTRY = {'HasBellCount':0x4880370, 'BellCountDown':0x487d450,
            'UIMoneyAppear':0x487d8b0}

# Read the real actor command tables. The decompiler deliberately strips the
# EventFlowAction/EventFlowQuery prefixes, so its display names cannot model
# what these native dispatchers receive.
RESOURCE = (Path(__file__).resolve().parents[2] / 'vanilla-3.0.3/EventFlow' /
            (FLOW + '.bfevfl')).read_bytes()
def resource_u64(offset): return struct.unpack_from('<Q', RESOURCE, offset)[0]
def resource_string(offset):
    size = struct.unpack_from('<H', RESOURCE, offset)[0]
    return RESOURCE[offset+2:offset+2+size].decode()
flowchart = resource_u64(resource_u64(0x28))
assert RESOURCE[flowchart:flowchart+4] == b'EVFL'
assert resource_string(resource_u64(flowchart+0x20)) == FLOW
NATIVE_COMMANDS = {}
ACTOR_COMMANDS = {}
actors = resource_u64(flowchart+0x28)
for index in range(struct.unpack_from('<H', RESOURCE, flowchart+0x10)[0]):
    actor = actors+index*0x38
    for table_offset,count_offset,prefix in ((0x18,0x30,'EventFlowAction'),
                                             (0x20,0x32,'EventFlowQuery')):
        table = resource_u64(actor+table_offset)
        for command in range(struct.unpack_from('<H', RESOURCE, actor+count_offset)[0]):
            name = resource_string(resource_u64(table+command*8))
            assert name.startswith(prefix)
            ACTOR_COMMANDS[index, table_offset == 0x20, command] = name
            if resource_string(resource_u64(actor)) == 'EventFlowSystemActor':
                NATIVE_COMMANDS[name[len(prefix):]] = name
for name in ('BellCountDown','HasBellCount','UIMoneyAppear','UIMoneyDisappear'):
    assert name in NATIVE_COMMANDS
RESOURCE_EVENTS = {}
events = resource_u64(flowchart+0x30)
for index in range(struct.unpack_from('<H', RESOURCE, flowchart+0x16)[0]):
    event = events+index*0x28
    kind = RESOURCE[event+8]
    if kind not in (0,1): continue
    actor,command = struct.unpack_from('<HH', RESOURCE, event+0xc)
    name = ACTOR_COMMANDS[actor, kind == 1, command]
    RESOURCE_EVENTS.setdefault(name, index)


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
        self.evfl_context = self.area+0x4800; self.nodes = self.area+0x4900
        self.flow_object = self.area+0x4a00; self.actor_binding = self.area+0x4b00
        self.command_binding = self.area+0x4c00
        self.resource = self.area+0x7000
        # Relocate the complete ORIGINAL BFEVFL, keeping its event names,
        # parameter records, events and owning flowchart distinct.
        relocated = bytearray(RESOURCE)
        relt = struct.unpack_from('<I', RESOURCE, 0x18)[0]
        assert RESOURCE[relt:relt+4] == b'RELT'
        assert struct.unpack_from('<I', RESOURCE, relt+8)[0] == 1
        for entry in range(struct.unpack_from('<I', RESOURCE, relt+0x24)[0]):
            offset,bits = struct.unpack_from('<II', RESOURCE, relt+0x28+entry*8)
            for bit in range(32):
                if bits & (1 << bit):
                    pointer = resource_u64(offset+bit*8)
                    if pointer: struct.pack_into('<Q', relocated, offset+bit*8, self.resource+pointer)
        self.u.mem_write(self.resource,bytes(relocated))
        self.write(self.evfl_context+0x38,self.nodes)
        self.write(self.evfl_context+0x40,4,4)
        self.write(self.nodes+3*0x20,self.flow_object)
        self.write(self.nodes+3*0x20+0x10,19,4)
        self.u.mem_write(self.chart,bytes(self.u.mem_read(self.resource+flowchart,0x48)))
        self.write(self.flow_object,self.chart)
        self.noop = base+0x9d9494
        self.write(self.methods+0x18,self.noop)
        for name,text in ((self.name,self.command_text),(self.flow_name,self.flow_text)):
            self.write(name,self.methods); self.write(name+8,text)
        self.u.mem_write(self.flow_text,b'SNPC_rco_00_Common\0')  # unrelated ROOT is intentional
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
    def command(self,name,flow=FLOW,query=False):
        native_name = NATIVE_COMMANDS.get(name, 'EventFlowAction'+name)
        self.u.mem_write(self.command_text,native_name.encode()+b'\0')
        self.u.mem_write(self.res_name,struct.pack('<H',len(flow))+flow.encode()+b'\0')
        self.write(self.chart+0x20,self.res_name)
        event_index = RESOURCE_EVENTS.get(native_name, RESOURCE_EVENTS[NATIVE_COMMANDS['SetTagFromSystem']])
        event = self.resource+events+event_index*0x28
        self.write(self.nodes+3*0x20+0x14,event_index,2)
        self.construct_argument(event,query)
        # Native GetParamCount independently follows this accessor's context
        # node, owning flowchart and ResEvent. It must find the original params.
        accessor = self.args+(0x28 if query else 0)
        params = resource_u64(events+event_index*0x28+0x10)
        count = struct.unpack_from('<H', RESOURCE, params+2)[0] if params else 0
        assert self.invoke(self.base+0x58aa0c,accessor) == count
        h=self.invoke(self.base+0x5bd550,self.command_text,0)
        if name in REGISTRY: assert h == struct.unpack_from('<I', RAW, REGISTRY[name])[0]
        self.write(self.action+0x10,h,4);self.write(self.check+0x10,h,4)
    def construct_argument(self,event,query):
        # Execute the actual ACNH EventFlow argument-construction windows and
        # ParamAccessor constructor. Supply engine bindings/current node only;
        # do not manufacture fields based on the feature's own assumptions.
        sp = self.ram+0x1d0000
        self.u.reg_write(UC_ARM64_REG_SP,sp)
        self.u.reg_write(UC_ARM64_REG_X29,self.args+0x50)
        self.u.reg_write(UC_ARM64_REG_X0,self.command_binding)
        self.u.reg_write(UC_ARM64_REG_X19,self.evfl_context)
        self.u.reg_write(UC_ARM64_REG_X20,3)
        self.u.reg_write(UC_ARM64_REG_X21,event)
        self.u.reg_write(UC_ARM64_REG_X22,self.actor_binding)
        self.u.reg_write(UC_ARM64_REG_X23,self.actor_binding)
        self.u.reg_write(UC_ARM64_REG_X9,self.nodes+3*0x20+8)
        self.write(sp+0x18,self.args+0x28)
        self.write(sp+0x28,self.nodes+3*0x20+8)
        start,end = (0x58d65c,0x58d688) if query else (0x58dc50,0x58dc90)
        guard = self.u.hook_add(UC_HOOK_CODE,lambda u,pc,n,d: u.emu_stop() if pc==self.base+end else None)
        self.u.emu_start(self.base+start,self.base+end,count=1000)
        self.u.hook_del(guard)
        assert self.u.reg_read(UC_ARM64_REG_PC)==self.base+end
        assert self.read(self.args+(0x10 if query else 0x38))==event
    def funds(self,player,savings,wallet,bags):
        self.write(self.event+0x2a8,player,1)
        current=self.balance(player)
        if current:self.invoke(self.base+0x257fb30,self.player,current)
        self.deposit(savings,player);self.cash[player]={'wallet':wallet,'bags':list(bags)}
    def total(self,player):
        return self.balance(player)+self.wallet+sum(self.bags)
    def affordable(self,price,flow=FLOW):
        self.command('HasBellCount',flow,query=True);self.write(self.check+0xd0,price,4)
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

# Join the actual argument/dispatcher path to the existing native BellPage and
# number-feedback fixture, so visibility alone cannot conceal a wiring failure.
from shopping_display_arm_test import DisplayMachine
# Both native state registries bind 1 to Idle and 3 to Play, not hidden.
assert RAW[0x3fe3b76:0x3fe3b76+13] == b'State::cIdle\0'
assert RAW[0x4000077:0x4000077+13] == b'State::cPlay\0'
class StorageDisplayMachine(StorageMachine, DisplayMachine):
    def __init__(self, base):
        super().__init__(base)
        self.phase(self.hud,3)   # State::cPlay, not hidden!
        self.phase(self.owner,3)
        self.money_context=self.ram+0x1e5000
    def phase(self,controller,state,pending=-1,next_pending=-1):
        self.write(controller+0x1b0,state,4)
        self.write(controller+0x1f0,pending & 0xffffffff,4)
        self.write(controller+0x1f4,next_pending & 0xffffffff,4)
    def native_disappear_complete(self,controller):
        # Run the ORIGINAL UIMoneyDisappear completion window, independently
        # of our presentation logic. Supply the money-screen context only.
        self.write(self.money_context+0x210,controller)
        self.write(self.action+0xb0,0,4)
        self.u.reg_write(UC_ARM64_REG_X8,self.money_context)
        self.u.reg_write(UC_ARM64_REG_X19,self.action)
        start,end=self.base+0xd18f14,self.base+0xd18f3c
        guard=self.u.hook_add(UC_HOOK_CODE,lambda u,pc,n,d: u.emu_stop() if pc==end else None)
        self.u.emu_start(start,end,count=64)
        self.u.hook_del(guard)
        assert self.u.reg_read(UC_ARM64_REG_PC)==end
        return bool(self.read(self.action+0xb0,4))
    def finish_out(self):
        self.phase(self.hud,1)
        self.phase(self.owner,1)
        assert self.native_disappear_complete(self.hud)
        assert self.native_disappear_complete(self.owner)
        self.write(self.panes['RootPane']+0x58,2,1)
        self.update()
        self.update_hud()

for base in (0,0x7100000000):
    m=StorageDisplayMachine(base)
    m.funds(0,2000000,77777,[])
    m.update_hud();assert not m.shown('N_ShoppingCard_00')
    m.dispatch('UIMoneyAppear');m.update_hud();m.update()
    assert m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
    assert m.texts[-1]=='2,077,777' and not m.feedback
    assert m.affordable(1800000)==0
    m.dispatch(price=1800000);m.update_hud()
    assert m.balance()==200000 and m.wallet==77777
    assert m.feedback[-1]==('Down',2)
    m.settle_hud(277777)
    assert m.feedback[-1]==('DownFinish',4)
    m.dispatch('UIMoneyDisappear');m.update_hud()
    assert not m.visible(0) # Payment scope ends before the Out animation.
    # Every frame of Out must retain the card, without revealing pocket Bells.
    # Play -> queued Out -> Disappear -> queued Idle; none is completed Idle.
    for state,pending,next_pending in ((3,-1,-1),(3,4,-1),(4,-1,-1),(4,1,-1),(1,-1,3)):
        m.phase(m.hud,state,pending,next_pending)
        m.phase(m.owner,5) # Auxiliary State::cDisappear (different enum value).
        assert not m.native_disappear_complete(m.hud)
        assert not m.native_disappear_complete(m.owner)
        for _ in range(8):
            m.update();m.update_hud();m.update()
            assert m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
    # A hidden HUD must not release the still-closing auxiliary counter.
    m.phase(m.hud,1)
    assert m.native_disappear_complete(m.hud)
    for _ in range(8):
        m.update_hud();m.update()
        assert m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
    m.finish_out()
    assert not m.shown('N_ShoppingCard_00') and m.shown('T_Num_00')
    assert not m.shown('RootPane') # Cash is restored only behind the hidden page.
    m.write(m.panes['RootPane']+0x58,3,1)
    m.update_hud()
    assert not m.shown('N_ShoppingCard_00') and m.shown('T_Num_00')
    # Cancellation/teardown can precede animation completion. An explicit
    # unrelated money counter must still take over immediately.
    m.phase(m.hud,3);m.phase(m.owner,3)
    m.dispatch('UIMoneyAppear');m.update_hud();m.update()
    m.invoke(m.callbacks[0x60f340],m.event);m.update_hud()
    assert not m.visible(0) and m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
    m.dispatch('OtherAction',flow='SNPC_rco_00_Common');m.update_hud()
    assert m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
    m.dispatch('UIMoneyAppear',flow='SNPC_rco_10_Common_MoveHouse');m.update_hud()
    assert not m.shown('N_ShoppingCard_00') and m.shown('T_Num_00')
    m.dispatch('UIMoneyAppear');m.update_hud()
    m.dispatch('UIMoneyDisappear');m.update_hud()
    m.dispatch('UIMoneyAppear');m.update_hud() # Reopen interrupts a pending Out.
    assert m.visible(0) and m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
    m.dispatch('UIMoneyDisappear');m.finish_out()
    # Reverse completion order too. The fixture deliberately shares the pane
    # binding between both controllers, exposing updater-order regressions.
    m.phase(m.hud,4);m.phase(m.owner,5)
    m.dispatch('UIMoneyAppear');m.update();m.update_hud()
    m.dispatch('UIMoneyDisappear')
    m.phase(m.owner,1)
    for _ in range(8):
        m.update();m.update_hud()
        assert m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
    m.finish_out()
    # Fast dialogue may dispatch both UI commands before the first render.
    m.phase(m.hud,3);m.phase(m.owner,3)
    m.dispatch('UIMoneyAppear');m.dispatch('UIMoneyDisappear')
    for _ in range(30): m.dispatch('OtherAction')
    m.update_hud();m.update()
    assert m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
    m.phase(m.hud,1)
    m.update_hud();m.update()
    assert m.shown('N_ShoppingCard_00') and not m.shown('T_Num_00')
    m.finish_out()
    # Cross-check our restoration against the actual native command's answer
    # for every phase and both pending-transition slots, not a guessed enum.
    for state in range(6):
        for pending,next_pending in ((-1,-1),(1,-1),(-1,1),(1,1)):
            m.phase(m.hud,state,pending,next_pending)
            m.phase(m.owner,state,pending,next_pending)
            m.dispatch('UIMoneyAppear');m.update();m.update_hud()
            m.dispatch('UIMoneyDisappear')
            done=m.native_disappear_complete(m.hud)
            assert m.native_disappear_complete(m.owner)==done
            m.write(m.panes['RootPane']+0x58,2 if done else 3,1)
            m.update_hud();m.update()
            assert m.shown('N_ShoppingCard_00') != done
            assert m.shown('T_Num_00') == done
    m.finish_out()
    # Missing, out-of-range, stale and mismatched native node contexts cannot
    # activate card money. The constructor/accessor path above is the valid case.
    gate=m.module.find('shopping','StorageFlow')
    assert m.invoke(gate,0,0,0x38)==0
    m.command('UIMoneyAppear')
    m.write(m.args+8,0);assert m.invoke(gate,m.args,0,0x38)==0
    m.command('UIMoneyAppear')
    m.write(m.args+0x10,4,4);assert m.invoke(gate,m.args,0,0x38)==0
    m.command('UIMoneyAppear')
    m.write(m.args+0x14,18,4);assert m.invoke(gate,m.args,0,0x38)==0
    m.command('UIMoneyAppear')
    m.write(m.args+0x38,m.resource+events);assert m.invoke(gate,m.args,0,0x38)==0
    print('PASS: native storage args/payment/feedback; actual UIMoneyDisappear completion, Play/Out/queued-Idle states, interleaved/either completion order, rapid dialogue, teardown/reopen/ordinary takeover, invalid contexts; base',hex(base))
