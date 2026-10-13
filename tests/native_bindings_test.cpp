#include "program/core/native/executable_view.hpp"
#include "program/pockets/game/build_profile.hpp"
#include "program/catalog/game/build_profile.hpp"
#include <cassert>
#include <iostream>
using namespace qol::native;
int main() {
    constexpr Fingerprint<> check{4, {1, 2, 3, 4, 5}, "test.binding"};
    // Relocation does not change module-relative validation. Unaligned buffers
    // also work: executable reads must not rely on a uint32_t pointer cast.
    for (unsigned displacement : {0, 1, 7, 32}) {
        std::array<std::byte, 64> data{};
        auto *text = data.data() + displacement;
        std::memcpy(text + check.offset, check.words.data(), sizeof(check.words));
        const auto base = reinterpret_cast<std::uintptr_t>(text);
        ExecutableView view(base, 24);
        const auto good = view.Check(check);
        assert(good && good.actualReadable && good.wordCount == 5);
        assert(good.expected == good.actual && good.binding == check.name && good.offset == 4);
        for (std::size_t size = 0; size < 24; ++size) {
            const auto shortRead = ExecutableView(base, size).Check(check);
            assert(!shortRead && shortRead.status == CodeStatus::OutOfRange);
            assert(!shortRead.actualReadable && shortRead.expected == good.expected);
        }
        assert(view.Contains(24, 0) && !view.Contains(24, 1));
        assert(!view.Contains(25, 0));
        for (std::size_t word = 0; word < 5; ++word) {
            auto expected = check;
            expected.words[word] = 0xdeadbeef;
            const auto bad = view.Check(expected);
            assert(!bad && bad.status == CodeStatus::Mismatch && bad.actualReadable);
            assert(bad.expected[word] == 0xdeadbeef && bad.actual[word] == word + 1);
        }
        constexpr FunctionBinding<void (*)()> function{"test.function", 4};
        assert(reinterpret_cast<std::uintptr_t>(function.Resolve(base)) == base + 4);
        assert(!qol_pockets::game::ValidateExpectedCode(view));
        assert(!catalog_aware_shops::game::ValidateExpectedCode(view));
    }
    constexpr auto max = std::numeric_limits<std::uintptr_t>::max();
    assert(!ExecutableView(0, max).Check(check));
    assert(!ExecutableView(max - 3, 32).Check(check));
    assert(!ExecutableView(1, max).Contains(max, 1));
    assert(!ExecutableView(1, max).Contains(8, max));
    auto overflow = check;
    overflow.offset = max - 2;
    assert(!ExecutableView(1, max).Check(overflow));
    // Invalid sentinel addresses are never read on an out-of-range path.
    assert(!ExecutableView(max, 0).Check(check));
    std::cout << "PASS: bounded code reads, truncation, exact boundaries, overflow, mismatches, relocation and profile guards\n";
}
