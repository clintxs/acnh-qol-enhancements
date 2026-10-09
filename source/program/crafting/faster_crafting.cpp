#include "lib.hpp"
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
constexpr unsigned RampFrames = 60;

float AutomaticRate(unsigned elapsedFrames) {
    // Sample 1 + 2 * log(1 + 9*t) / log(10), t = frame/60. A small constant
    // table avoids a runtime math-library dependency in the injected module.
    // Starts at 1x, rises quickly, then eases into the native maximum of 3x.
    static constexpr float rates[RampFrames + 1]{
        1.000000000f, 1.121395681f, 1.227886705f, 1.322736004f, 1.408239965f,
        1.486076097f, 1.557507202f, 1.623507722f, 1.684845362f, 1.742135725f,
        1.795880017f, 1.846491748f, 1.894316063f, 1.939644032f, 1.982723388f,
        2.023766722f, 2.062957834f, 2.100456706f, 2.136403448f, 2.170921459f,
        2.204119983f, 2.236096193f, 2.266936911f, 2.296720022f, 2.325515663f,
        2.353387219f, 2.380392160f, 2.406582756f, 2.432006687f, 2.456707564f,
        2.480725379f, 2.504096896f, 2.526855987f, 2.549033931f, 2.570659670f,
        2.591760035f, 2.612359948f, 2.632482600f, 2.652149605f, 2.671381143f,
        2.690196080f, 2.708612084f, 2.726645720f, 2.744312545f, 2.761627185f,
        2.778603405f, 2.795254183f, 2.811591761f, 2.827627705f, 2.843372951f,
        2.858837851f, 2.874032215f, 2.888965344f, 2.903646071f, 2.918082785f,
        2.932283465f, 2.946255707f, 2.960006743f, 2.973543469f, 2.986872461f,
        3.000000000f,
    };
    return rates[elapsedFrames < RampFrames ? elapsedFrames : RampFrames];
}

HOOK_DEFINE_INLINE(AutomaticMashBoost) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        // Native state entry resets this per-actor counter, and the update
        // wrappers increment it only when not suspended/busy. Reading it keeps
        // each action's ramp independent without another hook or cached actor.
        // Replace only the rate: input/mash counters, recipe readiness and
        // native exit resets remain intact. Manual mashing works when disabled.
        const auto actor = registers->X[19];
        const auto elapsed = *reinterpret_cast<const unsigned *>(actor + ActorStateFrames);
        registers->S[0] = AutomaticRate(elapsed);
    }
};
} // namespace

bool Install() {
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
            Logging.Log("[QoL Enhancements/Faster Crafting] DISABLED: instruction mismatch at main+0x%lx\n", expected.offset);
            return false;
        }
    }
    AutomaticMashBoost::InstallAtOffset(address::CraftingMashRate);
    AutomaticMashBoost::InstallAtOffset(address::CookingMashRate);
    Logging.Log("[QoL Enhancements/Faster Crafting] READY: C++ hooks; logarithmic crafting/cooking ramp=1..3x over %u updates\n", RampFrames);
    return true;
}
} // namespace qol::crafting
