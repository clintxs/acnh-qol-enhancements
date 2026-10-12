#include "lib.hpp"
#include "auto_swim.hpp"
#include <array>
#include <cstring>

namespace qol::swimming {
namespace {
namespace address {
constexpr std::uintptr_t SurfaceWait = 0x0236b77c;
constexpr std::uintptr_t SurfaceStride = 0x0236bea0;
constexpr std::uintptr_t SurfaceEntry = 0x0236bbb8;
constexpr std::uintptr_t DiveWait = 0x0236cf20;
constexpr std::uintptr_t DiveStride = 0x0236db00;
constexpr std::uintptr_t DiveEntry = 0x0236d6a0;
constexpr std::uintptr_t WaitMovement = 0x02364b10;
constexpr std::uintptr_t StrideMovement = 0x02365878;
}
namespace field {
constexpr std::size_t UpdateMode = 0x178, InputEnabled = 0x12f8, InputLocked = 0x17ac;
constexpr std::size_t LeftStick = 0xeec, HeldActions = 0xec8, PressedActions = 0xed0;
constexpr std::size_t HeldAFrames = 0x13d8, Resident = 0x1ee8, Handle = 0x230;
constexpr std::size_t Speed = 0x1820, TargetSpeed = 0x1844;
}
// Small neutral zone, then a linear analog speed range. Native steering still
// uses the game's own stick handling; this only sets swimming propulsion.
constexpr float StickDeadZone = .15f;
constexpr float QuietStrideScale = .8f; // Native FlapFoot speed multiplier.
constexpr float BoostFraction = .5f;
constexpr std::uint64_t TicksPerSecond = 19'200'000ull;
constexpr std::uint64_t StrokeInterval = TicksPerSecond * 2;
constexpr std::uint64_t BoostDelay = TicksPerSecond / 2;
constexpr std::uint64_t BoostDuration = TicksPerSecond * 3 / 2;
constexpr std::uint64_t SessionGap = TicksPerSecond / 2;
struct SwimState {
    const void *player{};
    unsigned handle{};
    std::uint64_t lastStroke{}, lastUpdate{};
    float cruiseSpeed{};
    bool moving{}, underwater{}, armPending{}, pendingManual{};
    bool hasStroke{}, armAnimation{};
};
std::array<SwimState, 8> Swimmers{};
struct MovementScope {
    void *player{};
    SwimState *state{};
    float throttle{}, boost{};
};
MovementScope Scope{};
template<class T> T Read(const void *object, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const std::byte *>(object) + offset, sizeof(value));
    return value;
}
template<class T> void Write(void *object, std::size_t offset, T value) {
    std::memcpy(static_cast<std::byte *>(object) + offset, &value, sizeof(value));
}
float StickThrottle(const void *player) {
    const auto x = Read<float>(player, field::LeftStick);
    const auto y = Read<float>(player, field::LeftStick + sizeof(float));
    const float squared = x*x + y*y;
    // Reject corrupt/nonfinite inputs before sqrt; clamp diagonal overtravel.
    if (!(squared > StickDeadZone*StickDeadZone && squared <= 2.f)) return 0.f;
    const float magnitude = __builtin_sqrtf(squared);
    return magnitude >= 1.f ? 1.f : (magnitude - StickDeadZone) / (1.f - StickDeadZone);
}
float BoostEnvelope(const SwimState &state, std::uint64_t now) {
    const auto elapsed = now - state.lastStroke;
    // Match the arm push rather than the start of the animation. Keep the
    // full 1.5-second fade after this delay, independent of stroke cooldown.
    if (!state.hasStroke || elapsed < BoostDelay || elapsed >= BoostDelay + BoostDuration) return 0.f;
    return 1.f - float(elapsed - BoostDelay) / float(BoostDuration);
}
void Update(void *player, void (*original)(void *), bool underwater, bool entry) {
    if (!player || Read<unsigned>(player, field::UpdateMode) != 0 ||
        !Read<unsigned char>(player, field::InputEnabled) ||
        Read<unsigned char>(player, field::InputLocked)) { original(player); return; }
    const auto resident = Read<unsigned char>(player, field::Resident);
    if (resident >= Swimmers.size()) { original(player); return; }
    auto &state = Swimmers[resident];
    const auto handle = Read<unsigned>(player, field::Handle);
    if (state.player != player || state.handle != handle) {
        state = {};
        state.player = player;
        state.handle = handle;
    }
    const auto now = svcGetSystemTick();
    // Preserve the boost cooldown across diving/surfacing, but relearn each
    // mode's native cruise speed. A gap identifies a new swimming session.
    if (state.underwater != underwater || now - state.lastUpdate > SessionGap) {
        state.cruiseSpeed = 0.f;
        state.moving = state.armPending = state.pendingManual = state.armAnimation = false;
    }
    state.underwater = underwater;
    state.lastUpdate = now;
    const auto held = Read<std::uint64_t>(player, field::HeldActions);
    const auto pressed = Read<std::uint64_t>(player, field::PressedActions);
    const float throttle = StickThrottle(player);
    const bool moving = throttle > 0.f;
    const bool nested = Scope.player == player;
    const bool ready = !state.hasStroke || now - state.lastStroke >= StrokeInterval;
    if (!nested && !entry) {
        if (!moving && !state.pendingManual) state.armPending = false;
        const bool manual = ((held | pressed) & 1) != 0;
        if (ready && (manual || (moving && !state.moving))) {
            state.armPending = true;
            state.pendingManual = manual;
        }
        state.moving = moving;
    }
    // Y keeps priority over a queued stroke when diving/surfacing/catching.
    const bool transition = (pressed & 8) != 0;
    if (transition) state.armPending = state.pendingManual = false;
    const bool armEntry = entry && state.armPending;
    // Consume before native entry so a nested movement helper can see the
    // boost immediately. Entries between requests select quiet foot kicking.
    if (entry) {
        state.armAnimation = armEntry;
        if (armEntry) {
            state.armPending = false;
            state.pendingManual = false;
            state.hasStroke = true;
            state.lastStroke = now;
        }
    }
    const float boost = BoostEnvelope(state, now);
    // Keep a neutral-stick stroke active through its wind-up as well as the
    // impulse, so manual A does not lose its animation before propulsion.
    const bool strokeActive = state.hasStroke && now - state.lastStroke < BoostDelay + BoostDuration;
    const bool active = !transition && (moving || state.armPending || strokeActive);
    const bool pulse = !entry && state.armPending && !transition;
    const auto frames = Read<unsigned>(player, field::HeldAFrames);
    // A is an input to our cooldown policy. Hide physical A from the native
    // stroke gates during cooldown; restore it after this synchronous scope.
    Write(player, field::HeldActions, (held & ~1ull) | std::uint64_t(active));
    Write(player, field::PressedActions, (pressed & ~1ull) | std::uint64_t(pulse));
    Write(player, field::HeldAFrames, (armEntry || state.armPending) ? 0u : 31u);
    const auto previous = Scope;
    Scope = {player, &state, throttle, boost};
    original(player);
    Scope = previous;
    Write(player, field::HeldActions, (Read<std::uint64_t>(player, field::HeldActions) & ~1ull) | (held & 1));
    Write(player, field::PressedActions, (Read<std::uint64_t>(player, field::PressedActions) & ~1ull) | (pressed & 1));
    Write(player, field::HeldAFrames, frames);
}
void AdjustMovement(void *player, const void *parameters, bool stride) {
    if (Scope.player != player || !Scope.state) return;
    if (stride) {
        // Both native stride callers pass their full-speed parameter in slot
        // zero, scaled by .8 for FlapFoot. Normalize to quiet cruise so changing
        // animation does not change analog speed or introduce diving pauses.
        const float nativeSpeed = Read<float>(parameters, 0);
        if (nativeSpeed > 0.f && nativeSpeed < 10.f)
            Scope.state->cruiseSpeed = nativeSpeed * (Scope.state->armAnimation ? QuietStrideScale : 1.f);
    }
    // Replace the animation's start/stop speed curve with analog propulsion
    // plus a time-based stroke impulse. Neutral A boosts use facing.
    const float speed = Scope.state->cruiseSpeed * (Scope.throttle + BoostFraction * Scope.boost);
    Write(player, field::Speed, speed);
    Write(player, field::TargetSpeed, speed);
}
HOOK_DEFINE_TRAMPOLINE(AutoSwimSurfaceWait) {
    static void Callback(void *player) { Update(player, [](void *p) { Orig(p); }, false, false); }
};
HOOK_DEFINE_TRAMPOLINE(AutoSwimSurfaceStride) {
    static void Callback(void *player) { Update(player, [](void *p) { Orig(p); }, false, false); }
};
HOOK_DEFINE_TRAMPOLINE(AutoSwimSurfaceEntry) {
    static void Callback(void *player) { Update(player, [](void *p) { Orig(p); }, false, true); }
};
HOOK_DEFINE_TRAMPOLINE(AutoSwimDiveWait) {
    static void Callback(void *player) { Update(player, [](void *p) { Orig(p); }, true, false); }
};
HOOK_DEFINE_TRAMPOLINE(AutoSwimDiveStride) {
    static void Callback(void *player) { Update(player, [](void *p) { Orig(p); }, true, false); }
};
HOOK_DEFINE_TRAMPOLINE(AutoSwimDiveEntry) {
    static void Callback(void *player) { Update(player, [](void *p) { Orig(p); }, true, true); }
};
HOOK_DEFINE_TRAMPOLINE(AutoSwimWaitMovement) {
    static unsigned Callback(void *player, const void *parameters) {
        const auto result = Orig(player, parameters);
        AdjustMovement(player, parameters, false);
        return result;
    }
};
HOOK_DEFINE_TRAMPOLINE(AutoSwimStrideMovement) {
    static unsigned Callback(void *player, const void *parameters) {
        const auto result = Orig(player, parameters);
        AdjustMovement(player, parameters, true);
        // Native stride updates skip the A request gate when this helper
        // returns >1 (e.g. the current animation is still playing). A ready
        // boost must interrupt that cycle instead of waiting an unpredictable
        // amount of time. Type 0 is the native ordinary stride request; retain
        // all native input/world guards and the result outside a pending boost.
        return Scope.player == player && Scope.state && Scope.state->armPending ? 0u : result;
    }
};
}
bool InstallAutoSwim() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
        {address::SurfaceWait, {0xd104c3ff,0xfd0073e8,0xa90f7bfd,0xa9105ffc,0xa91157f6}},
        {address::SurfaceStride, {0xd101c3ff,0xfd001be8,0xa9047bfd,0xa90557f6,0xa9064ff4}},
        {address::SurfaceEntry, {0xd10143ff,0xfd0013e8,0xf9001ff5,0xa902fbfd,0xa9044ff4}},
        {address::DiveWait, {0xd10503ff,0x6d0f23e9,0xa9107bfd,0xa9115ffc,0xa91257f6}},
        {address::DiveStride, {0xd101c3ff,0x6d0323e9,0xa9047bfd,0xa90557f6,0xa9064ff4}},
        {address::DiveEntry, {0xd101c3ff,0x6d0123e9,0xf9001bf9,0xa9027bfd,0xa9045ff8}},
        {address::WaitMovement, {0xd105c3ff,0x6d1023e9,0xa9117bfd,0xa9126ffc,0xa91367fa}},
        {address::StrideMovement, {0xd10683ff,0x6d1423e9,0xf900b3fc,0xa9157bfd,0xa9175ff8}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &f : fingerprints) if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + f.offset), f.words, sizeof(f.words))) {
        Logging.Error("[QoL Enhancements/Swimming] auto swim: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        return false;
    }
    AutoSwimSurfaceWait::InstallAtOffset(address::SurfaceWait);
    AutoSwimSurfaceStride::InstallAtOffset(address::SurfaceStride);
    AutoSwimSurfaceEntry::InstallAtOffset(address::SurfaceEntry);
    AutoSwimDiveWait::InstallAtOffset(address::DiveWait);
    AutoSwimDiveStride::InstallAtOffset(address::DiveStride);
    AutoSwimDiveEntry::InstallAtOffset(address::DiveEntry);
    AutoSwimWaitMovement::InstallAtOffset(address::WaitMovement);
    AutoSwimStrideMovement::InstallAtOffset(address::StrideMovement);
    Logging.Debug("[QoL Enhancements/Swimming] auto swim: READY; prototype 10; analog speed; delayed 1.5s 50 percent boosts; interruptible strokes\n");
    return true;
}
}
