"""Execute the compiled B shortcut, native focus/input and dialog choice routing.

UI callback adapters and rendering are modeled; native input polling, focus-list
updates and No/Yes answer dispatch execute from the supplied 3.0.3 main.
"""
from runtime_arm_test import Machine
from unicorn.arm64_const import *

class BackMachine(Machine):
    def __init__(self, base):
        super().__init__(base)
        self.d = self.ram + 0x100000
        self.dp = self.d + 0x3000
        self.ctx = self.d + 0x4000
        self.manager = self.d + 0x5000
        self.cursor = self.d + 0x6000
        self.no = self.d + 0x7000
        self.yes = self.d + 0x8000
        self.nc = self.d + 0x9000
        self.yc = self.d + 0xb000
        self.vt = self.d + 0xd000
        self.events = []
        self.write(base + 0x5590d38, self.d)
        self.write(self.d, base + 0x49cd650)
        self.write(self.d + 0x210, self.dp)
        self.write(self.d + 0x2388, self.owner + 0x1c4d0)
        self.write(self.d + 0x1b0, 3, 4)
        self.write(self.d + 0x1f0, 0xffffffffffffffff)
        self.write(self.d + 0x220, 0xffffffffffffffff)
        self.write(self.d + 0x248, self.d + 0xe000)
        self.write(self.d + 0x250, self.d + 0xe100)
        self.write(self.owner + 0x1c4f0, 5, 4)
        self.write(self.dp + 0x210, self.ctx)
        self.write(self.dp + 0x570, self.hint)
        self.write(self.dp + 0x578, self.no)
        self.write(self.dp + 0x588, self.yes)
        self.write(self.no + 0x218, self.nc)
        self.write(self.yes + 0x218, self.yc)
        self.write(self.ctx + 0x48, self.manager)
        for off in (0, 0x18, 0x30):
            self.write(self.ctx + off, self.ctx + off)
            self.write(self.ctx + off + 8, self.ctx + off)
        # No pre/post input callbacks; linked list contains two cursor targets.
        self.write(self.ctx + 0x68, self.vt)
        self.write(self.ctx + 0x88, self.vt)
        self.write(self.vt + 0x10, self.stub + 0xa00)
        self.write(self.vt + 0x18, self.stub + 0xa00)
        self.services[self.stub + 0xa00] = ('noop', None)
        self.write(self.manager, self.nc + 8)
        self.write(self.manager + 8, self.yc + 8)
        self.write(self.manager + 0x10, 2, 4)
        self.write(self.manager + 0x14, 8, 4)
        self.write(self.manager + 0x18, 1, 4)
        self.write(self.manager + 0x20, self.manager + 0x100)
        self.write(self.manager + 0x100, self.cursor)
        self.write(self.cursor + 0x10, self.yc)
        # Same timer initialization as native focus entry (3C48C00..20).
        for off in range(0x20,0xa0,8): self.write(self.cursor+off,0xfffffffefffffffe)
        for node, prev, nxt in ((self.nc,self.manager,self.yc+8),(self.yc,self.nc+8,self.manager)):
            self.write(node,self.vt)
            self.write(node+8,prev)
            self.write(node+16,nxt)
            self.write(node+0x14b0,1,4)
            for group in range(4):
                for key in range(32):
                    self.write(node+0x60+group*0x500+key*0x28,self.vt+0x200)
                    self.write(node+0x68+group*0x500+key*0x28,node)
            for off in (0x20,0x40):
                self.write(node+off,self.vt+0x100)
            self.write(node+0x28,node)
            self.write(node+0x48,node)
        self.write(self.vt+0x200,self.stub+0xa40)
        self.write(self.vt+0x210,self.stub+0xa00)
        self.services[self.stub+0xa40]=('button-event',None)
        self.write(self.vt+0x100,self.stub+0xa20)
        self.services[self.stub+0xa20]=('focus-event',None)
        self.write(base+0x4ab2230,self.d+0xf000)
        self.write(self.d+0xf000,self.app)
        self.write(self.mod.find('L13originalInputE'),base+0x3c58d24)
    def service(self,u,pc,size,data):
        service=self.services.get(pc)
        if service and service[0]=='noop':
            u.reg_write(UC_ARM64_REG_X0,0);u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR));return
        if service and service[0]=='button-event':
            event=self.reg(2);key=self.read(event+0xc,4);group=self.reg(1)
            self.events.append(('key',key,group))
            if key==4 and group==0:
                node=self.read(self.reg(0)+8)
                fake=self.d+0xf100
                self.write(fake+8,self.no if node==self.nc else self.yes)
                u.reg_write(UC_ARM64_REG_X0,self.d);u.reg_write(UC_ARM64_REG_X1,fake)
                u.reg_write(UC_ARM64_REG_PC,self.base+0x2cfb318);return
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR));return
        if service and service[0]=='focus-event':
            node=self.read(self.reg(0)+8)
            self.events.append(('focus',node))
            u.reg_write(UC_ARM64_REG_PC,u.reg_read(UC_ARM64_REG_LR));return
        super().service(u,pc,size,data)
    def press(self,buttons,blocked=0,consumed=0):
        self.write(self.rows,buttons,4);self.write(self.rows+0x3c,consumed,4)
        self.call(self.mod.find('L15WearChoiceInput'),self.dp,self.snapshot,blocked)
        assert self.read(self.rows,4)==buttons
    def focus(self, i):
        # Base construction calls this before this fixture is initialized.
        if not hasattr(self,'cursor'): return super().focus(i)
        self.write(self.cursor+0x10,self.nc if i==0 else self.yc)

def main():
    checks=0
    for base in (0,0x7100000000):
        m=BackMachine(base)
        m.press(0);m.press(0x20)
        assert m.read(m.cursor+0x10)==m.nc
        assert m.read(m.d+0x220,4)==0xffffffff
        assert m.read(m.owner+0x1c318,4)==0xffffffff
        for _ in range(30):m.press(0x20)
        assert m.read(m.d+0x220,4)==0xffffffff
        m.press(0);m.press(0x20)
        assert m.read(m.d+0x220,4)==0
        assert m.read(m.rows+0x3c,4)&0x30==0x20
        # The original fitting-room answer handler routes No to send, Yes to wear.
        m.call(base+0x31141cc,m.owner,m.d+0xf200,0)
        assert m.read(m.owner+0x1c318,4)==4
        m.write(m.owner+0x1c318,0xffffffff,4)
        m.call(base+0x31141cc,m.owner,m.d+0xf200,1)
        assert m.read(m.owner+0x1c318,4)==5
        checks+=1
        # Replay live Eden 0.3.74 input: 0 -> 0x10020 -> 0 -> 0x10020.
        # The accompanying flag is preserved when B is forwarded as native A.
        m=BackMachine(base);m.press(0);m.press(0x10020)
        assert m.read(m.cursor+0x10)==m.nc
        assert m.read(m.d+0x220,4)==0xffffffff
        for _ in range(30):m.press(0x10020)
        assert m.read(m.d+0x220,4)==0xffffffff
        m.press(0);m.press(0x10020)
        assert m.read(m.d+0x220,4)==0
        assert m.read(m.rows,4)==0x10020
        checks+=1
        for extra in (1,2,4,8,16,64,128,0x20000):
            m=BackMachine(base);m.press(0)
            # Isolate the shortcut: native arrows/A may independently act.
            m.write(m.mod.find('L13originalInputE'),m.stub)
            m.press(0x10020|extra)
            assert m.read(m.cursor+0x10)==m.yc
            assert m.read(m.rows+0x3c,4)==0
            checks+=1
        # Already focused No, including the alternate visual style.
        for style in (0,1,2):
            m=BackMachine(base);m.write(m.d+0x22c,style,4)
            if style==2:
                m.write(m.dp+0x578,0);m.write(m.dp+0x588,0)
                m.write(m.dp+0x580,m.no);m.write(m.dp+0x590,m.yes)
            m.focus(0);m.press(0);m.press(0x20)
            assert m.read(m.d+0x220,4)==(1 if style==2 else 0)
            checks+=1
        # Physical A keeps its normal Yes answer. No synthetic A leaks out.
        m=BackMachine(base);m.press(0);m.press(0x10)
        assert m.read(m.d+0x220,4)==2
        checks+=1
        m=BackMachine(base);m.press(0x20);m.press(0x20)
        assert m.read(m.cursor+0x10)==m.yc
        m.press(0);m.press(0x20)
        assert m.read(m.cursor+0x10)==m.nc
        checks+=1
        # No shortcut on another dialog, during transitions, under another owner,
        # with missing/disabled focus, consumed input or simultaneous buttons.
        guards=(
            ('other question',lambda m:m.write(m.owner+0x1c4f0,4,4)),
            ('other delegate',lambda m:m.write(m.d+0x2388,m.owner)),
            ('other page',lambda m:m.write(m.d+0x210,m.page)),
            ('other controller',lambda m:m.write(m.d,base+0x49cd658)),
            ('not ready',lambda m:m.write(m.d+0x1b0,2,4)),
            ('transition',lambda m:m.write(m.d+0x1f0,4,4)),
            ('already chosen',lambda m:m.write(m.d+0x220,2,4)),
            ('three choices',lambda m:m.write(m.d+0x258,m.d+0xe200)),
            ('no focus',lambda m:m.write(m.cursor+0x10,0)),
            ('disabled target',lambda m:m.write(m.nc+0x14b0,0,4)),
            ('blocked context',lambda m:m.write(m.ctx+0x54,1,1)),
        )
        for name,mutate in guards:
            m=BackMachine(base);m.press(0);mutate(m)
            before=m.read(m.cursor+0x10)
            m.press(0x20)
            assert m.read(m.cursor+0x10)==before,name
            assert not any(e[:2]==('key',4) for e in m.events),name
            checks+=1
        for blocked,consumed,buttons in ((1,0,32),(0,32,32),(0,16,32),(0,0,32|128)):
            m=BackMachine(base);m.press(0);m.press(buttons,blocked,consumed)
            assert m.read(m.cursor+0x10)==m.yc
            assert m.read(m.d+0x220,4)==0xffffffff
            checks+=1
        # A rejected press must be released before it can act later.
        m=BackMachine(base);m.press(0);m.press(32,1);m.press(32)
        assert m.read(m.cursor+0x10)==m.yc
        m.press(0);m.press(32);assert m.read(m.cursor+0x10)==m.nc
        checks+=1
        # Reopen while B is held: the new ready-state edge starts unarmed.
        m=BackMachine(base);m.press(0);m.press(32)
        m.write(m.d+0x1b0,2,4);m.press(32)
        m.write(m.d+0x1b0,3,4);m.focus(1);m.press(32)
        assert m.read(m.cursor+0x10)==m.yc
        m.press(0);m.press(32);assert m.read(m.cursor+0x10)==m.nc
        checks+=1
    print(f'PASS: {checks} fitting-room B cases at two bases; native focus, input and answer dispatch')

if __name__=='__main__':main()
