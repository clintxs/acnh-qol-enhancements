#include "lib.hpp"
#include "auto_vault.hpp"
#include <cstring>
#include <array>

namespace qol::controls {
namespace {
namespace address {
constexpr std::uintptr_t ActionEntered = 0x02275730;
constexpr std::uintptr_t ToolChangeEntry = 0x023758a0, ToolChangeUpdate = 0x02375ed0;
constexpr std::uintptr_t ToolChangeAnimationBranch = 0x02376018;
constexpr std::uintptr_t StableActions = 0x02222950, RequestAction = 0x02272704;
constexpr std::uintptr_t PocketCapacity = 0x02572e00, PocketItem = 0x02577ea0;
constexpr std::uintptr_t IsEquippedSlot = 0x0256a0c0;
constexpr std::uintptr_t HeldItem = 0x0242d8f0, ItemParameter = 0x019d5d04, ItemToolKind = 0x019d78cc;
constexpr std::uintptr_t ContactNormals = 0x015f3040;
constexpr std::uintptr_t HopGeometry = 0x0230c470;
constexpr std::uintptr_t HopEntry = 0x0230b8d0, HopDirection = 0x0230bf18;
constexpr std::uintptr_t TryLadder = 0x02396154, LadderGeometry = 0x0239630c;
constexpr std::uintptr_t TryPlacedLadder = 0x022d9bd8;
constexpr std::uintptr_t PlacedObjectKind = 0x0113db50, PlacedObjectCoordinates = 0x015e49e4;
constexpr std::uintptr_t ValidLadderLanding = 0x02396bfc;
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
constexpr std::size_t CurrentActionData = 0x18e0;
constexpr std::size_t Resident = 0x1ee8, PlayerHandle = 0x230;
constexpr std::size_t ParameterValueMethod = 0xf0;
}
constexpr unsigned VaultingPoleKind = 0x28, LadderKind = 0x11;
// Proof discriminator only; never passed to inventory or equipment functions.
constexpr unsigned PlacedLadderTraversal = 0x100;
bool VaultEnabled{}, LadderEnabled{}, AutoEquipEnabled{}, PlacedLadderEnabled{};
constexpr unsigned NoCrossing = 0, CrossingReady = 1, Waiting = 2;
std::uintptr_t Main{};
template<class T> T Read(const void *object, std::size_t offset = 0) {
    T value;
    std::memcpy(&value, static_cast<const std::byte *>(object) + offset, sizeof(value));
    return value;
}
template<class F> F Native(std::uintptr_t offset) { return reinterpret_cast<F>(Main + offset); }

// Item values are eight bytes, including customization/durability data. Native
// pocket accessors own their read locks; never keep their returned pointers.
using Item = std::uint64_t;
constexpr Item EmptyItem = 0xfffe;
constexpr unsigned MoveAction = 0xc, ToolChangeAction = 0x29;
struct ToolSelection { Item item = EmptyItem; int slot = -1; };
Item Held(void *player) {
    // The native nontrivial Item return uses AArch64's hidden X8 result,
    // rather than X0. A nontrivial local wrapper preserves that calling ABI.
    struct NativeItem { Item value; ~NativeItem() {} };
    auto resident = Read<unsigned char>(player, field::Resident);
    return Native<NativeItem (*)(const unsigned char *)>(address::HeldItem)(&resident).value;
}
unsigned Kind(Item item) {
    if (static_cast<unsigned short>(item) >= 0xfffd) return 0;
    std::array<std::uintptr_t, 3> parameter{};
    Native<void (*)(void *, const Item *)>(address::ItemParameter)(parameter.data(), &item);
    return Native<unsigned (*)(const void *)>(address::ItemToolKind)(parameter.data());
}
ToolSelection FindTool(void *player, unsigned kind, Item exact = EmptyItem, bool requireEquipped = false) {
    auto resident = Read<unsigned char>(player, field::Resident);
    const auto capacity = Native<unsigned (*)(const unsigned char *)>(address::PocketCapacity)(&resident);
    if (capacity > 40) return {};
    for (unsigned i = 0; i < capacity; ++i) {
        auto pointer = Native<const Item *(*)(const unsigned char *, unsigned)>(address::PocketItem)(&resident, i);
        if (!pointer) continue;
        const Item item = Read<Item>(pointer);
        if (!((exact != EmptyItem && item == exact) || (exact == EmptyItem && Kind(item) == kind))) continue;
        if (requireEquipped && !Native<bool (*)(const unsigned char *, const Item *, unsigned)>(address::IsEquippedSlot)(&resident, &item, i)) continue;
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
    return Native<bool (*)(void *, unsigned, const void *)>(address::RequestAction)(player, ToolChangeAction, &request);
}
enum class SwapStage { None, Equipping, Traversing };
struct BorrowedTool {
    SwapStage stage{};
    void *player{};
    unsigned handle{}, kind{}, facing{};
    ToolSelection borrowed{}, previous{};
    std::array<float, 3> bank{};
    bool equipEntered{}, equipFinished{}, traversalEntered{}, traversalFinished{};
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
    auto swap = SwapFor(player);
    if (!swap || swap->stage != SwapStage::None || selection.slot < 0) return false;
    ToolSelection previous{};
    if (heldKind) {
        previous = FindTool(player, 0, Held(player), true);
        // A temporary/event item with no pocket association is not ours to swap.
        if (previous.slot < 0) return false;
    }
    auto actor = Read<void *>(player, field::Actor);
    *swap = {SwapStage::Equipping, player, Read<unsigned>(player, field::PlayerHandle), kind,
             Read<unsigned>(actor, field::Facing), selection, previous,
             Read<std::array<float, 3>>(actor, field::Position)};
    if (!ChangeTool(player, selection)) { *swap = {}; return false; }
    TraceSwap("borrowed", kind, selection.slot);
    return true;
}
void RestoreTool(void *player, BorrowedTool &swap) {
    auto previous = swap.previous;
    // Resolve the exact item again in case pockets were reordered. If it was
    // removed, return to empty hands instead of recreating a stale item value.
    if (previous.slot >= 0) {
        auto resident = Read<unsigned char>(player, field::Resident);
        auto pointer = Native<const Item *(*)(const unsigned char *, unsigned)>(address::PocketItem)(&resident, previous.slot);
        if (!pointer || Read<Item>(pointer) != previous.item) previous = FindTool(player, 0, previous.item);
    }
    swap = {};
    ChangeTool(player, previous);
    TraceSwap("restored", 0, previous.slot);
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
        proof.heldKind != Native<unsigned (*)(void *)>(address::EquippedToolKind)(player)) return false;
    for (unsigned i = 0; i < proof.bank.size(); ++i)
        if (proof.bank[i] != Read<float>(actor, field::AlternatePosition + i*sizeof(float))) return false;
    return true;
}
void RememberBank(void *player, unsigned tool) {
    if (auto proof = ProofFor(player)) {
        proof->player = player;
        proof->tool = tool;
        proof->heldKind = Native<unsigned (*)(void *)>(address::EquippedToolKind)(player);
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
    const auto result = Native<unsigned (*)(void *, unsigned)>(placed ? address::TryPlacedLadder : address::TryLadder)(player, 1);
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
        if (AutoEquipEnabled) {
            const auto swap = SwapFor(player);
            if (swap && swap->stage != SwapStage::None) return Orig(player, probe, automatic);
        }
        HopAttempt attempt{player};
        auto previous = CurrentAttempt;
        CurrentAttempt = &attempt;
        auto hop = Orig(player, probe, automatic);
        CurrentAttempt = previous;
        if (hop == NoCrossing && !attempt.eligible && LadderEnabled &&
            (PlacedLadderEnabled || AutoEquipEnabled || Native<unsigned (*)(void *)>(address::EquippedToolKind)(player) == LadderKind)) {
            attempt.includeWalls = true;
#if CAS_AUTO_VAULT_DIAGNOSTIC
            attempt.geometry |= 32u; // Ladder-only wall-contact retry.
#endif
            CurrentAttempt = &attempt;
            hop = Orig(player, probe, automatic);
            CurrentAttempt = previous;
        }
        // Keep the complete ordinary-hop search, including adjusted landings
        // and its pending hold, first. Eligibility was observed only AFTER
        // native stick strength, low speed, contact and direction checks.
        if (hop != NoCrossing || !attempt.eligible) {
            TraceEntry(player, probe, automatic, hop, attempt.geometry);
            return hop;
        }
        // World ladders have their own native object search (including its
        // orientation and placement metadata), followed by the shared climb
        // validator. Prefer that path before borrowing a handheld ladder.
        // As with tools, only the stable phase searches; movement consumes a
        // one-use proof to advance the existing native hold timer.
        if (PlacedLadderEnabled) {
            VaultProbe placed{player};
            if (probe & 1) {
                placed.transferred = MatchesMovementStart(movementProof, player, PlacedLadderTraversal);
            } else {
                placed = ProbeCurrentPositionLadder(player, true);
            }
            if (placed.transferred || (placed.nativeResult && placed.landingValidated)) {
                if (!HoldComplete(player)) {
                    if (!(probe & 1)) RememberBank(player, PlacedLadderTraversal);
                    return Waiting;
                }
                if (probe & 1) return CrossingReady;
                const auto result = Native<unsigned (*)(void *, unsigned)>(address::TryPlacedLadder)(player, 0) & 1;
                Logging.Debug("[QoL Enhancements/Controls] auto placed ladder: native result=%u\n", result);
                return result;
            }
        }
        const auto heldKind = Native<unsigned (*)(void *)>(address::EquippedToolKind)(player);
        unsigned tool = heldKind;
        bool ladder = LadderEnabled && tool == LadderKind;
        ToolSelection selection{};
        VaultProbe check{player};
        if (probe & 1) {
            tool = movementProof.tool;
            ladder = tool == LadderKind;
            check.transferred = MatchesMovementStart(movementProof, player, tool) &&
                (AutoEquipEnabled || tool == heldKind);
        } else {
            if (ladder) check = ProbeCurrentPositionLadder(player);
            else if (!attempt.includeWalls && VaultEnabled && tool == VaultingPoleKind) check = ProbeCurrentPositionVault(player);
            if (AutoEquipEnabled && !(check.nativeResult && check.landingValidated)) {
                ladder = false;
                tool = 0;
                if (LadderEnabled && heldKind != LadderKind) {
                    selection = FindTool(player, LadderKind);
                    if (selection.slot >= 0) {
                        check = ProbeCurrentPositionLadder(player);
                        if (check.nativeResult && check.landingValidated) { tool = LadderKind; ladder = true; }
                    }
                }
                if (!ladder && !attempt.includeWalls && VaultEnabled && heldKind != VaultingPoleKind) {
                    selection = FindTool(player, VaultingPoleKind);
                    if (selection.slot >= 0) { tool = VaultingPoleKind; check = ProbeCurrentPositionVault(player); }
                }
            }
        }
        // The wider wall-contact retry is for climbing only. A failed placed
        // climb must not grant pole traversal eligibility through a solid wall.
        if (attempt.includeWalls && !ladder) return hop;
        if (!(ladder || (VaultEnabled && tool == VaultingPoleKind))) return hop;
        auto traverse = Native<unsigned (*)(void *, unsigned)>(ladder ? address::TryLadder : address::TryVault);
        const unsigned valid = check.transferred || (check.nativeResult && check.landingValidated);
        const bool complete = valid && HoldComplete(player);
        if (!valid || !complete || (probe & 1)) {
            const auto result = !valid ? NoCrossing : complete ? CrossingReady : Waiting;
            if (valid && !complete && !(probe & 1)) RememberBank(player, tool);
            TraceEntry(player, probe, automatic, hop, attempt.geometry, valid, result, &check);
            return result;
        }

        if (tool != heldKind) {
            return AutoEquipEnabled && BorrowTool(player, tool, selection, heldKind) ? CrossingReady : NoCrossing;
        }
        // Re-run native validation against the current position and let the
        // game reserve landing space and enter its normal tool-ready state.
        const auto result = traverse(player, 0) & 1;
        TraceEntry(player, probe, automatic, hop, attempt.geometry, valid, result, &check);
        if (result) Logging.Debug("[QoL Enhancements/Controls] auto %s: native traversal requested\n",
                                  ladder ? "ladder" : "vault");
        return result;
    }
};
// The dispatcher can still report Move while a new action is queued. Observe
// actual action entries, rather than treating that old Move value as completion.
HOOK_DEFINE_TRAMPOLINE(AutoTraversalActionEntered) {
    static void Callback(void *player, unsigned action) {
        if (auto swap = SwapFor(player); swap && swap->stage != SwapStage::None) {
            if (swap->stage == SwapStage::Equipping) {
                if (action == ToolChangeAction) swap->equipEntered = true;
                if (action == MoveAction && swap->equipEntered) swap->equipFinished = true;
            } else {
                const bool traversal = swap->kind == LadderKind ? action >= 0x8a && action <= 0x8c
                                                               : action >= 0xb4 && action <= 0xb7;
                if (traversal) swap->traversalEntered = true;
                if (action == MoveAction && swap->traversalEntered) swap->traversalFinished = true;
            }
        }
        Orig(player, action);
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
        if (swap->stage == SwapStage::Equipping && !swap->equipFinished) return result;
        // Manual tool changes take ownership away from this transaction.
        if (Held(player) != swap->borrowed.item) { TraceSwap("ownership lost", swap->kind, swap->borrowed.slot); *swap = {}; return result; }
        if (swap->stage == SwapStage::Traversing) {
            if (!swap->traversalFinished) return result;
            // A real traversal state followed by Move proves completion, even
            // for a cancelled traversal that never left its starting bank.
            RestoreTool(player, *swap);
            return CrossingReady;
        }
        const auto x = Read<float>(player, field::StickX), y = Read<float>(player, field::StickY);
        const bool intent = x*x+y*y >= .81f && Read<unsigned>(actor, field::Facing) == swap->facing &&
            Read<std::array<float, 3>>(actor, field::Position) == swap->bank;
        const auto check = swap->kind == LadderKind ? ProbeCurrentPositionLadder(player) : ProbeCurrentPositionVault(player);
        if (!intent || !check.nativeResult || !check.landingValidated) {
            TraceSwap("cancelled", swap->kind, swap->borrowed.slot,
                      unsigned(intent) | (check.nativeResult << 1) | (unsigned(check.landingValidated) << 2));
            RestoreTool(player, *swap);
            return CrossingReady;
        }
        TraceSwap("traversing", swap->kind, swap->borrowed.slot);
        swap->stage = SwapStage::Traversing;
        const auto address = swap->kind == LadderKind ? address::TryLadder : address::TryVault;
        if (!(Native<unsigned (*)(void *, unsigned)>(address)(player, 0) & 1)) RestoreTool(player, *swap);
        return CrossingReady;
    }
};

}

bool InstallAutoTraversal(bool vault, bool ladder, bool autoEquip) {
    // Feature bits: 1 pole, 2 ladder, 3 shared dependency.
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; unsigned features = 3; };
    constexpr Fingerprint fingerprints[]{
#if CAS_AUTO_VAULT_DIAGNOSTIC
        {address::VaultCandidateRejected, {0x1100077b,0x71000f7f,0x54fffd40,0xf9403688,0xb4000428}, 1},
        {address::VaultSearchExit, {0x6d4a23e9,0x6d492beb,0x6d4833ed,0x6d473bef,0xa9504ff4}, 1},
        {address::MovementGuards, {0xb00144e8,0xf9414d08,0x79800108,0xb100051f,0x54000740}},
#endif
        {address::ContactNormals, {0xd2e7f009,0x52a7f008,0xf900003f,0xa900a428,0xf9405008}, 2},
        {address::TryLadder, {0xb9417808,0x34000068,0x120003e0,0xd65f03c0,0xd101c3ff}, 2},
        {address::LadderGeometry, {0xfc170fee,0x6d0133ed,0x6d022beb,0x6d0323e9,0xa9047bfd}, 2},
        {address::ValidLadderLanding, {0x36000075,0x52800028,0x17fffff2,0x528e1488,0x72a7efa8}, 2},
        {address::HopGeometry, {0xd10783ff,0x6d143bef,0x6d1533ed,0x6d162beb,0x6d1723e9}},
        {address::HopEntry, {0xd10243ff,0x6d0523e9,0xa9067bfd,0xa90757f6,0xa9084ff4}},
        {address::HopDirection, {0xd10643ff,0xa9137bfd,0xa9146ffc,0xa91567fa,0xa9165ff8}},
        {address::VaultPositionSelection, {0x7200027f,0xbd400008,0x52804f0a,0x5280508b,0x9a8a1178}, 1},
        {address::TryVault, {0xd10443ff,0x6d073bef,0x6d0833ed,0x6d092beb,0x6d0a23e9}, 1},
        {address::PrepareVault, {0x36000062,0x52800020,0xd65f03c0,0xd10343ff,0xfd0033e8}, 1},
        {address::EquippedToolKind, {0xd10183ff,0xa9037bfd,0xa90457f6,0xa9054ff4,0x9100c3fd}},
        {address::PlayerParameters, {0xd100c3ff,0xa9017bfd,0xf90013f3,0x910043fd,0x7103ac1f}},
        {address::MovementProbe, {0xaa1303e0,0x52800021,0x52800022,0x9403c60c,0xb90003e0}},
        {address::HopCommit, {0xaa1303e0,0x2a1f03e1,0x52800022,0x52800035,0x9403a11b}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    unsigned enabled = unsigned(vault) | (unsigned(ladder) << 1);
    const auto requested = enabled;
    for (const auto &f : fingerprints) {
        if (!(f.features & requested)) continue;
        if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + f.offset), f.words, sizeof(f.words))) {
            if (vault && (f.features & 1))
                Logging.Error("[QoL Enhancements/Controls] auto vault: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
            if (ladder && (f.features & 2))
                Logging.Error("[QoL Enhancements/Controls] auto ladder: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
            enabled &= ~f.features;
        }
    }
    if (!enabled) return false;
    VaultEnabled = enabled & 1;
    LadderEnabled = enabled & 2;
    Main = main.m_Text.m_Start;
    // A changed world-object dependency disables only this extension. Keep
    // the already confirmed handheld ladder and pole paths available.
    constexpr Fingerprint placedFingerprints[]{
        {address::TryPlacedLadder, {0xb9417808,0x34000068,0x120003e0,0xd65f03c0,0xd10483ff}},
        {address::PlacedObjectKind, {0xa9be7bfd,0xf9000bf3,0x910003fd,0xb4000481,0xb9400028}},
        {address::PlacedObjectCoordinates, {0xd10143ff,0xa9027bfd,0xa90357f6,0xa9044ff4,0x910083fd}},
    };
    PlacedLadderEnabled = LadderEnabled;
    if (LadderEnabled) for (const auto &f : placedFingerprints) {
        if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(Main + f.offset), f.words, sizeof(f.words))) {
            PlacedLadderEnabled = false;
            Logging.Error("[QoL Enhancements/Controls] auto placed ladder: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        }
    }
    constexpr Fingerprint swapFingerprints[]{
        {address::ActionEntered, {0xd10303ff,0x6d0523e9,0xa9067bfd,0xa9076ffc,0xa90867fa}},
        {address::ToolChangeEntry, {0xd101c3ff,0xa9037bfd,0xa9045ff8,0xa90557f6,0xa9064ff4}},
        {address::ToolChangeUpdate, {0xd10683ff,0xfd00abe8,0xf900b7fc,0xa915fbfd,0xa9175ff8}},
        {address::ToolChangeAnimationBranch, {0x39400ee8,0x34000b08,0x52800020,0x9402a25a,0xf949d809}},
        {address::StableActions, {0xd103c3ff,0xfd003bea,0x6d0823e9,0xa9097bfd,0xa90a6ffc}},
        {address::RequestAction, {0xa9be7bfd,0xf9000bf3,0x910003fd,0xb9001fa1,0xb9401fb3}},
        {address::PocketCapacity, {0xd100c3ff,0xa9017bfd,0xa9024ff4,0x910043fd,0x39400008}},
        {address::PocketItem, {0xa9bc7bfd,0xf9000bf7,0xa90257f6,0xa9034ff4,0x910003fd}},
        {address::IsEquippedSlot, {0xd10103ff,0xa9017bfd,0xa90257f6,0xa9034ff4,0x910043fd}},
        {address::HeldItem, {0xa9bd7bfd,0xf9000bf5,0xa9024ff4,0x910003fd,0xaa0803f3}},
        {address::ItemParameter, {0xa9bd7bfd,0xa90157f6,0xa9024ff4,0x910003fd,0xb0017b08}},
        {address::ItemToolKind, {0xf9400808,0xb4000488,0xb9400109,0x7100013f,0xfa491104}},
    };
    AutoEquipEnabled = autoEquip;
    if (autoEquip) for (const auto &f : swapFingerprints) {
        if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(Main + f.offset), f.words, sizeof(f.words))) {
            AutoEquipEnabled = false;
            Logging.Error("[QoL Enhancements/Controls] auto equip: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        }
    }
    if (AutoEquipEnabled) {
        AutoTraversalActionEntered::InstallAtOffset(address::ActionEntered);
        AutoTraversalSwap::InstallAtOffset(address::StableActions);
    }
#if CAS_AUTO_VAULT_DIAGNOSTIC
    if (VaultEnabled) {
        AutoVaultRejectedTrace::InstallAtOffset(address::VaultCandidateRejected);
        AutoVaultExitTrace::InstallAtOffset(address::VaultSearchExit);
    }
    AutoVaultMovementTrace::InstallAtOffset(address::MovementGuards);
    Logging.Log("[QoL Enhancements/Controls] DIAGNOSTIC: auto-vault-v4; bounded movement and entry traces\n");
#endif
    if (VaultEnabled) AutoVaultPrepare::InstallAtOffset(address::PrepareVault);
    if (LadderEnabled) {
        AutoLadderContact::InstallAtOffset(address::ContactNormals);
        AutoLadderLanding::InstallAtOffset(address::ValidLadderLanding);
    }
    AutoVaultGeometry::InstallAtOffset(address::HopGeometry);
    AutoVaultEntry::InstallAtOffset(address::HopEntry);
    if (LadderEnabled) Logging.Debug("[QoL Enhancements/Controls] auto ladder: READY; prototype 2; equipped ladder; native hop timer\n");
    if (PlacedLadderEnabled) Logging.Debug("[QoL Enhancements/Controls] auto placed ladder: READY; prototype 1; native object search\n");
    if (VaultEnabled) Logging.Debug("[QoL Enhancements/Controls] auto vault: READY; prototype 3; equipped pole; native hop timer\n");
    if (AutoEquipEnabled) Logging.Debug("[QoL Enhancements/Controls] auto equip: READY; prototype 2; pocket tool and restore\n");
    return enabled == requested && (!autoEquip || AutoEquipEnabled);
}
}
