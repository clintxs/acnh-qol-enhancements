#pragma once
#include "executable_view.hpp"

namespace qol::native {
// ACNH 3.0.3 SpeedFix v1.4.1. These stable sites are outside our hook spans.
// Verify the redirected menu predicate as well as the model-rate marker; a
// matching FPS constant alone does not establish compatible native behavior.
inline unsigned SpeedFixFrequency(const ExecutableView &text) {
    constexpr Fingerprint<3> menuTail{0x03090d04,
        {0x54fdc58b, 0x7100051f, 0x17ffee24}, "speedfix.menu_tail"};
    if (!text.Check(menuTail)) return 30;
    constexpr Fingerprint<3> menu2{0x03088644,
        {0xd360fd09, 0x7100093f, 0x140021ae}, "speedfix.menu_45_60"};
    constexpr Fingerprint<3> menu4{0x03088644,
        {0xd360fd09, 0x7100113f, 0x140021ae}, "speedfix.menu_120"};
    if (text.Check(menu2)) {
        if (text.Check(Fingerprint<1>{0x0374f700, {0x1cd7a868}, "speedfix.rate_45"})) return 45;
        if (text.Check(Fingerprint<1>{0x0374f700, {0x1e2c1008}, "speedfix.rate_60"})) return 60;
    }
    if (text.Check(menu4) &&
        text.Check(Fingerprint<1>{0x0374f700, {0x1e2a1008}, "speedfix.rate_120"})) return 120;
    return 30;
}

// Translate only two complete, audited vanilla fingerprints. Reserve the
// actual patched bytes; Controls retains the changed timer decrement in its
// trampoline. Pockets uses an adapter for the menu helper's external back-edge.
// Never restore vanilla instructions over either SpeedFix edit.
template<std::size_t N>
Fingerprint<N> CompatibleFingerprint(const ExecutableView &text, Fingerprint<N> expected) {
    if constexpr (N == 5) {
        constexpr std::array<std::uint32_t, 5> menu{
            0xf9410c08, 0xb941b108, 0x7100051f, 0x540000c1, 0x39480008};
        constexpr std::array<std::uint32_t, 5> advance{
            0xb964666b, 0x37f801ab, 0x3400024b, 0x5100096c, 0x7100057f};
        if ((expected.offset != 0x0308c590 || expected.words != menu) &&
            (expected.offset != 0x01afa530 || expected.words != advance)) return expected;
        // Prefer unmodified code; mixed vanilla/SpeedFix instructions still fail.
        if (text.Check(expected)) return expected;
        const auto fps = SpeedFixFrequency(text);
        if (fps == 30) return expected;
        if (expected.offset == 0x0308c590) {
            expected.words[1] = 0xf940d908;
            expected.words[2] = 0x17fff02b;
        } else {
            expected.words[3] = fps == 45 ? 0x5100116c : 0x5100056c;
            expected.words[4] = fps == 45 ? 0x7100117f : 0x7100057f;
        }
    }
    return expected;
}
template<std::size_t N>
CodeResult CheckCompatibleCode(const ExecutableView &text, Fingerprint<N> expected) {
    return text.Check(CompatibleFingerprint(text, expected));
}
} // namespace qol::native
