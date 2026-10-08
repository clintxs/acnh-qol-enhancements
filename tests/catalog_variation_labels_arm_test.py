"""Compiled variation scopes/setters with real read-only catalog queries.

Native UI controllers and font/render resources are service doubles. The
native grouped dispatcher, typed pane setter and UTF-16 buffer copy execute
real game code; the final drawing step is not run. The test does not load
layouts or render Eden; it verifies item/body selection, binding isolation,
text lifetime/length, original-call preservation and scope cleanup.
"""
import struct
from catalog_variants_arm_test import Variants, furniture
from catalog_stars_arm_test import symbol, ids, symbols
from unicorn import UC_HOOK_CODE, UC_PROT_READ
from unicorn.arm64_const import *


def setter_callback(name):
    matches = [address for symbol_name, address in symbols.items()
               if symbol_name.endswith(name + '8CallbackEmPKDsj')]
    assert len(matches) == 1
    return matches[0]


def move(register, value):
    return [0xd2800000 | ((value & 0xffff) << 5) | register] + [
        0xf2800000 | (shift << 21) | (((value >> (shift * 16)) & 0xffff) << 5) | register
        for shift in range(1, 4)]


class Labels(Variants):
    def __init__(self, base):
        super().__init__(base)
        self.ui = base + 0x23000000
        self.u.mem_map(self.ui, 0x80000)
        self.controller = self.ui
        self.page = self.ui + 0x60000
        self.info = self.ui + 0x61000
        self.binding = self.ui + 0x62100
        self.adapter = self.ui + 0x62200
        self.methods = self.ui + 0x63000
        self.group = self.ui + 0x64000
        self.panes = [self.ui + 0x65000, self.ui + 0x65200]
        self.source = self.ui + 0x68000
        self.original_update = self.ui + 0x70000
        self.original_text = base + 0x3c1b588
        self.reset_stub = self.ui + 0x73000
        self.captured = []
        self.update_calls = 0
        self.u.mem_write(self.reset_stub, bytes.fromhex('c0035fd6'))
        for offset in (0x210, 0x220): self.write(self.controller + offset, self.page)
        for offset in (0x5d8, 0x580): self.write(self.page + offset, self.info)
        self.write(self.info + 0x220, self.ui + 0x62000)
        self.write(self.ui + 0x62000, self.binding)
        self.write(self.binding + 0x48, self.adapter)
        self.write(self.adapter + 0x18, self.methods)
        for shop in ('Nook', 'Hhp'):
            self.write(self.mod + symbol(shop + 'VariationLabel', 's_FnPtr'), self.original_update)
        self.write(self.mod + symbol('VariationPaneText', 's_FnPtr'), self.original_text)
        self.write(self.mod + symbol('VariationGroupText', 's_FnPtr'), base + 0x3c47b90)
        # Native grouped setter dispatches twice to the compiled pane hook.
        members = [self.group + 0x100, self.group + 0x200]
        self.write(self.group, members[-1] + 0x28)
        self.write(self.group + 8, members[0] + 0x28)
        self.write(self.group + 0x10, 2, 4)
        self.write(self.group + 0x14, 0x28, 4)
        member_methods = self.ui + 0x66000
        self.write(member_methods + 0x5e0, self.mod + setter_callback('VariationPaneText'))
        for i, member in enumerate(members):
            self.write(member + 0x10, self.panes[i])
            self.write(member + 0x18, member_methods)
            self.write(member + 0x28, self.group if i == 0 else members[0] + 0x28)
            self.write(member + 0x30, members[1] + 0x28 if i == 0 else self.group)
        # The native RGUI UTF-16 setter (03a0ee68) calls TextBox::SetString
        # 0028a714, which copies into +d8, updates length/dirty flags and retains
        # no source pointer. Substitute only the subsequent presentation reset.
        vtable = self.ui + 0x67000
        self.write(vtable + 0x150, base + 0x3a0ee68)
        self.write(vtable + 0x168, self.reset_stub)
        self.native_buffers = {}
        for index, pane in enumerate(self.panes + [self.ui + 0x65400]):
            destination = self.ui + 0x74000 + index * 0x1000
            self.native_buffers[pane] = destination
            self.write(pane, vtable)
            self.write(pane + 0xd8, destination)
            self.write(pane + 0xf0, 1) # nonnull font; copy does not dereference it
            self.write(pane + 0x110, 512, 2)
        self.u.mem_protect(self.source, 0x1000, UC_PROT_READ)
        self.u.hook_add(UC_HOOK_CODE, self.service)

    def hook(self, u, pc, size, data):
        if pc == self.base + 0x3f04470:
            u.mem_write(self.reg(0), bytes(u.mem_read(self.reg(1), self.reg(2))))
            u.reg_write(UC_ARM64_REG_PC, u.reg_read(UC_ARM64_REG_LR))
            return
        super().hook(u, pc, size, data)

    def service(self, u, pc, size, _):
        if pc == self.original_update: self.update_calls += 1
        if pc == self.original_text:
            length = self.reg(2)
            data = bytes(u.mem_read(self.reg(1), (length + 1) * 2))
            assert data[-2:] == b'\0\0'
            self.captured.append((self.reg(0), data[:-2].decode('utf-16-le'), length))

    def set_render(self, grouped, text, unknown_adapter=False, mismatch=False):
        self.grouped = grouped
        self.target = self.group if grouped else self.panes[0]
        self.write(self.adapter + 8, self.target)
        setter = self.base + (0x3c47b90 if grouped else 0x3c1b588)
        self.write(self.methods + 0x5e0, 0 if unknown_adapter else setter)
        self.text = text
        data = text.encode('utf-16-le') + b'\0\0'
        self.u.mem_write(self.source, data)
        self.source_data = data
        length = len(data) // 2 - 1
        target_callback = self.mod + setter_callback('VariationGroupText' if grouped else 'VariationPaneText')
        pane_callback = self.mod + setter_callback('VariationPaneText')
        # Native label updates clear old text, then submit the formatted label.
        # Include another target and repeated submission to detect overreach.
        calls = [(target_callback, self.target, self.source + length * 2, 0),
                 (pane_callback, self.ui + 0x65400, self.source, length),
                 (target_callback, self.target, self.source, length),
                 (target_callback, self.target, self.source, length)]
        if mismatch: self.write(self.adapter + 8, self.target + 0x800)
        words = [0xa9bf7bfd, 0x910003fd] # save frame/LR
        for function, target, source, count in calls:
            for reg, value in ((0, target), (1, source), (2, count), (16, function)):
                words += move(reg, value)
            words += [0xd63f0200] # blr x16
        words += [0xa8c17bfd, 0xd65f03c0]
        self.u.mem_write(self.original_update, struct.pack('<' + 'I' * len(words), *words))
        self.u.ctl_remove_cache(self.original_update, self.original_update + len(words) * 4)

    def check(self, shop, item, remake, body, known_body, main_known, expected,
              player=0, hhp_body=None, extra=0):
        bits = bytearray(0x754)
        if main_known: bits[item // 8] |= 1 << (item % 8)
        self.u.mem_write(self.bits, bytes(bits))
        before_history = self.history(remake, known_body)
        original_item = struct.pack('<HBBI', item, 0, 0, extra | body)
        self.u.mem_write(self.item, original_item)
        self.write(self.controller + 0x1f8, player, 1)
        self.write(self.controller + 0x574f8, body if hhp_body is None else hhp_body, 4)
        self.captured.clear()
        before = self.update_calls
        self.u.reg_write(UC_ARM64_REG_X0, self.controller)
        self.u.reg_write(UC_ARM64_REG_X1, self.item)
        self.call(self.mod + symbol(shop + 'VariationLabel', 'Callback'))
        assert self.update_calls == before + 1
        active_panes = self.panes if self.grouped else self.panes[:1]
        expected_text = '☆ ' + self.text if expected else self.text
        for pane in active_panes:
            texts = [text for target, text, count in self.captured if target == pane]
            assert texts == ['', expected_text, expected_text], (shop, body, texts, self.logs[-2:])
            expected_bytes = expected_text.encode('utf-16-le') + b'\0\0'
            destination = self.native_buffers[pane]
            assert bytes(self.u.mem_read(destination, len(expected_bytes))) == expected_bytes
            # Tagged input can have a native embedded NUL: compare the buffer,
            # but assert its recorded length only for plain labels.
            if '\0' not in expected_text:
                assert int.from_bytes(self.u.mem_read(pane + 0x112, 2), 'little') == len(expected_bytes) // 2 - 1
        assert [text for target, text, count in self.captured if target == self.ui + 0x65400] == [self.text]
        assert bytes(self.u.mem_read(self.item, 8)) == original_item
        assert bytes(self.u.mem_read(self.bits, len(bits))) == bytes(bits)
        assert bytes(self.u.mem_read(self.variants, 2000)) == before_history
        assert bytes(self.u.mem_read(self.source, len(self.source_data))) == self.source_data
        # An identical setter call after returning must see no catalog scope.
        self.captured.clear()
        self.u.reg_write(UC_ARM64_REG_X0, self.panes[0])
        self.u.reg_write(UC_ARM64_REG_X1, self.source)
        self.u.reg_write(UC_ARM64_REG_X2, len(self.source_data) // 2 - 1)
        self.call(self.mod + setter_callback('VariationPaneText'))
        assert self.captured[0][1] == self.text


def main():
    count = 0
    item, remake, variants = furniture[0]
    assert variants >= 2
    for base in (0x7100000000, 0x7200000000):
        q = Labels(base)
        for shop in ('Nook', 'Hhp'):
            for grouped in (False, True):
                q.set_render(grouped, 'Silver')
                for body, known_body, main_known, marked in (
                        (0, None, False, True), (0, 0, True, False),
                        (1, 0, True, True), (1, 1, True, False), (0, 1, True, True)):
                    q.check(shop, item, remake, body, known_body, main_known, marked); count += 1
                q.check(shop, item, remake, 0, None, False, False, player=8); count += 1
                q.check(shop, ids[0][0], remake, 0, None, False, False); count += 1 # wallpaper
                for text, marked in [('Écru', True), ('銀色', True), ('☆ Silver', False),
                                     ('★ Silver', False), ('\x0eTagged', False),
                                     ('a' * 253, True), ('a' * 254, False), ('', False)]:
                    q.set_render(grouped, text)
                    q.check(shop, item, remake, 0, None, False, marked); count += 1
                for kw in ({'unknown_adapter': True}, {'mismatch': True}):
                    q.set_render(grouped, 'Silver', **kw)
                    q.check(shop, item, remake, 0, None, False, False); count += 1
                q.set_render(grouped, 'Silver')
                # Body selection uses the HHP controller, not a stale item body.
                if shop == 'Hhp':
                    q.check(shop, item, remake, 0, 0, True, True, hhp_body=1, extra=0x100); count += 1
                    q.check(shop, item, remake, 1, 0, True, False, hhp_body=0); count += 1
                    q.check(shop, item, remake, 0, None, False, False, hhp_body=8); count += 1
    print(f'PASS: {count} variation-label cases; two bases, direct/grouped bindings, native group dispatch, scope cleanup and read-only history')

if __name__ == '__main__': main()
