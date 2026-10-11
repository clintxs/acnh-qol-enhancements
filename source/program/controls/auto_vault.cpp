#include "lib.hpp"
#include "auto_vault.hpp"
#include <cstring>

namespace qol::controls {
namespace {
namespace address {
constexpr std::uintptr_t HopGeometry = 0x0230c470;
constexpr std::uintptr_t HopEntry = 0x0230b8d0, HopDirection = 0x0230bf18;
constexpr std::uintptr_t TryVault = 0x0239a340, PrepareVault = 0x0239aff4;
constexpr std::uintptr_t EquippedToolKind = 0x015f5c40, PlayerParameters = 0x0241e98c;
constexpr std::uintptr_t MovementProbe = 0x0221a094, HopCommit = 0x02223454;
}
namespace field {
constexpr std::size_t HopTimer = 0x6a0, JudgeTimerParameter = 0xed0;
constexpr std::size_t ParameterValueMethod = 0xf0;
}
constexpr unsigned VaultingPoleKind = 0x28;
constexpr unsigned NoCrossing = 0, CrossingReady = 1, Waiting = 2;
std::uintptr_t Main{};
template<class T> T Read(const void *object, std::size_t offset = 0) {
    T value;
    std::memcpy(&value, static_cast<const std::byte *>(object) + offset, sizeof(value));
    return value;
}
template<class F> F Native(std::uintptr_t offset) { return reinterpret_cast<F>(Main + offset); }

// Shared native hop geometry request, constructed by HopDirection. The first
// byte selects a check at the predicted position; automatic distinguishes the
// held-stick path from explicit A/tool actions. Skewed candidates and output
// probes belong to the native hop's alternate landing search, not our vault.
struct HopRequest {
    unsigned char probe;
    unsigned char padding[3];
    unsigned direction;
    unsigned char automatic;
    unsigned char padding2[3];
    float skewDegrees;
    unsigned char alternate;
};
static_assert(offsetof(HopRequest, automatic) == 8);
static_assert(offsetof(HopRequest, skewDegrees) == 0xc);
static_assert(offsetof(HopRequest, alternate) == 0x10);

// Only a synchronous automatic HopEntry may extend a failed native search.
// Recording intent at geometry entry retains the upstream contact/direction
// guards without copying them, while letting ALL native landing alternatives
// finish before we consider a pole vault. Like the existing pickup scope,
// this relies on the player dispatcher running synchronously on the game thread.
struct HopAttempt { void *player; bool eligible = false; };
HopAttempt *CurrentAttempt{};

bool HoldComplete(void *player) {
    auto parameters = Native<void *(*)(unsigned)>(address::PlayerParameters)(1);
    auto parameter = static_cast<std::byte *>(parameters) + field::JudgeTimerParameter;
    auto vtable = Read<const void *>(parameter);
    auto getter = Read<const unsigned *(*)(void *)>(vtable, field::ParameterValueMethod);
    // Match the native strict comparison: the default threshold is 30, and
    // a counter equal to the threshold must still return Waiting.
    return Read<unsigned short>(player, field::HopTimer) > *getter(parameter);
}

HOOK_DEFINE_TRAMPOLINE(AutoVaultGeometry) {
    static unsigned Callback(void *player, const HopRequest *request, void *output) {
        if (CurrentAttempt && CurrentAttempt->player == player && request->automatic &&
            request->skewDegrees == 0.0f && !output)
            CurrentAttempt->eligible = true;
        return Orig(player, request, output);
    }
};
HOOK_DEFINE_TRAMPOLINE(AutoVaultEntry) {
    static unsigned Callback(void *player, unsigned probe, unsigned automatic) {
        if (!(automatic & 1)) return Orig(player, probe, automatic);
        HopAttempt attempt{player};
        auto previous = CurrentAttempt;
        CurrentAttempt = &attempt;
        const auto hop = Orig(player, probe, automatic);
        CurrentAttempt = previous;
        // Keep the complete ordinary-hop search, including adjusted landings
        // and its pending hold, first. Eligibility was observed only AFTER
        // native stick strength, low speed, contact and direction checks.
        if (hop != NoCrossing || !attempt.eligible ||
            Native<unsigned (*)(void *)>(address::EquippedToolKind)(player) != VaultingPoleKind)
            return hop;

        auto vault = Native<unsigned (*)(void *, unsigned)>(address::TryVault);
        // The native validator first checks the short-hop path with automatic
        // false, so recursive geometry calls cannot extend themselves here.
        // Probe=1 skips the reservation/state transition in PrepareVault.
        if (!(vault(player, 1) & 1)) return NoCrossing;
        if (!HoldComplete(player)) return Waiting;
        if (probe & 1) return CrossingReady;

        // Re-run native validation against the current position and let the
        // game reserve landing space and enter LongStickJumpReady itself.
        const auto result = vault(player, 0) & 1;
        if (result) Logging.Debug("[QoL Enhancements/Controls] auto vault: native traversal requested\n");
        return result;
    }
};
}

bool InstallAutoVault() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
        {address::HopGeometry, {0xd10783ff,0x6d143bef,0x6d1533ed,0x6d162beb,0x6d1723e9}},
        {address::HopEntry, {0xd10243ff,0x6d0523e9,0xa9067bfd,0xa90757f6,0xa9084ff4}},
        {address::HopDirection, {0xd10643ff,0xa9137bfd,0xa9146ffc,0xa91567fa,0xa9165ff8}},
        {address::TryVault, {0xd10443ff,0x6d073bef,0x6d0833ed,0x6d092beb,0x6d0a23e9}},
        {address::PrepareVault, {0x36000062,0x52800020,0xd65f03c0,0xd10343ff,0xfd0033e8}},
        {address::EquippedToolKind, {0xd10183ff,0xa9037bfd,0xa90457f6,0xa9054ff4,0x9100c3fd}},
        {address::PlayerParameters, {0xd100c3ff,0xa9017bfd,0xf90013f3,0x910043fd,0x7103ac1f}},
        {address::MovementProbe, {0xaa1303e0,0x52800021,0x52800022,0x9403c60c,0xb90003e0}},
        {address::HopCommit, {0xaa1303e0,0x2a1f03e1,0x52800022,0x52800035,0x9403a11b}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &f : fingerprints) if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + f.offset), f.words, sizeof(f.words))) {
        Logging.Error("[QoL Enhancements/Controls] auto vault: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        return false;
    }
    Main = main.m_Text.m_Start;
    AutoVaultGeometry::InstallAtOffset(address::HopGeometry);
    AutoVaultEntry::InstallAtOffset(address::HopEntry);
    Logging.Debug("[QoL Enhancements/Controls] auto vault: READY; prototype 1; equipped pole; native hop timer\n");
    return true;
}
}
