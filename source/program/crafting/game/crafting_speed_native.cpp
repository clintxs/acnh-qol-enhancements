#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "../settings.hpp"
#include "../features/crafting.hpp"
#include <cstring>

namespace qol::crafting {
namespace {
namespace address {
// Immediately before storing the computed rate in S0. Both paths have already
// excluded non-local player actors; crafting's readiness check still follows.
constexpr std::uintptr_t CraftingMashRate = 0x022fc6bc;
constexpr std::uintptr_t CookingMashRate = 0x02279a14;
} // namespace address
constexpr std::uintptr_t ActorStateFrames = 0x1c04;
constexpr unsigned RampFrames = features::CraftingRampFrames;
unsigned MaximumSpeed = 3;

HOOK_DEFINE_INLINE(AutomaticMashBoost) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        // Native state entry resets this per-actor counter, and the update
        // wrappers increment it only when not suspended/busy. Reading it keeps
        // each action's ramp independent without another hook or cached actor.
        // Replace only the rate: input/mash counters, recipe readiness and
        // native exit resets remain intact. Manual mashing works when disabled.
        const auto actor = registers->X[19];
        const auto elapsed = *reinterpret_cast<const unsigned *>(actor + ActorStateFrames);
        registers->S[0] = features::CraftingRate(elapsed, MaximumSpeed);
    }
};
} // namespace

bool InstallFasterCrafting(unsigned maximumSpeed) {
    // Zero preserves the native rate calculation, including manual A mashing.
    // Do not validate or install speed hooks when this feature is disabled.
    if (maximumSpeed == 0) return true;
    if (maximumSpeed > MaximumCraftingSpeed) return false;
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    // Original instructions only; behavior is implemented by the C++ callback.
    constexpr Fingerprint fingerprints[]{
        {address::CraftingMashRate, {0xbd192a60, 0xf9401108, 0x34000089, 0xf000e3e9, 0x913aed29}},
        {address::CookingMashRate, {0xbd192260, 0xf9403274, 0xb50000f4, 0xb0017668, 0x91070108}},
        // Guard local-player/input checks as well as the two hook sites.
        {0x022fc63c, {0x5282f588, 0xaa0003f3, 0x8b080015, 0xb9417808, 0x35000528}},
        {0x02279998, {0x5282f588, 0xaa0003f3, 0x8b080015, 0xb9417808, 0x34000088}},
        // Common state-entry reset and both registered update increments.
        {0x022758a8, {0xb91c067f, 0xb00141f6, 0xb91d9a69, 0xf943fed6, 0x37900428}},
        {0x022a6bec, {0xb95c0668, 0x11000508, 0xb91c0668, 0xf9400bf3, 0xa8c27bfd}},
        {0x022b1830, {0xb95c0668, 0x11000508, 0xb91c0668, 0xf9400bf3, 0xa8c27bfd}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &expected : fingerprints) {
        if (expected.offset + sizeof(expected.words) > main.m_Text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + expected.offset),
                        expected.words, sizeof(expected.words))) {
            Logging.Error("[QoL Enhancements/Crafting] DISABLED: instruction mismatch at main+0x%lx\n", expected.offset);
            return false;
        }
    }
    MaximumSpeed = maximumSpeed;

    qol::core::HookPlan hookPlan{
        qol::core::Site<AutomaticMashBoost>("AutomaticMashBoost", address::CraftingMashRate, {0xbd192a60, 0xf9401108, 0x34000089, 0xf000e3e9, 0x913aed29}, true),
        qol::core::Site<AutomaticMashBoost>("AutomaticMashBoost", address::CookingMashRate, {0xbd192260, 0xf9403274, 0xb50000f4, 0xb0017668, 0x91070108}, true),
    };
    if (!hookPlan) return false;
    if (!hookPlan.Publish<AutomaticMashBoost>(address::CraftingMashRate)) return false;
    if (!hookPlan.Publish<AutomaticMashBoost>(address::CookingMashRate)) return false;
    Logging.Debug("[QoL Enhancements/Crafting] READY: C++ hooks; logarithmic crafting/cooking ramp=1..%ux over %u updates\n", MaximumSpeed, RampFrames);
    return true;
}
} // namespace qol::crafting
