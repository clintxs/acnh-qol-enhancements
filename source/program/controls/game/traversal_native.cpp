#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "../auto_vault.hpp"
#include "traversal_native.hpp"
#include "program/core/native/runtime.hpp"
#include "../features/traversal.hpp"
#include <cstring>
#include <array>

namespace qol::controls {
namespace {
namespace profile = game::v303::traversal;
namespace address = profile::address;
namespace field = profile::field;

constexpr unsigned VaultingPoleKind = 0x28, LadderKind = 0x11;
// Proof discriminator only; never passed to inventory or equipment functions.
constexpr unsigned PlacedLadderTraversal = 0x100;
bool VaultEnabled{}, LadderEnabled{}, AutoEquipEnabled{}, PlacedLadderEnabled{};
constexpr unsigned NoCrossing = 0, CrossingReady = 1, Waiting = 2;
std::uintptr_t Main{};
using game::traversal_native::Read;

// Item values are eight bytes, including customization/durability data. Native
// pocket accessors own their read locks; never keep their returned pointers.
using Item = std::uint64_t;
constexpr Item EmptyItem = 0xfffe;
constexpr unsigned IdleAction = 5, MoveAction = 0xc, ToolChangeAction = 0x29;
struct ToolSelection { Item item = EmptyItem; int slot = -1; };
Item Held(void *player) {
    const auto resident = Read<unsigned char>(player, field::Resident);
    return qol::game::inventory::ForResident(Main, resident).Held();
}
unsigned Kind(Item item) { return qol::game::inventory::ToolKind(Main, item); }
ToolSelection FindTool(void *player, unsigned kind, Item exact = EmptyItem, bool requireEquipped = false) {
    auto resident = Read<unsigned char>(player, field::Resident);
    const auto pockets = qol::game::inventory::ForResident(Main, resident).Slots();
    const auto capacity = pockets.Capacity();
    if (capacity > 40) return {};
    for (unsigned i = 0; i < capacity; ++i) {
        const auto value = pockets.TryItemAt(i);
        if (!value.present) continue;
        const Item item = value.value;
        if (!((exact != EmptyItem && item == exact) || (exact == EmptyItem && Kind(item) == kind))) continue;
        if (requireEquipped && !pockets.IsEquipped(item, i)) continue;
        return {item, static_cast<int>(i)};
    }
    return {};
}
// Generic native action data is 0x7a bytes: a 0x20-byte shared header followed
// by state-specific data. ToolChange entry consumes item, variant, pocket slot,
// shortcut and animation bytes. RequestAction copies the local data before
// scheduling the action, just like native callers; inventory is not edited here.
bool ChangeTool(void *player, ToolSelection selection) {
    struct Request {
        std::array<std::byte, 0x20> header{};
        Item item{};
        unsigned char variant{};
        signed char slot{};
        unsigned char shortcut{};
        unsigned char animationState{};
        unsigned short animationFlags{};
        unsigned char animate{};
        std::array<std::byte, 0x51> unused{};
    } request{};
    static_assert(sizeof(Request) == 0x80);
    static_assert(offsetof(Request, slot) == 0x29 && offsetof(Request, animate) == 0x2e);
    std::memcpy(request.header.data(), static_cast<const std::byte *>(player) + field::CurrentActionData, request.header.size());
    request.item = selection.item;
    request.slot = static_cast<signed char>(selection.slot);
    // Native entry skips PlayerToolChangeOn/Off when animate=0. Its update
    // takes a separate non-rotating path, refreshes the held model/visibility,
    // and returns to movement without waiting for animation events.
    request.shortcut = 1;
    request.animate = 0;
    return profile::RequestAction.Resolve(Main)(player, ToolChangeAction, &request);
}
using SwapStage = features::BorrowStage;
struct BorrowedTool {
    SwapStage stage{};
    void *player{};
    unsigned handle{}, kind{}, facing{};
    ToolSelection borrowed{}, previous{};
    std::array<float, 3> bank{};
    bool equipEntered{}, equipFinished{}, traversalEntered{};
};
std::array<BorrowedTool, 8> BorrowedTools{};
BorrowedTool *SwapFor(void *player) {
    const auto resident = Read<unsigned char>(player, field::Resident);
    if (resident >= BorrowedTools.size()) return nullptr;
    auto &swap = BorrowedTools[resident];
    if (swap.player != player || swap.handle != Read<unsigned>(player, field::PlayerHandle)) swap = {};
    return &swap;
}
void TraceSwap(const char *event, unsigned kind, int slot, unsigned detail = 0) {
#if CAS_AUTO_VAULT_DIAGNOSTIC
    static unsigned budget = 64;
    if (budget) {
        --budget;
        Logging.Log("[QoL Enhancements/Controls] SWAP %s: kind=0x%x slot=%d detail=0x%x\n", event, kind, slot, detail);
    }
#else
    (void)event; (void)kind; (void)slot; (void)detail;
#endif
}
bool BorrowTool(void *player, unsigned kind, ToolSelection selection, unsigned heldKind) {
    struct Equipment {
        void *player; unsigned kind; BorrowedTool *swap;
        bool CanBeginBorrow() const { return swap && swap->stage == SwapStage::None; }
        ToolSelection EmptySelection() const { return {}; }
        ToolSelection FindEquippedItem() const { return FindTool(player, 0, Held(player), true); }
        void RecordBorrow(ToolSelection selection, ToolSelection previous) {
            auto actor = Read<void *>(player, field::Actor);
            *swap = {SwapStage::Equipping, player, Read<unsigned>(player, field::PlayerHandle), kind,
                     Read<unsigned>(actor, field::Facing), selection, previous,
                     Read<std::array<float, 3>>(actor, field::Position)};
        }
        bool RequestToolChange(ToolSelection selection) const { return ChangeTool(player, selection); }
        void ClearBorrow() { *swap = {}; }
        void TraceBorrow() const { TraceSwap("borrowed", kind, swap->borrowed.slot); }
    } equipment{player, kind, SwapFor(player)};
    return features::BorrowTraversalTool(equipment, selection, heldKind != 0);
}
void RestoreTool(void *player, BorrowedTool &swap) {
    struct Equipment {
        void *player; BorrowedTool &swap;
        bool SameItemAtSlot(ToolSelection selection) const {
            const auto resident = Read<unsigned char>(player, field::Resident);
            const auto item = qol::game::inventory::ForResident(Main, resident).Slots().TryItemAt(selection.slot);
            return item.present && item.value == selection.item;
        }
        ToolSelection FindExactItem(Item item) const { return FindTool(player, 0, item); }
        void ClearBorrow() { swap = {}; }
        bool RequestToolChange(ToolSelection selection) const { return ChangeTool(player, selection); }
        void TraceRestore(ToolSelection previous) const { TraceSwap("restored", 0, previous.slot); }
    } equipment{player, swap};
    features::RestoreTraversalTool(equipment, swap.previous);
}

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
// finish before we consider a tool traversal. Like the existing pickup scope,
// this relies on the player dispatcher running synchronously on the game thread.
struct HopAttempt { void *player; bool eligible = false; bool includeWalls = false; unsigned geometry = 0; };
HopAttempt *CurrentAttempt{};

// Native automatic hops ask for contact classes 4 then 6. Class 1 includes
// solid wall contacts (flags 0x84 without the excluded 0x40 bit), which a cliff
// face may use. Retry only a failed ladder-enabled attempt with class 7, after
// the unchanged native search. Keep the native facing filter and outputs. The
// caller's request fields remain intact; only native output fields are copied.
struct ContactRequest {
    float normals[6];
    unsigned total, accepted;
    unsigned classes, padding;
    const unsigned *facing;
};
static_assert(offsetof(ContactRequest, classes) == 0x20);
static_assert(offsetof(ContactRequest, facing) == 0x28);
static_assert(sizeof(ContactRequest) == 0x30);
HOOK_DEFINE_TRAMPOLINE(AutoLadderContact) {
    static unsigned Callback(void *player, ContactRequest *request) {
        const auto attempt = CurrentAttempt;
        if (!attempt || attempt->player != player || !attempt->includeWalls || request->classes != 6)
            return Orig(player, request);
        auto extended = *request;
        extended.classes |= 1;
        const auto result = Orig(player, &extended);
        std::memcpy(request, &extended, offsetof(ContactRequest, classes));
#if CAS_AUTO_VAULT_DIAGNOSTIC
        // Bounded evidence if the live cliff uses a different contact class.
        constexpr std::size_t Collision = 0xa0, Count = 0x2ef4, Records = 0x2ef8;
        constexpr std::size_t Stride = 0x80, Flags = 0x4c;
        std::array<unsigned, 7> now{result, extended.total, extended.accepted};
        if (auto collision = Read<void *>(player, Collision)) {
            const auto count = Read<unsigned>(collision, Count);
            for (unsigned i = 0; i < 4 && i < count; ++i)
                now[3+i] = Read<unsigned>(collision, Records + i*Stride + Flags);
        }
        static std::array<unsigned, 7> last{};
        static unsigned budget = 32;
        if (budget && now != last) {
            last = now;
            --budget;
            Logging.Log("[QoL Enhancements/Controls] LADDER contact: classes=6->7 result=%u total=%u accepted=%u flags=%x,%x,%x,%x\n",
                        now[0], now[1], now[2], now[3], now[4], now[5], now[6]);
        }
#endif
        return result;
    }
};

// Movement tests a tentative transform before collision correction. Transfer
// one stable-phase validation to the following movement timer check, requiring
// the same player/handle and EXACT starting bank position. The native intent
// guards still run in both phases, and every actual jump is validated afresh.
struct MovementProof {
    void *player{};
    unsigned handle{}, tool{}, heldKind{};
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
bool MatchesMovementStart(const MovementProof &proof, void *player, unsigned tool) {
    auto actor = Read<void *>(player, field::Actor);
    if (!proof.valid || proof.player != player || proof.tool != tool || !actor ||
        proof.handle != Read<unsigned>(player, field::PlayerHandle) ||
        proof.heldKind != profile::EquippedToolKind.Resolve(Main)(player)) return false;
    for (unsigned i = 0; i < proof.bank.size(); ++i)
        if (proof.bank[i] != Read<float>(actor, field::AlternatePosition + i*sizeof(float))) return false;
    return true;
}
void RememberBank(void *player, unsigned tool) {
    if (auto proof = ProofFor(player)) {
        proof->player = player;
        proof->tool = tool;
        proof->heldKind = profile::EquippedToolKind.Resolve(Main)(player);
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
    bool Valid() const { return transferred || (nativeResult && landingValidated); }
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
    const auto result = profile::TryVault.Resolve(Main)(player, 0);
    CurrentVaultProbe = previous;
    probe.nativeResult = result & 1;
    return probe;
}

// The native ladder probe returns success for both a real climb and its
// DisableLadder error dialogue. Observe only the valid-climb branch, after
// height, obstruction, occupancy and terrain checks. This hook reads context
// without changing registers, flags or the native reservation/animation path.
struct LadderProbe { void *player; bool valid = false; };
LadderProbe *CurrentLadderProbe{};
HOOK_DEFINE_INLINE(AutoLadderLanding) {
    static void Callback(exl::hook::InlineCtx *ctx) {
        if (CurrentLadderProbe && CurrentLadderProbe->player == reinterpret_cast<void *>(ctx->X[19]))
            CurrentLadderProbe->valid = true;
    }
};
VaultProbe ProbeCurrentPositionLadder(void *player, bool placed = false) {
    LadderProbe probe{player};
    auto previous = CurrentLadderProbe;
    CurrentLadderProbe = &probe;
    const auto result = (placed ? profile::TryPlacedLadder : profile::TryLadder).Resolve(Main)(player, 1);
    CurrentLadderProbe = previous;
    VaultProbe check{player};
    check.nativeResult = result & 1;
    check.landingValidated = probe.valid;
    return check;
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
            profile::EquippedToolKind.Resolve(Main)(player),
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
    const unsigned tool = profile::EquippedToolKind.Resolve(Main)(player);
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
    auto parameters = profile::PlayerParameters.Resolve(Main)(1);
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
using game::TraversalKind;
using game::Crossing;
TraversalKind DecodeTool(unsigned kind) {
    if (kind == VaultingPoleKind) return TraversalKind::Pole;
    if (kind == LadderKind) return TraversalKind::Ladder;
    if (kind == PlacedLadderTraversal) return TraversalKind::PlacedLadder;
    return kind ? TraversalKind::Other : TraversalKind::None;
}
unsigned EncodeTool(TraversalKind kind) {
    switch (kind) {
    case TraversalKind::Pole: return VaultingPoleKind;
    case TraversalKind::Ladder: return LadderKind;
    case TraversalKind::PlacedLadder: return PlacedLadderTraversal;
    default: return 0;
    }
}
Crossing DecodeCrossing(unsigned result) {
    return static_cast<Crossing>(result);
}
unsigned EncodeCrossing(Crossing result) {
    return static_cast<unsigned>(result);
}
HOOK_DEFINE_TRAMPOLINE(AutoVaultEntry) {
    static unsigned Callback(void *player, unsigned probe, unsigned automatic) {
        if (!(automatic & 1)) {
            // A current-position vault probe must not commit a nested short
            // hop after the complete ordinary hop search has already failed.
            if (CurrentVaultProbe && CurrentVaultProbe->player == player) return NoCrossing;
            ConsumeProof(player);
            const auto result = Orig(player, probe, automatic);
            TraceEntry(player, probe, automatic, result, false);
            return result;
        }
        struct Traversal {
            void *player; unsigned probe, automatic;
            HopAttempt attempt;
            // Search scopes never survive Orig; native alternatives and output
            // requests retain their ordering. Wall retry changes contact class
            // only inside this attempt and preserves the request's input fields.
            Crossing SearchNativeHop(bool walls) {
                attempt.includeWalls = walls;
#if CAS_AUTO_VAULT_DIAGNOSTIC
                if (walls) attempt.geometry |= 32u;
#endif
                const auto previous = CurrentAttempt;
                CurrentAttempt = &attempt;
                const auto hop = Orig(player, probe, automatic);
                CurrentAttempt = previous;
                return DecodeCrossing(hop);
            }
            Crossing OrdinaryHop() const { return DecodeCrossing(Orig(player, probe, automatic)); }
            MovementProof ConsumeMovementProof() const { return ConsumeProof(player); }
            bool ToolBorrowActive() const { const auto swap = SwapFor(player); return swap && swap->stage != SwapStage::None; }
            bool IntentObserved() const { return attempt.eligible; }
            bool IncludesWalls() const { return attempt.includeWalls; }
            TraversalKind HeldTool() const { return DecodeTool(profile::EquippedToolKind.Resolve(Main)(player)); }
            TraversalKind ProvenTool(const MovementProof &proof) const { return DecodeTool(proof.tool); }
            ToolSelection EmptySelection() const { return {}; }
            VaultProbe EmptyCheck() const { return {player}; }
            VaultProbe TransferProof(const MovementProof &proof, TraversalKind tool) const {
                VaultProbe check{player};
                check.transferred = MatchesMovementStart(proof, player, EncodeTool(tool));
                return check;
            }
            VaultProbe CheckLanding(TraversalKind tool) const {
                return tool == TraversalKind::Pole ? ProbeCurrentPositionVault(player)
                    : ProbeCurrentPositionLadder(player, tool == TraversalKind::PlacedLadder);
            }
            bool HoldComplete() const { return qol::controls::HoldComplete(player); }
            void RememberBank(TraversalKind tool) const { qol::controls::RememberBank(player, EncodeTool(tool)); }
            ToolSelection FindTool(TraversalKind tool) const { return qol::controls::FindTool(player, EncodeTool(tool)); }
            bool BorrowTool(TraversalKind tool, ToolSelection selection, TraversalKind held) const {
                return qol::controls::BorrowTool(player, EncodeTool(tool), selection,
                    held != TraversalKind::None);
            }
            Crossing RequestTraversal(TraversalKind tool) const {
                const auto traverse = tool == TraversalKind::PlacedLadder ? profile::TryPlacedLadder
                    : tool == TraversalKind::Ladder ? profile::TryLadder : profile::TryVault;
                const auto result = traverse.Resolve(Main)(player, 0) & 1;
                if (tool == TraversalKind::PlacedLadder)
                    Logging.Debug("[QoL Enhancements/Controls] auto placed ladder: native result=%u\n", result);

                return DecodeCrossing(result);
            }
            void TraceRequestedTraversal(TraversalKind tool, Crossing result) const {
                if (result != Crossing::None)
                    Logging.Debug("[QoL Enhancements/Controls] auto %s: native traversal requested\n",
                                  tool == TraversalKind::Ladder ? "ladder" : "vault");
            }
            void TraceHop(Crossing hop) const { TraceEntry(player, probe, automatic, EncodeCrossing(hop), attempt.geometry); }
            void TraceCheck(Crossing hop, bool valid, Crossing result, const VaultProbe &check) const {
                TraceEntry(player, probe, automatic, EncodeCrossing(hop), attempt.geometry, valid, EncodeCrossing(result), &check);
            }
        } traversal{player, probe, automatic, {player}};
        return EncodeCrossing(features::TraverseWhileHeld(traversal,
            {VaultEnabled, LadderEnabled, AutoEquipEnabled, PlacedLadderEnabled}, probe & 1));
    }
};
// Borrow state is correlated by resident, player pointer and generation in
// SwapFor. Exact full-item equality protects manual equipment ownership.
struct BorrowedEquipment {
    void *player; BorrowedTool &swap;
    bool StillOwnsBorrowedTool() const { return Held(player) == swap.borrowed.item; }
    void RestorePreviousTool() { RestoreTool(player, swap); }
    void ReleaseOwnership() { TraceSwap("ownership lost", swap.kind, swap.borrowed.slot); swap = {}; }
    struct StickPosition { float x, y; };
    StickPosition Stick() const { return {Read<float>(player, field::StickX), Read<float>(player, field::StickY)}; }
    bool FacingMatchesBorrow() const {
        const auto actor = Read<void *>(player, field::Actor);
        return Read<unsigned>(actor, field::Facing) == swap.facing;
    }
    bool AtBorrowedBank() const {
        const auto actor = Read<void *>(player, field::Actor);
        return Read<std::array<float, 3>>(actor, field::Position) == swap.bank;
    }
    VaultProbe CheckLanding() const {
        return swap.kind == LadderKind ? ProbeCurrentPositionLadder(player) : ProbeCurrentPositionVault(player);
    }
    unsigned RequestTraversal() const {
        const auto traversal = swap.kind == LadderKind ? profile::TryLadder : profile::TryVault;
        return traversal.Resolve(Main)(player, 0) & 1;
    }
    void TraceCancellation(bool intent, const VaultProbe &check) const {
        TraceSwap("cancelled", swap.kind, swap.borrowed.slot,
                  unsigned(intent) | (check.nativeResult << 1) | (unsigned(check.landingValidated) << 2));
    }
    void TraceTraversal() const { TraceSwap("traversing", swap.kind, swap.borrowed.slot); }
};
// State entry finishes even while the stick is released. Observe its native
// finalizer after all entry setup, rather than waiting for the walking-only
// dispatcher or mistaking an old Move value for a queued traversal finishing.
HOOK_DEFINE_TRAMPOLINE(AutoTraversalActionFinished) {
    static void Callback(void *player, unsigned action) {
        Orig(player, action);
        const bool locomotion = action == IdleAction || action == MoveAction;
        if (auto swap = SwapFor(player); swap && swap->stage != SwapStage::None) {
            BorrowedEquipment equipment{player, *swap};
            const bool traversal = swap->kind == LadderKind ? action >= 0x8a && action <= 0x8c
                                                           : action >= 0xb4 && action <= 0xb7;
            features::FinishTraversalAction(*swap, equipment, locomotion, action == ToolChangeAction, traversal);
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(AutoTraversalSwap) {
    static unsigned Callback(void *player) {
        const auto result = Orig(player);
        auto swap = SwapFor(player);
        if (!swap || swap->stage == SwapStage::None || result) return result;
        auto controller = Read<void *>(player, field::ActionController);
        auto actor = Read<void *>(player, field::Actor);
        if (!controller || !actor || Read<unsigned>(controller, field::Action) != MoveAction ||
            !Read<unsigned char>(player, field::InputValid)) return result;
        BorrowedEquipment equipment{player, *swap};
        const auto continuation = features::ContinueBorrowedTraversal(*swap, equipment);
        return continuation == Crossing::None ? result : EncodeCrossing(continuation);
    }
};

}

namespace {
void TraversalResult(const char *id, bool requested, bool available, const char *reason) {
    const core::FeatureResult result{!requested ? core::FeatureState::Disabled :
        available ? core::FeatureState::Ready : core::FeatureState::Unsupported,
        requested && available, reason};
    core::ActivationResults.Store(id, result);
    if (result.Complete()) Logging.Debug("[QoL Enhancements] feature=%s state=%s published=%s reason=%s\n",
        id, core::FeatureStateName(result.state), result.published ? "true" : "false", reason);
    else Logging.Error("[QoL Enhancements] feature=%s state=%s published=false reason=%s\n",
        id, core::FeatureStateName(result.state), reason);
}
}
bool InstallAutoTraversal(bool vault, bool ladder, bool autoEquip) {
    // Feature bits: 1 pole, 2 ladder, 3 shared dependency.

    const auto &main = exl::util::GetMainModuleInfo();
    const auto executable = native::MainExecutable();
    unsigned enabled = unsigned(vault) | (unsigned(ladder) << 1);
    const auto requested = enabled;
    for (const auto &f : profile::Checks) {
        if (!(f.features & requested)) continue;
        const auto result = executable.Check(native::Fingerprint<>{f.offset, f.words, f.name});
        if (!result) {
            native::ReportMismatch(result);
            if (vault && (f.features & 1))
                Logging.Error("[QoL Enhancements/Controls] auto vault: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
            if (ladder && (f.features & 2))
                Logging.Error("[QoL Enhancements/Controls] auto ladder: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
            enabled &= ~f.features;
        }
    }
    if (!enabled) {
        TraversalResult("controls.auto_vault", vault, false, "native traversal bindings unavailable");
        TraversalResult("controls.auto_ladder", ladder, false, "native traversal bindings unavailable");
        return false;
    }
    VaultEnabled = enabled & 1;
    LadderEnabled = enabled & 2;
    Main = main.m_Text.m_Start;
    // A changed world-object dependency disables only this extension. Keep
    // the already confirmed handheld ladder and pole paths available.

    PlacedLadderEnabled = LadderEnabled;
    if (LadderEnabled) for (const auto &f : profile::PlacedChecks) {
        const auto result = executable.Check(native::Fingerprint<>{f.offset, f.words, f.name});
        if (!result) {
            native::ReportMismatch(result);
            PlacedLadderEnabled = false;
            Logging.Error("[QoL Enhancements/Controls] auto placed ladder: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        }
    }

    AutoEquipEnabled = autoEquip;
    if (autoEquip) for (const auto &f : profile::SwapChecks) {
        const auto result = executable.Check(native::Fingerprint<>{f.offset, f.words, f.name});
        if (!result) {
            native::ReportMismatch(result);
            AutoEquipEnabled = false;
            Logging.Error("[QoL Enhancements/Controls] auto equip: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        }
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<AutoTraversalActionFinished>("AutoTraversalActionFinished", address::ActionEntryFinished, profile::HookWords(address::ActionEntryFinished), AutoEquipEnabled),
        qol::core::Site<AutoTraversalSwap>("AutoTraversalSwap", address::StableActions, profile::HookWords(address::StableActions), AutoEquipEnabled),
#if CAS_AUTO_VAULT_DIAGNOSTIC
        qol::core::Site<AutoVaultRejectedTrace>("AutoVaultRejectedTrace", address::VaultCandidateRejected, profile::HookWords(address::VaultCandidateRejected), VaultEnabled),
#endif
#if CAS_AUTO_VAULT_DIAGNOSTIC
        qol::core::Site<AutoVaultExitTrace>("AutoVaultExitTrace", address::VaultSearchExit, profile::HookWords(address::VaultSearchExit), VaultEnabled),
#endif
#if CAS_AUTO_VAULT_DIAGNOSTIC
        qol::core::Site<AutoVaultMovementTrace>("AutoVaultMovementTrace", address::MovementGuards, profile::HookWords(address::MovementGuards), true),
#endif
        qol::core::Site<AutoVaultPrepare>("AutoVaultPrepare", address::PrepareVault, profile::HookWords(address::PrepareVault), VaultEnabled),
        qol::core::Site<AutoLadderContact>("AutoLadderContact", address::ContactNormals, profile::HookWords(address::ContactNormals), LadderEnabled),
        qol::core::Site<AutoLadderLanding>("AutoLadderLanding", address::ValidLadderLanding, profile::HookWords(address::ValidLadderLanding), LadderEnabled),
        qol::core::Site<AutoVaultGeometry>("AutoVaultGeometry", address::HopGeometry, profile::HookWords(address::HopGeometry), true),
        qol::core::Site<AutoVaultEntry>("AutoVaultEntry", address::HopEntry, profile::HookWords(address::HopEntry), true),
    };
    if (!hookPlan) return false;
    if (AutoEquipEnabled) {
        if (!hookPlan.Publish<AutoTraversalActionFinished>(address::ActionEntryFinished)) return false;
        if (!hookPlan.Publish<AutoTraversalSwap>(address::StableActions)) return false;
    }
#if CAS_AUTO_VAULT_DIAGNOSTIC
    if (VaultEnabled) {
        if (!hookPlan.Publish<AutoVaultRejectedTrace>(address::VaultCandidateRejected)) return false;
        if (!hookPlan.Publish<AutoVaultExitTrace>(address::VaultSearchExit)) return false;
    }
    if (!hookPlan.Publish<AutoVaultMovementTrace>(address::MovementGuards)) return false;
    Logging.Log("[QoL Enhancements/Controls] DIAGNOSTIC: auto-vault-v4; bounded movement and entry traces\n");
#endif
    if (VaultEnabled) if (!hookPlan.Publish<AutoVaultPrepare>(address::PrepareVault)) return false;
    if (LadderEnabled) {
        if (!hookPlan.Publish<AutoLadderContact>(address::ContactNormals)) return false;
        if (!hookPlan.Publish<AutoLadderLanding>(address::ValidLadderLanding)) return false;
    }
    if (!hookPlan.Publish<AutoVaultGeometry>(address::HopGeometry)) return false;
    if (!hookPlan.Publish<AutoVaultEntry>(address::HopEntry)) return false;
    if (LadderEnabled) Logging.Debug("[QoL Enhancements/Controls] auto ladder: READY; prototype 2; equipped ladder; native hop timer\n");
    if (PlacedLadderEnabled) Logging.Debug("[QoL Enhancements/Controls] auto placed ladder: READY; prototype 1; native object search\n");
    if (VaultEnabled) Logging.Debug("[QoL Enhancements/Controls] auto vault: READY; prototype 3; equipped pole; native hop timer\n");
    if (AutoEquipEnabled) Logging.Debug("[QoL Enhancements/Controls] auto equip: READY; prototype 2; pocket tool and restore\n");
    TraversalResult("controls.auto_vault", vault, VaultEnabled, "pole traversal validation");
    TraversalResult("controls.auto_ladder", ladder, LadderEnabled, "handheld ladder validation");
    TraversalResult("controls.placed_ladder", ladder, PlacedLadderEnabled, "placed ladder validation");
    TraversalResult("controls.auto_equip", autoEquip, AutoEquipEnabled, "tool swap validation");
    return enabled == requested && (!ladder || PlacedLadderEnabled) && (!autoEquip || AutoEquipEnabled);
}
}
