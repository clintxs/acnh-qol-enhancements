"""Execute native cash readers, debit/change, credit/overflow and encryption.

Resident/parameter services and inventory iteration/insertion are modeled;
the game's actual monetary arithmetic and compiled Shopping Card hook execute.
"""
from shopping_card_arm_test import CardMachine, WALLET_CALLS, DEBIT_CALLS
from unicorn.arm64_const import UC_ARM64_REG_PC


class NativeCashMachine(CardMachine):
    def __init__(self, base):
        self.native_entry = None
        super().__init__(base)
        extra = self.ram + 0x300000
        self.u.mem_map(extra, 0x100000)
        flags = extra + 0x1000
        self.write(base + 0x4ab4fb8, extra); self.write(extra, flags)
        singleton = extra + 0x20000; bank = extra + 0x80000
        self.write(base + 0x4ab43c0, extra + 8); self.write(extra + 8, singleton)
        self.write(singleton + 0x57e58, bank)
        self.write(base + 0x4ac0428, extra + 16); self.write(extra + 16, bank)
        self.write(bank + 0x1f8, 0x8a4, 2); self.write(bank + 0x1d8, 1, 4)
        self.write(bank + 0x1e0, bank + 0x300); self.write(bank + 0x300, bank + 0x414)
        self.write(bank + 0x400, 1, 4); self.write(bank + 0x404, 8, 4)
        self.write(bank + 0x408, 1, 2); self.write(bank + 0x414, 20, 4)
        self.write(base + 0x4ac00c8, bank + 0x500)
        self.write(bank + 0x500, 1, 4); self.write(bank + 0x508, bank + 0x510)
        self.write(bank + 0x510, 0x86, 2)
        self.write(base + 0x4ab37e0, bank + 0x600)
        self.write(bank + 0x600, 0xfffe)
        self.write(base + 0x549bc2c, 255, 4)
        for player in range(8):
            self.write(flags + player * 0x13f0 + 0x13d4, 1, 1)
            self.write(flags + player * 0x13f0 + 0x13d6, 1, 1)
            view = self.resident(player) + 0x194b0
            self.write(view + 0x78, 0); self.write(view + 0x80, self.inventory(player))
            self.write(view + 0x98, 0)
            self.set_cash(player, 0, [])

    def inventory(self, player): return self.resident(player) + 0x4000
    def slot(self, inventory, index): return inventory + (0xb8 + index*8 if index < 20 else (index-20)*8)
    def set_cash(self, player, wallet, bags):
        inventory = self.inventory(player)
        for index in range(40): self.write(self.slot(inventory,index), 0xfffe)
        self.write(inventory + 0x158, 20, 4); self.write(inventory + 0xa0, 20, 4)
        self.write(inventory + 0x178, 2, 1)
        self.write(inventory + 0x170, 0)
        self.invoke(self.base + 0x275dc20, inventory + 0x170, wallet, 99999)
        for index, value in enumerate(bags):
            self.write(self.slot(inventory,index), 0x8a4 | (value << 32))
        # Ordinary furniture occupies a slot and must survive all cash writes.
        self.write(self.slot(inventory,39), 0x1234)

    def snapshot(self, player):
        self.write(self.player, player, 1)
        wallet = 99999 - self.invoke(self.base + 0x257ad90, self.player)
        native_cash = self.invoke(self.ram + 0x9000, self.player)
        inventory = self.inventory(player)
        bags = sum(self.read(self.slot(inventory,i)+4,4) for i in range(40)
                   if self.read(self.slot(inventory,i),2) == 0x8a4)
        assert native_cash == wallet + bags
        assert self.read(self.slot(inventory,39)) == 0x1234
        return self.balance(player), wallet, bags

    def service(self, u, pc, size, data):
        off = pc - self.base
        if off == self.native_entry:
            self.native_entry = None
            return # execute the real native entry, bypassing only this test's hook router
        service = self.services.get(pc)
        if service in (('card_original','ShopWallet'),('card_original','ShopDebit')):
            self.native_entry = 0x257abf0 if service[1] == 'ShopWallet' else 0x257b340
            u.reg_write(UC_ARM64_REG_PC, self.base + self.native_entry); return
        if off in (0x257ad90,0x257af90): return
        if off == 0x2482d8c: self.finish(self.read(self.reg(0),1)); return
        if off == 0x2407c6c:
            self.write(self.reg(0),255,1); self.finish(); return
        if off == 0x267d4ac:
            self.write(self.reg(0)+4,99000,4); self.finish(); return
        if off == 0x267d6c4: self.finish(self.read(self.reg(0),2)); return
        if off == 0x19ed730: self.finish(self.read(self.reg(0)+12,4)); return
        if off in (0x267c8f0,0x267cd30):
            array = self.read(self.reg(1)+8)
            values = [self.read(array+i*8+4,4) for i in range(self.reg(0))
                      if self.read(array+i*8,2)==0x8a4]
            if off == 0x267c8f0: self.finish(sum(values))
            else:
                free = sum(self.read(array+i*8,2)==0xfffe for i in range(self.reg(0)))
                self.finish(free*99000 + sum(99000-value for value in values))
            return
        if off == 0x26fa2dc:
            slot = self.slot(self.reg(0), self.reg(1))
            assert self.read(slot,2) == 0x8a4
            self.write(slot,0xfffe); self.finish(1); return
        if off == 0x267cb20:
            # The native overflow branch clears cash items before repacking.
            array = self.read(self.reg(1)+8)
            for index in range(self.reg(0)):
                if self.read(array+index*8,2)==0x8a4: self.write(array+index*8,0xfffe)
            self.finish(); return
        if off == 0x26faa50:
            for index in range(40):
                slot = self.slot(self.reg(0),index)
                if self.read(slot,2)==0xfffe:
                    self.write(slot,self.read(self.reg(1))); self.finish(1); return
            self.finish(0); return
        super().service(u, pc, size, data)


for base in (0x7100000000,0x7200000000):
    m = NativeCashMachine(base)
    for player in range(8):
        for caller in (DEBIT_CALLS[3],0x311d400,0x2b7f588):
            for savings,wallet,bags,price,expected in (
                (300000,99000,[99000,99000,2000],500000,(0,99000,0)),
                (300000,99000,[99000,51000],500000,(0,49000,0)),
                (1000,50000,[10000],500,(500,50000,10000)),
                (1000,50000,[],20000,(0,31000,0)),
                (0,1000,[99000],10000,(0,90000,0)),
                (0,90000,[99000],10000,(0,80000,99000)),
                (1,1,[1],3,(0,0,0)),
                (1,1,[1],4,(1,1,1)),
            ):
                m.write(m.player,player,1)
                m.invoke(base+0x257fb30,m.player,999999999); m.deposit(savings,player)
                m.set_cash(player,wallet,bags)
                before = m.snapshot(player)
                assert m.route(0x257abf0,WALLET_CALLS[0],m.player) == sum(before)
                m.route(0x257b340,caller,m.player,price)
                after = m.snapshot(player)
                assert sum(after)==sum(before)-(price if price<=sum(before) else 0),(before,price,after)
                if expected is not None: assert after==expected,(before,price,after,expected)
    print('PASS: native cash/savings reads, encrypted writes, wallet-first helper adapted to bags-first, denomination change/overflow, ordinary items preserved, all residents, shops/fitting/catalog, base',hex(base))
