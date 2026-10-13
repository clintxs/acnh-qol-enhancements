"""Run the inherited workbench/cooking shortcut assertions on the shared fixture."""
from quantity_shortcut_fixture import *

cases=0
# Cooking supplies its own page/footer while inheriting the same native state
# table. Keep the fixture tied to those real vtable entries, not only mocks.
for offset,target in ((0x1e0,0x34942f0),(0x218,0x2cbb710),(0x248,0x2cbb99c)):
    assert struct.unpack_from('<Q',RAW,0x49c8c50+offset)[0] == target
for base,kitchen in product((0x7100000000,0x7200000000),(False,True)):
    m=ShortcutMachine(base,kitchen)
    for valid,materials,multiple,supports in product((False,True),repeat=4):
        m.valid,m.materials,m.multiple=valid,materials,multiple
        m.write(m.seq+0x1c1f0,int(supports),1)
        m.enter();m.reset_footer();m.hook(m.footer_offset,m.seq)
        labels=m.hints()
        allowed=valid and materials and multiple and supports
        assert labels==[(6,'2004'),(5,'2003'),(0xffffffff,'2005')]+([(7,'qol_crafting.set_quantity')] if allowed else []),labels
        m.input_y();before=m.native_inputs;m.input_y(0x80)
        assert m.read(m.seq+0x1f0,4)==(9 if allowed else 0xffffffff)
        assert m.native_inputs==before+int(not allowed)
        cases+=1
    m.valid=m.materials=m.multiple=True;m.write(m.seq+0x1c1f0,1,1)
    for capacity in (0,1,2,3,4,5):
        m.reset_footer(capacity);m.hook(m.footer_offset,m.seq)
        assert len(m.hints())==min(capacity,4)
        cases+=1
    # Held Y cannot open on entry. Blocked/consumed/competing/pending input
    # cannot queue a selector; the next accepted gesture requires release.
    for consumed,blocked,buttons,pending,ready in ((0,0,0x80,-1,True),(0x80,0,0x80,-1,True),
            (0,1,0x80,-1,True),(0,0,0x90,-1,True),(0,0,0xa0,-1,True),
            (0,0,0x80,4,True),(0,0,0x80,-1,False)):
        m.enter();m.ready=ready
        if (consumed,blocked,buttons,pending,ready)!=(0,0,0x80,-1,True):m.input_y()
        m.write(m.seq+0x1f0,pending&0xffffffff,4)
        m.input_y(buttons,consumed,blocked)
        assert m.read(m.seq+0x1f0,4)==pending&0xffffffff
        cases+=1
    m.ready=True;m.enter();m.input_y();m.input_y(0x80)
    assert m.read(m.seq+0x1f0,4)==9 and m.read(m.seq+0x239,1)&4
    m.hook(0x3494e7c,m.seq) # Native Play exit disables the preview keys.
    assert m.read(m.input+0x54,1)==1
    m.write(m.seq+0x1b0,9,4);m.write(m.seq+0x1f0,0xffffffff,4)
    m.invoke(base+0x3496160,m.seq) # Native quantity opening/material transfer.
    assert m.attached==1 and m.read(m.child+0x165,1)==1
    m.closed=False;m.invoke(base+0x349634c,m.seq)
    assert m.read(m.seq+0x1f0,4)==0xffffffff
    m.confirmed=True;m.invoke(base+0x349634c,m.seq)
    assert m.read(m.seq+0x1c1c8,4)==0 # Native crafting result propagates unchanged.
    m.confirmed=False;m.write(m.seq+0x1c1c8,0xffffffff,4)
    m.closed=True;m.invoke(base+0x349634c,m.seq) # B/cancel returns to preview.
    assert m.detached==1 and m.read(m.seq+0x1f0,4)==3
    m.reset_footer();m.enter(prior=9)
    assert m.read(m.input+0x54,1)==0 and (7,'qol_crafting.set_quantity') in m.hints()
    # Leaving clears ownership: same page receiving later Y stays native.
    m.hook(0x3494e7c,m.seq);m.write(m.input+0x54,0,1)
    m.input_y();m.input_y(0x80)
    assert m.read(m.seq+0x1f0,4)==0xffffffff
    # X now favorites through the native Play action, once per press. Y must
    # be consumed even for a recipe that cannot open quantity.
    m.enter();m.write(m.seq+0x1c1cc,0xffffffff,4);m.input_y()
    m.input_y(0x40)
    assert m.read(m.seq+0x1c1cc,4)==3 and m.read(m.row+0x3c,4)&0x40
    m.invoke(base+0x3494d00,m.seq) # Native Play dispatches the favorite toggle.
    assert m.favorites==1
    m.write(m.seq+0x1c1cc,0xffffffff,4);m.input_y(0x40)
    assert m.read(m.seq+0x1c1cc,4)==0xffffffff
    m.materials=False;m.input_y();m.input_y(0x80)
    assert m.read(m.row+0x3c,4)&0x80 and m.read(m.seq+0x1c1cc,4)==0xffffffff
    m.materials=True
    # Footer building restores the key binding after copying the X icon.
    wrapper=m.read(m.page+0x570);key_pointer=m.read(wrapper+0xa0)
    assert m.read(key_pointer+0x14,4)==7
    cases+=4
    saved_type=m.read(m.page);m.write(m.page,base+0x49de8f0)
    m.enter();m.input_y();m.input_y(0x80)
    assert m.read(m.seq+0x1f0,4)==0xffffffff and not m.read(m.row+0x3c,4)&0x80
    m.write(m.page,saved_type);cases+=1
    # Existing localized caption routing, including with Pockets installed.
    regs,key,source=m.alloc(0x200),m.alloc(),m.alloc()
    m.write(key+8,source)
    for label in ('qol_crafting.set_quantity','2004','qol_pockets.put_in_storage'):
        m.u.mem_write(source,label.encode()+b'\0');m.write(regs+16,123);m.write(regs+24,key)
        m.hook(0x2de4940,regs)
        if label=='qol_crafting.set_quantity':
            assert m.string(m.read(m.read(regs+16)+8))=='DIYDialogConfirmNum'
            assert m.string(m.read(m.read(regs+24)+8))=='0001'
        else:assert m.read(regs+16)==123 and m.read(regs+24)==key
        cases+=1
    # Module conflicts isolate the shortcut, including with every other
    # Crafting setting disabled. No partial shortcut hooks get installed.
    for site in (0x3494b2c,0x3494e7c,0x346ec1c,0x2cbb99c,0x2cbb710,0x2de4940,0x3496160,0x2de2b00):
        q=Machine(base,{'crafting':DEFAULT+b'diy_app_preview=false\nworkbench_preview_tilt=false\ncrafting_quantity_wrap=false'})
        q.write(base+site,0xdeadbeef,4);q.boot();q.activate()
        assert 'quantity shortcut: DISABLED' in q.text and 'BOOT_PARTIAL' in q.text
        assert not any(base+off in q.hooks for off in (0x3494b2c,0x3494e7c,0x346ec1c,0x2cbb99c,0x2de4940))
        assert base+0x22fc6bc in q.hooks and base+0x2279a14 in q.hooks
        cases+=1
print(f'PASS: {cases} quantity-shortcut cases at two bases; native workbench/cooking preview/footer, quantity open/cancel, material gates, fresh Y, caption routing and isolated conflicts. Live Eden test pending.')
