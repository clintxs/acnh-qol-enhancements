"""Exercise the compiled phone-preview integration against ACNH 3.0.3 code.

UI allocation, async loading, rendering and animations are modeled. Native
layout-table construction, confirmation translation/dispatch, recipe copy,
navigation, item lookup and craft-action dispatch execute from main.raw.
This is not a live Eden rendering/input test.
"""
import struct
from core_arm_test import Machine, RAW
from unicorn.arm64_const import *

HOOKS = (0x2e9cde0, 0x2e9fbb0, 0x2e9d538)
GUARDS = (0x2e9cf2c, 0x2e9cfe8, 0x3492b50, 0x34930b0, 0x3493430,
          0x34940e0, 0x3492fec, 0x3494d8c, 0x34947d0, 0x3495408,
          0x346fdb0, 0x3473ac8, 0x376890c, 0x3769260, 0x3768c7c, 0x2af0d30)


class PreviewMachine(Machine):
    def __init__(self, base):
        self.running = False
        self.originals = {}
        self.events = []
        super().__init__(base, {})
        self.u.mem_map(self.ram + 0x200000, 0x600000)
        # The real loader relocates these vtables/GOT entries before startup.
        for offset in range(0x4a3b768, 0x4a3ba38, 8):
            value = struct.unpack_from('<Q', RAW, offset)[0]
            self.write(base + offset, base + value if value else 0)
        for table in (0x49e8408, 0x4a5ef98):
            for offset in (0, 8, 16):
                self.write(base + table + offset,
                           base + struct.unpack_from('<Q', RAW, table + offset)[0])
        self.boot(); self.activate()
        self.running = True
        self.next_preview = self.ram + 0x300000
        self.next_aux = self.ram + 0x400000
        self.fail_allocation = False
        self.hidden = True
        self.reloads = []
        self.sounds = []
        self.tls = self.aux(0x200)
        self.heap = self.aux()
        self.previous_heap = self.aux()
        self.write(self.tls + 0xa8, self.previous_heap)
        self.thread_manager = self.aux()
        self.write(self.thread_manager + 0x68, 7, 4)
        self.global_pointer(0x4ab3350, self.thread_manager)
        self.game = self.aux(0x500)
        resource = self.aux()
        self.write(self.game + 0x418, resource)
        self.write(resource + 0xc, 123, 4)
        self.global_pointer(0x4ab2718, self.game)
        for offset in (0x4ab2020, 0x4ac39b0, 0x4ab3170):
            self.write(base + offset, self.aux())
        self.context = self.aux()
        manager = self.aux()
        self.write(manager + 0x30, self.context)
        self.global_pointer(0x4ab2230, manager)
        self.layout_table = self.aux(0x200)
        self.write(self.layout_table + 0x140, self.stub('hidden'))
        self.write(self.layout_table + 0x1a8, self.stub('button_source'))
        self.write(self.layout_table + 0x1b8, base + 0x375ab74)
        self.write(self.layout_table + 0x1c0, base + 0x375b8e0)
        self.write(self.layout_table + 0x1c8, base + 0x2ab1d24)
        self.write(self.layout_table + 0x1d0, self.stub('button_feedback'))
        self.button_table = self.aux(0x200)
        self.write(self.button_table + 0x140, self.stub('button_visibility'))
        self.visibility_table = self.aux()
        self.write(self.visibility_table + 0x20, self.stub('button_hidden'))
        self.native_actions = {
            0x2478c0: 'allocate', 0x3492b50: 'construct', 0x376890c: 'initialize',
            0x3493c20: 'initialize_children', 0x3769260: 'activate_child',
            0x3768c7c: 'remove_child', 0x3f03880: 'lock', 0x3f038c0: 'unlock',
            0x3f033a0: 'tls', 0x2af0d30: 'layout_visibility',
            0x34930b0: 'bind_preview', 0x34940e0: 'frame_preview',
            0x2b0c960: 'show_preview',
            0x2e96b80: 'render_phone', 0x335b620: 'refresh_grid',
            0x346fdb0: 'reload', 0x346ea1c: 'no_animation',
            0x346eac8: 'no_animation', 0x267d4ac: 'item_init', 0x32ba168: 'sound',
        }

    def aux(self, size=0x100):
        pointer = self.next_aux
        self.next_aux += (size + 15) & ~15
        return pointer

    def stub(self, action):
        pointer = self.aux(0x10)
        self.services[pointer] = ('preview', action)
        return pointer

    def global_pointer(self, offset, value):
        storage = self.aux()
        self.write(storage, value)
        self.write(self.base + offset, storage)

    def invoke(self, address, *args):
        for i, arg in enumerate(args):
            self.u.reg_write(UC_ARM64_REG_X0 + i, arg)
        self.call(address)
        return self.reg(0)

    def hook(self, offset, *args):
        return self.invoke(self.hooks[self.base + offset], *args)

    def settle(self, page, state):
        self.write(page + 0x1b0, state, 4)
        self.write(page + 0x1f0, 0xffffffffffffffff)
        self.write(page + 0x200, 0, 1)

    def phone(self, resident=0, count=3):
        page = self.ram + 0x10000 + resident * 0x100000
        self.u.mem_write(page, bytes(0xf6380))
        self.write(page + 0x1f8, resident, 1)
        self.settle(page, 3)
        # Native lists are circular sentinel lists; nodes use native offsets.
        self.write(page + 0x50, page + 0x50)
        self.write(page + 0x58, page + 0x50)
        self.write(page + 0x64, 0x10, 4)
        manager = self.aux()
        self.write(manager + 0xb8, self.heap)
        self.write(page + 0x98, manager)
        layout = self.layout()
        self.write(page + 0x210, layout)
        self.write(layout + 0x580, self.aux(0x500))
        # Execute the native table constructor. The original fixture invented
        # six rows, masking the bug that rejected the game's 35-row table.
        self.native_layout_tables(layout)
        columns = self.read(layout + 0x430, 4)
        entries = self.read(layout + 0x438)
        for row, function in ((0, 0x2e96a80), (1, 0x2e99d90), (5, 0x2e99d94)):
            self.write(entries + row * columns * 0x20, self.base + 0x49e8408)
            self.write(entries + row * columns * 0x20 + 8, page)
            self.write(entries + row * columns * 0x20 + 0x10, self.base + function)
        recipes = self.aux(count * 8)
        for i in range(count):
            recipe = self.aux(0xd0)
            data = bytearray((j + i * 13) & 255 for j in range(0xd0))
            struct.pack_into('<H', data, 0, 100 + i)
            struct.pack_into('<Q', data, 8, 200 + i)
            self.u.mem_write(recipe, bytes(data))
            self.write(recipes + i * 8, recipe)
        self.write(page + 0x8a350, count, 4)
        self.write(page + 0x8a358, recipes)
        self.write(page + 0x218, count - 1, 4)
        self.hook(0x2e9cde0, page)
        assert self.read(entries + 0x10) == self.base + 0x2e96a80
        assert self.read(entries + 5 * columns * 0x20 + 0x10) == self.base + 0x2e99d94
        assert self.read(entries + columns * 0x20 + 0x10) != self.base + 0x2e99d90
        return page

    def native_layout_tables(self, layout):
        # Only allocation and the final resource-bind virtual are modeled;
        # table dimensions and delegate initialization execute in game code.
        self.global_pointer(0x4ab2698, 0)
        table = self.aux(0xd0)
        self.write(table + 0xc8, self.stub('layout_resources'))
        self.write(layout, table)
        params = self.aux(0x30)
        for offset, value in ((8, 1), (0xc, 1), (0x14, 1), (0x18, 2), (0x1c, 2)):
            self.write(params + offset, value, 4)
        self.write(params + 0x28, 1, 1)
        self.native_actions[0x2478c0] = 'layout_allocate'
        try:
            self.invoke(self.base + 0x3759dd0, layout, params)
        finally:
            self.native_actions[0x2478c0] = 'allocate'
        assert self.read(layout + 0x430, 4) == 2
        assert self.read(layout + 0x434, 4) == 35
        assert self.read(layout + 0x468, 4) == 2
        assert self.read(layout + 0x46c, 4) == 5
        self.write(layout, self.layout_table)

    def layout(self):
        layout = self.aux(0x600)
        self.write(layout, self.layout_table)
        self.write(layout + 0x210, self.aux())
        return layout

    def confirm(self, page):
        layout = self.read(page + 0x210)
        # Feed a confirmation event through the real native translation,
        # row/column dispatch and member-function delegate invocation.
        event = self.aux(0x20)
        self.write(event, 1, 4)
        self.invoke(self.base + 0x375aa40, layout, event)

    def frame(self, page):
        self.hook(0x2e9fbb0, page)

    def open(self, page):
        self.confirm(page)
        preview = self.next_preview - 0x1d000
        assert self.read(self.tls + 0xa8) == self.previous_heap
        self.frame(page)
        assert self.read(preview + 0x1f0, 4) == 2
        assert self.read(preview + 0x1c200) == page + 0x8a350
        self.settle(preview, 3)
        self.frame(page)
        assert self.read(preview + 0x264, 4) == self.read(page + 0x218, 4)
        assert self.read(preview + 0x1f0, 4) == 4
        self.settle(preview, 3)
        self.frame(page)
        return preview

    def service(self, u, pc, size, user):
        if not self.running:
            service = self.services.get(pc)
            super().service(u, pc, size, user)
            if service and service[0] == 'install' and self.reg(0) == self.ram + 0x200:
                target = next(reversed(self.hooks)) - self.base
                original = self.ram + 0x500 + len(self.originals) * 0x10
                self.originals[original] = target
                u.reg_write(UC_ARM64_REG_X0, original)
            return
        action = self.native_actions.get(pc - self.base)
        if pc in self.originals:
            action = 'original_' + hex(self.originals[pc])
        if self.services.get(pc, (None,))[0] == 'preview':
            action = self.services[pc][1]
        if action is None:
            if pc in self.services:
                return super().service(u, pc, size, user)
            # Execute real native code, including the recipe-copy/navigation
            # functions and read-only craft-action dispatch.
            return
        args = [self.reg(i) for i in range(4)]
        self.events.append((action, *args))
        result = 0
        if action == 'layout_allocate':
            result = self.aux(args[0])
        elif action == 'allocate':
            assert args[0] == 0x1c210
            assert self.read(self.tls + 0xa8) == self.heap
            if not self.fail_allocation:
                result = self.next_preview
                self.next_preview += 0x1d000
        elif action == 'tls':
            assert args[0] == 7
            result = self.tls
        elif action == 'construct':
            page, name, params = args[:3]
            assert self.string(self.read(name + 8)) == 'WorkCraftInfoSeq'
            assert self.read(params + 8, 4) == 123
            assert self.read(params + 0xc, 2) == 1
            self.write(page + 0x1f8, self.read(params + 0xe, 1), 1)
            self.settle(page, 0)
            self.write(page + 0x1c1c8, 0xffffffff, 4)
            self.write(page + 0x1c1cc, 0xffffffff, 4)
        elif action == 'initialize':
            page = args[0]
            assert self.read(page + 0x48) != 0 and self.read(page + 0x98) != 0
            layout = self.layout()
            self.write(page + 0x1c1f8, layout)
            button = self.aux(0x400)
            self.write(button, self.button_table)
            visibility = self.aux()
            self.write(visibility, self.visibility_table)
            self.write(button + 0x1b0, visibility)
            self.write(layout + 0x598, button)
            entries = self.aux(0x20)
            self.write(layout + 0x468, 1, 4)
            self.write(layout + 0x46c, 1, 4)
            self.write(layout + 0x470, entries)
            self.write(entries + 0x10, self.base + 0x3492fec)
            # Run the compiled vtable override at the native bind boundary.
            u.reg_write(UC_ARM64_REG_PC, self.read(self.read(page) + 0xd0))
            return
        elif action == 'activate_child':
            assert self.read(args[1] + 0x48) == args[0]
            assert self.read(args[1] + 0xa8, 4) == 2
            self.write(args[1] + 0xa8, 3, 4)
            self.settle(args[1], 1)
        elif action == 'show_preview':
            # Model generic UI activation, execute the real preview request.
            u.reg_write(UC_ARM64_REG_PC, self.base + 0x3494090)
            return
        elif action == 'layout_visibility':
            assert args[1] in (0, 1)
        elif action == 'button_visibility':
            assert args[1:3] == [0, 0]
        elif action == 'button_hidden':
            assert args[1] == 0
        elif action == 'hidden':
            result = int(self.hidden)
        elif action == 'reload':
            self.reloads.append(args[1])
        elif action == 'sound':
            assert args[0] == self.context
            self.sounds.append(self.read(args[1], 8))
        elif action == 'remove_child':
            assert self.read(args[1] + 0x48) == args[0]
        elif action.startswith('original_') or action in (
                'lock', 'unlock', 'bind_preview', 'frame_preview', 'initialize_children',
                'render_phone', 'refresh_grid', 'no_animation', 'item_init', 'layout_resources',
                'button_source', 'button_feedback'):
            pass
        else:
            raise AssertionError(action)
        u.reg_write(UC_ARM64_REG_X0, result)
        u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))


def check_flow(base):
    m = PreviewMachine(base)
    phone = m.phone()
    original_index = m.read(phone + 0x218, 4)
    preview = m.open(phone)
    layout = m.read(preview + 0x1c1f8)
    table = m.read(preview)
    native_table = bytes(m.u.mem_read(base + 0x4a3b778, 0x2c0))
    assert table != base + 0x4a3b778
    assert m.invoke(m.read(table + 0x220), preview) == 0
    assert m.invoke(m.read(table + 0x288), preview, 0) == 0
    m.invoke(m.read(table + 0x1a0), preview)

    # Real native list count, left/right wrapping, item lookup and 0xd0-byte
    # recipe copy, including every material slot, use the borrowed app list.
    m.invoke(base + 0x3473aac, preview)
    assert m.read(preview + 0x260, 4) == 3
    output = m.aux(0xd0)
    for direction, expected in ((1, 0), (255, 2), (255, 1), (1, 2)):
        m.invoke(base + 0x34969d0, preview, direction)
        assert m.read(preview + 0x264, 4) == expected
        assert m.reloads[-1] == expected
        assert m.sounds[-1] == 200 + expected
        m.invoke(base + 0x3473c74, preview, expected, output)
        record = m.read(m.read(phone + 0x8a358) + expected * 8)
        # The native copy intentionally omits alignment padding between
        # material records. Compare all semantic bytes, not that padding.
        spans = [(0, 0x2a), (0xbc, 2), (0xc0, 5), (0xc8, 4)]
        spans += [(0x2c + material * 0x18, 0x16) for material in range(6)]
        for offset, size in spans:
            assert bytes(m.u.mem_read(output + offset, size)) == bytes(m.u.mem_read(record + offset, size))
    # The actual native action dispatcher must reject crafting before any
    # inventory, dialog, crafting actor or save routine can run.
    m.write(preview + 0x1c1cc, 2, 4)
    m.invoke(base + 0x3494d00, preview)
    assert m.read(preview + 0x1f0, 8) == 0xffffffffffffffff
    m.invoke(m.read(table + 0x1a0), preview)
    assert m.read(preview + 0x1c1cc, 4) == 0xffffffff

    event = m.aux()
    entries = m.read(layout + 0x470)
    callback = m.read(entries + 0x10)
    assert callback != base + 0x3492fec
    m.write(event + 0x10, 3, 4)
    assert m.invoke(callback, preview, event) == 1
    assert m.read(preview + 0x1c1cc, 4) == 3
    m.write(event + 0x10, 0, 4)
    assert m.invoke(callback, preview, event) == 1
    m.frame(phone)
    assert m.read(preview + 0x1f0, 4) == 5
    m.settle(preview, 5)
    m.hidden = False
    m.frame(phone)
    assert m.read(preview + 0x1f0, 4) == 0xffffffff
    m.hidden = True
    m.frame(phone)
    assert m.read(preview + 0x1f0, 4) == 1
    # Native idle entry clears the borrowed data and selection.
    m.write(preview + 0x1c200, 0)
    m.settle(preview, 1)
    m.frame(phone)
    phone_layout = m.read(phone + 0x210)
    assert m.read(m.read(phone_layout + 0x210) + 0x54, 1) == 0
    assert m.read(phone + 0x218, 4) == original_index
    assert m.read(phone + 0x1b0, 4) == 3
    allocations = len([e for e in m.events if e[0] == 'allocate'])
    m.confirm(phone); m.confirm(phone)
    assert len([e for e in m.events if e[0] == 'allocate']) == allocations
    m.hook(0x2e9d538, phone)
    assert len([e for e in m.events if e[0] == 'remove_child']) == 1
    assert bytes(m.u.mem_read(base + 0x4a3b778, 0x2c0)) == native_table


def check_lists(base):
    for count in (1, 4):
        m = PreviewMachine(base)
        phone = m.phone(count=count)
        array = m.read(phone + 0x8a358)
        records = [m.read(array + i * 8) for i in range(count)]
        # A filtered/sorted view can have arbitrary ordering. Navigation must
        # follow the current pointer array, never recipe IDs or a full catalog.
        for i, record in enumerate(reversed(records)):
            m.write(array + i * 8, record)
        preview = m.open(phone)
        m.invoke(base + 0x3473aac, preview)
        for direction, expected in ((1, 0), (255, count - 1)):
            m.invoke(base + 0x34969d0, preview, direction)
            assert m.read(preview + 0x264, 4) == expected
            assert m.sounds[-1] == 200 + count - 1 - expected
    m = PreviewMachine(base)
    first, second = m.phone(0), m.phone(1)
    first_preview = m.open(first)
    second_preview = m.open(second)
    assert first_preview != second_preview
    assert m.read(first_preview + 0x1c200) == first + 0x8a350
    assert m.read(second_preview + 0x1c200) == second + 0x8a350
    m.hook(0x2e9d538, first)
    assert [e[2] for e in m.events if e[0] == 'remove_child'] == [first_preview]
    m.frame(second)
    assert m.read(second_preview + 0x1c200) == second + 0x8a350
    # A child already removed by native teardown must not be removed twice.
    m.write(second_preview + 0xa8, 2, 4)
    m.hook(0x2e9d538, second)
    assert len([e for e in m.events if e[0] == 'remove_child']) == 1


def check_failures(base):
    for case in ('empty', 'invalid_index', 'unavailable_item', 'allocation', 'busy'):
        m = PreviewMachine(base)
        phone = m.phone()
        if case == 'empty': m.write(phone + 0x8a350, 0, 4)
        if case == 'invalid_index': m.write(phone + 0x218, 99, 4)
        if case == 'unavailable_item':
            record = m.read(m.read(phone + 0x8a358) + 16)
            m.write(record + 8, 0xfffe, 2)
        if case == 'allocation': m.fail_allocation = True
        if case == 'busy': m.write(phone + 0x1f0, 5, 4)
        m.confirm(phone)
        layout = m.read(phone + 0x210)
        assert m.read(m.read(layout + 0x210) + 0x54, 1) == 0
        assert not any(e[0] == 'construct' for e in m.events)
    for offset in HOOKS + GUARDS:
        m = Machine(base, {})
        m.write(base + offset, 0, 4)
        m.boot(); m.activate()
        assert 'DIY preview: DISABLED' in m.text
        assert not any(base + hook in m.hooks for hook in HOOKS)
        assert base + 0xbc4144 in m.hooks


if __name__ == '__main__':
    for base in (0x7100000000, 0x7200000000):
        check_flow(base)
        check_lists(base)
        check_failures(base)
    print('PASS: native 35-row layout construction and confirmation dispatch, Core phone preview lifecycle, borrowed recipe/materials, native navigation/wrap, craft rejection, B return, reuse, teardown and fail-closed fingerprints at two ASLR bases')
