#include "lib.hpp"
#include <cstring>

namespace qol::crafting {
namespace {
namespace address {
// Immediately after loading each state's mash-A counter, before native 1..3
// clamping. Both paths have already excluded non-local player actors.
constexpr std::uintptr_t CraftingMashRate = 0x022fc694;
constexpr std::uintptr_t CookingMashRate = 0x022799fc;
} // namespace address
constexpr unsigned MaximumMashBoost = 3;

HOOK_DEFINE_INLINE(AutomaticMashBoost) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        // Supply the native maximum to its existing calculation. Do not write
        // the counter or controller state: no synthetic A presses can confirm
        // menus, and disabling this module restores ordinary manual boosting.
        // Crafting's native recipe/animation gate can still reset the rate to
        // 1, and native state teardown restores normal animation playback.
        registers->W[8] = MaximumMashBoost;
    }
};
} // namespace

bool Install() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    // Original instructions only; behavior is implemented by the C++ callback.
    constexpr Fingerprint fingerprints[]{
        {address::CraftingMashRate, {0x1e211002, 0x5283dd0a, 0x3945eaa9, 0x8b0a0260, 0x1e220101}},
        {address::CookingMashRate, {0x1e211002, 0x1e220101, 0x1e222020, 0x1e21cc41, 0x7100051f}},
        // Guard local-player/input checks as well as the two hook sites.
        {0x022fc63c, {0x5282f588, 0xaa0003f3, 0x8b080015, 0xb9417808, 0x35000528}},
        {0x02279998, {0x5282f588, 0xaa0003f3, 0x8b080015, 0xb9417808, 0x34000088}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &expected : fingerprints) {
        if (expected.offset + sizeof(expected.words) > main.m_Text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + expected.offset),
                        expected.words, sizeof(expected.words))) {
            Logging.Log("[QoL Enhancements/Faster Crafting] DISABLED: instruction mismatch at main+0x%lx\n", expected.offset);
            return false;
        }
    }
    AutomaticMashBoost::InstallAtOffset(address::CraftingMashRate);
    AutomaticMashBoost::InstallAtOffset(address::CookingMashRate);
    Logging.Log("[QoL Enhancements/Faster Crafting] READY: C++ hooks; automatic crafting/cooking boost=%ux\n", MaximumMashBoost);
    return true;
}
} // namespace qol::crafting
