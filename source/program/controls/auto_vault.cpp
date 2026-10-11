#include "lib.hpp"
#include "auto_vault.hpp"
#include <cstring>
#include <array>

namespace qol::controls {
namespace {
namespace address {
constexpr std::uintptr_t HopGeometry = 0x0230c470;
constexpr std::uintptr_t HopEntry = 0x0230b8d0, HopDirection = 0x0230bf18;
constexpr std::uintptr_t TryVault = 0x0239a340, PrepareVault = 0x0239aff4;
constexpr std::uintptr_t EquippedToolKind = 0x015f5c40, PlayerParameters = 0x0241e98c;
constexpr std::uintptr_t VaultPositionSelection = 0x0239a398;
constexpr std::uintptr_t VaultCandidateRejected = 0x0239a6c0, VaultSearchExit = 0x0239a5cc;
constexpr std::uintptr_t MovementGuards = 0x02219fc0;
constexpr std::uintptr_t MovementProbe = 0x0221a094, HopCommit = 0x02223454;
}
namespace field {
constexpr std::size_t Actor = 0x68, PlayerContext = 0x60, ActionController = 0x70;
constexpr std::size_t Position = 0x278, AlternatePosition = 0x284, Facing = 0x2b4;
constexpr std::size_t Action = 0x280, MovementState = 0x1810, InputValid = 0x12f8;
constexpr std::size_t StickX = 0xeec, StickY = 0xef0, Speed = 0x2e0;
constexpr std::size_t HopTimer = 0x6a0, JudgeTimerParameter = 0xed0;
constexpr std::size_t Resident = 0x1ee8, PlayerHandle = 0x230;
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
struct HopAttempt { void *player; bool eligible = false; unsigned geometry = 0; };
HopAttempt *CurrentAttempt{};

// Movement tests a tentative transform before collision correction. Transfer
// one stable-phase validation to the following movement timer check, requiring
// the same player/handle and EXACT starting bank position. The native intent
// guards still run in both phases, and every actual jump is validated afresh.
struct MovementProof {
    void *player{};
    unsigned handle{};
    std::array<float, 3> bank{};
    bool valid = false;
};
std::array<MovementProof, 8> MovementProofs{};
MovementProof *ProofFor(void *player) {
    const auto resident = Read<unsigned char>(player, field::Resident);
    return resident < MovementProofs.size() ? &MovementProofs[resident] : nullptr;
}
MovementProof ConsumeProof(void *player) {
    if (auto proof = ProofFor(player)) {
        const auto result = *proof;
        *proof = {};
        return result;
    }
    return {};
}
bool MatchesMovementStart(const MovementProof &proof, void *player) {
    auto actor = Read<void *>(player, field::Actor);
    if (!proof.valid || proof.player != player || !actor ||
        proof.handle != Read<unsigned>(player, field::PlayerHandle)) return false;
    for (unsigned i = 0; i < proof.bank.size(); ++i)
        if (proof.bank[i] != Read<float>(actor, field::AlternatePosition + i*sizeof(float))) return false;
    return true;
}
void RememberBank(void *player) {
    if (auto proof = ProofFor(player)) {
        proof->player = player;
        proof->handle = Read<unsigned>(player, field::PlayerHandle);
        auto actor = Read<void *>(player, field::Actor);
        for (unsigned i = 0; i < proof->bank.size(); ++i)
            proof->bank[i] = Read<float>(actor, field::Position + i*sizeof(float));
        proof->valid = true;
    }
}

// TryVault's argument combines position selection with side-effect control.
// Use the current-position path, suppress its redundant short-hop attempt,
// and make ONLY its final preparation check-only. No player positions change.
struct VaultProbe {
    void *player;
    bool landingValidated = false, transferred = false;
    unsigned nativeResult = 0;
#if CAS_AUTO_VAULT_DIAGNOSTIC
    unsigned failedCandidates = 0, failureMask = 0, lastCall = 0, integerResult = 0, floatBits = 0;
#endif
};
VaultProbe *CurrentVaultProbe{};
HOOK_DEFINE_TRAMPOLINE(AutoVaultPrepare) {
    static unsigned Callback(void *player, const void *position, unsigned probe) {
        auto scope = CurrentVaultProbe;
        const bool checkOnly = scope && scope->player == player;
        const auto result = Orig(player, position, probe | unsigned(checkOnly));
        if (checkOnly && (result & 1)) scope->landingValidated = true;
        return result;
    }
};
VaultProbe ProbeCurrentPositionVault(void *player) {
    VaultProbe probe{player};
    auto previous = CurrentVaultProbe;
    CurrentVaultProbe = &probe;
    const auto result = Native<unsigned (*)(void *, unsigned)>(address::TryVault)(player, 0);
    CurrentVaultProbe = previous;
    probe.nativeResult = result & 1;
    return probe;
}

#if CAS_AUTO_VAULT_DIAGNOSTIC
// Each rejected candidate branches here with LR still identifying the last
// native query. Capture GP and SIMD without changing either or calling the
// query again. This also covers arithmetic checks following float queries.
unsigned RejectionCategory(unsigned call) {
    switch (call) {
    case 0x239a8b0: return 1; // landing obstruction
    case 0x239a98c: case 0x239aa04: case 0x239aa40: return 2; // occupancy
    case 0x239aabc: case 0x239ab60: case 0x239ac04: case 0x239aca8:
    case 0x239ad4c: case 0x239adec: case 0x239ae90: return 4; // water samples
    case 0x239aad4: case 0x239ab78: case 0x239ac1c: case 0x239acc0:
    case 0x239ad64: case 0x239ae04: case 0x239aea8: case 0x239aefc:
    case 0x239a85c: case 0x239a908: case 0x239a5a0: return 8; // height/type query
    case 0x239ab08: case 0x239abac: case 0x239ac50: case 0x239acf4:
    case 0x239ad98: case 0x239ae38: case 0x239aedc: case 0x239af30: return 16; // terrain/radius
    case 0x239af80: return 32; // path height
    case 0x239a6b4: return 64; // final preparation
    default: return 128;
    }
}
void ObserveSearch(exl::hook::InlineFloatCtx *ctx, bool rejected) {
    auto scope = CurrentVaultProbe;
    if (!scope || scope->player != reinterpret_cast<void *>(ctx->X[20])) return;
    if (rejected || !scope->failedCandidates) {
        scope->lastCall = static_cast<unsigned>(ctx->X[30] - Main);
        scope->integerResult = ctx->W[0];
        const float value = ctx->S[0];
        std::memcpy(&scope->floatBits, &value, sizeof(value));
    }
    if (rejected) {
        ++scope->failedCandidates;
        scope->failureMask |= RejectionCategory(scope->lastCall);
    }
}
HOOK_DEFINE_INLINE(AutoVaultRejectedTrace) {
    static void Callback(exl::hook::InlineFloatCtx *ctx) { ObserveSearch(ctx, true); }
};
HOOK_DEFINE_INLINE(AutoVaultExitTrace) {
    static void Callback(exl::hook::InlineFloatCtx *ctx) { ObserveSearch(ctx, false); }
};
// Independent budgets retain useful river-contact traces after startup/walking.
// Quantize timer/input values so the log records stages instead of every frame.
struct TraceSnapshot {
    void *player{};
    unsigned state{}, action{}, tool{}, timer{}, strong{}, slow{}, input{};
    bool operator==(const TraceSnapshot &) const = default;
};
TraceSnapshot LastMovement{};
unsigned MovementFrames{}, MovementBudget = 16, ContactBudget = 128;
HOOK_DEFINE_INLINE(AutoVaultMovementTrace) {
    static void Callback(exl::hook::InlineCtx *ctx) {
        auto player = reinterpret_cast<void *>(ctx->X[19]);
        if (!player) return;
        auto actor = Read<void *>(player, field::Actor);
        if (!actor || !Read<void *>(player, field::PlayerContext)) return;
        auto controller = Read<void *>(player, field::ActionController);
        const auto x = Read<float>(player, field::StickX), y = Read<float>(player, field::StickY);
        const auto speed = Read<float>(actor, field::Speed);
        const auto timer = Read<unsigned short>(player, field::HopTimer);
        TraceSnapshot now{player, Read<unsigned>(player, field::MovementState),
            controller ? Read<unsigned>(controller, field::Action) : ~0u,
            Native<unsigned (*)(void *)>(address::EquippedToolKind)(player),
            timer / 15u, x*x + y*y >= .81f, speed >= -5.f && speed <= 5.f,
            Read<unsigned char>(player, field::InputValid)};
        auto &budget = now.strong && now.slow ? ContactBudget : MovementBudget;
        if (!(now == LastMovement) || ++MovementFrames >= 120) {
            LastMovement = now;
            MovementFrames = 0;
            if (budget) {
                --budget;
                Logging.Log("[QoL Enhancements/Controls] VAULT movement: state=%u action=%u tool=0x%x input=%u stick=%d,%d speed=%d timer=%u\n",
                    now.state, now.action, now.tool, now.input,
                    static_cast<int>(x*1000.f), static_cast<int>(y*1000.f),
                    static_cast<int>(speed*1000.f), timer);
            }
        }
    }
};
void TraceEntry(void *player, unsigned probe, unsigned automatic, unsigned hop,
                unsigned geometry, int vault = -1, int result = -1, const VaultProbe *details = nullptr) {
    struct Stage {
        unsigned budget = 96, otherBudget = 16, last[10]{}, frames{};
        void *lastPlayer{};
    };
    // Native vault probes recursively check a manual short hop. Keep each
    // mode's deduplication separate so recursion cannot exhaust the trace.
    static Stage stages[4];
    auto &stage = stages[((automatic & 1) << 1) | (probe & 1)];
    const auto x = Read<float>(player, field::StickX), y = Read<float>(player, field::StickY);
    auto &remaining = x*x+y*y >= .81f ? stage.budget : stage.otherBudget;
    const unsigned tool = Native<unsigned (*)(void *)>(address::EquippedToolKind)(player);
    const auto timer = Read<unsigned short>(player, field::HopTimer);
    const unsigned now[]{probe, automatic, hop, geometry, tool, timer/15u,
                         static_cast<unsigned>(vault), static_cast<unsigned>(result),
                         details ? details->lastCall : 0, details ? details->failureMask | (unsigned(details->transferred) << 31) : 0};
    if (stage.lastPlayer == player && !std::memcmp(stage.last, now, sizeof(now)) && ++stage.frames < 120) return;
    stage.lastPlayer = player; stage.frames = 0; std::memcpy(stage.last, now, sizeof(now));
    if (!remaining) return;
    --remaining;
    Logging.Log("[QoL Enhancements/Controls] VAULT entry: probe=%u auto=%u hop=%u geometry=0x%x tool=0x%x timer=%u vault=%d result=%d\n",
        probe, automatic, hop, geometry, tool, timer, vault, result);
    if (details) {
        auto actor = Read<void *>(player, field::Actor);
        Logging.Log("[QoL Enhancements/Controls] VAULT search: phase=%u native=%u prepared=%u transfer=%u candidates=%u mask=0x%x last=0x%x ret=0x%x fp=0x%x pos=%d,%d,%d alt=%d,%d,%d facing=0x%x\n",
            probe, details->nativeResult, details->landingValidated, details->transferred, details->failedCandidates,
            details->failureMask, details->lastCall, details->integerResult, details->floatBits,
            static_cast<int>(Read<float>(actor, field::Position)*100.f),
            static_cast<int>(Read<float>(actor, field::Position+4)*100.f),
            static_cast<int>(Read<float>(actor, field::Position+8)*100.f),
            static_cast<int>(Read<float>(actor, field::AlternatePosition)*100.f),
            static_cast<int>(Read<float>(actor, field::AlternatePosition+4)*100.f),
            static_cast<int>(Read<float>(actor, field::AlternatePosition+8)*100.f),
            Read<unsigned>(actor, field::Facing));
    }
}
#else
void TraceEntry(void *, unsigned, unsigned, unsigned, unsigned, int = -1, int = -1, const VaultProbe * = nullptr) {}
#endif

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
        if (CurrentAttempt && CurrentAttempt->player == player) {
#if CAS_AUTO_VAULT_DIAGNOSTIC
            // Mask: reached=1, automatic=2, straight=4, null output=8.
            CurrentAttempt->geometry |= 1u | (request->automatic ? 2u : 0u) |
                (request->skewDegrees == 0.f ? 4u : 0u) | (!output ? 8u : 0u);
#endif
            if (request->automatic && request->skewDegrees == 0.0f && !output)
                CurrentAttempt->eligible = true;
#if CAS_AUTO_VAULT_DIAGNOSTIC
            if (CurrentAttempt->eligible) CurrentAttempt->geometry |= 16u;
#endif
        }
        return Orig(player, request, output);
    }
};
HOOK_DEFINE_TRAMPOLINE(AutoVaultEntry) {
    static unsigned Callback(void *player, unsigned probe, unsigned automatic) {
        if (!(automatic & 1)) {
            // The complete ordinary automatic-hop search already failed.
            // A check-only current-position vault must not commit a nested hop.
            if (CurrentVaultProbe && CurrentVaultProbe->player == player) return NoCrossing;
            ConsumeProof(player);
            const auto result = Orig(player, probe, automatic);
            TraceEntry(player, probe, automatic, result, false);
            return result;
        }
        const auto movementProof = ConsumeProof(player);
        HopAttempt attempt{player};
        auto previous = CurrentAttempt;
        CurrentAttempt = &attempt;
        const auto hop = Orig(player, probe, automatic);
        CurrentAttempt = previous;
        // Keep the complete ordinary-hop search, including adjusted landings
        // and its pending hold, first. Eligibility was observed only AFTER
        // native stick strength, low speed, contact and direction checks.
        if (hop != NoCrossing || !attempt.eligible ||
            Native<unsigned (*)(void *)>(address::EquippedToolKind)(player) != VaultingPoleKind) {
            TraceEntry(player, probe, automatic, hop, attempt.geometry);
            return hop;
        }

        auto vault = Native<unsigned (*)(void *, unsigned)>(address::TryVault);
        VaultProbe check{player};
        if (probe & 1) {
            // The full vault search reads tentative actor+0x278 even with
            // probe=1. Its transient river height fails the landing check.
            check.transferred = MatchesMovementStart(movementProof, player);
        } else {
            check = ProbeCurrentPositionVault(player);
        }
        const unsigned valid = check.transferred || (check.nativeResult && check.landingValidated);
        const bool complete = valid && HoldComplete(player);
        if (!valid || !complete || (probe & 1)) {
            const auto result = !valid ? NoCrossing : complete ? CrossingReady : Waiting;
            if (valid && !complete && !(probe & 1)) RememberBank(player);
            TraceEntry(player, probe, automatic, hop, attempt.geometry, valid, result, &check);
            return result;
        }

        // Re-run native validation against the current position and let the
        // game reserve landing space and enter LongStickJumpReady itself.
        const auto result = vault(player, 0) & 1;
        TraceEntry(player, probe, automatic, hop, attempt.geometry, valid, result, &check);
        if (result) Logging.Debug("[QoL Enhancements/Controls] auto vault: native traversal requested\n");
        return result;
    }
};
}

bool InstallAutoVault() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
#if CAS_AUTO_VAULT_DIAGNOSTIC
        {address::VaultCandidateRejected, {0x1100077b,0x71000f7f,0x54fffd40,0xf9403688,0xb4000428}},
        {address::VaultSearchExit, {0x6d4a23e9,0x6d492beb,0x6d4833ed,0x6d473bef,0xa9504ff4}},
        {address::MovementGuards, {0xb00144e8,0xf9414d08,0x79800108,0xb100051f,0x54000740}},
#endif
        {address::HopGeometry, {0xd10783ff,0x6d143bef,0x6d1533ed,0x6d162beb,0x6d1723e9}},
        {address::HopEntry, {0xd10243ff,0x6d0523e9,0xa9067bfd,0xa90757f6,0xa9084ff4}},
        {address::HopDirection, {0xd10643ff,0xa9137bfd,0xa9146ffc,0xa91567fa,0xa9165ff8}},
        {address::VaultPositionSelection, {0x7200027f,0xbd400008,0x52804f0a,0x5280508b,0x9a8a1178}},
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
#if CAS_AUTO_VAULT_DIAGNOSTIC
    AutoVaultRejectedTrace::InstallAtOffset(address::VaultCandidateRejected);
    AutoVaultExitTrace::InstallAtOffset(address::VaultSearchExit);
    AutoVaultMovementTrace::InstallAtOffset(address::MovementGuards);
    Logging.Log("[QoL Enhancements/Controls] DIAGNOSTIC: auto-vault-v4; bounded movement and entry traces\n");
#endif
    AutoVaultPrepare::InstallAtOffset(address::PrepareVault);
    AutoVaultGeometry::InstallAtOffset(address::HopGeometry);
    AutoVaultEntry::InstallAtOffset(address::HopEntry);
    Logging.Debug("[QoL Enhancements/Controls] auto vault: READY; prototype 3; equipped pole; native hop timer\n");
    return true;
}
}
