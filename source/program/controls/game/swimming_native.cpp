#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "../auto_swim.hpp"
#include "../features/swimming.hpp"
#include <array>
#include <cstring>

namespace qol::controls {
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
// Identity is native-owned; stroke timing/animation policy is mod-owned.
struct SwimState : features::StrokeState { const void *player{}; unsigned handle{}; };
bool StrokeBoost = true;
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
    const auto held = Read<std::uint64_t>(player, field::HeldActions);
    const auto pressed = Read<std::uint64_t>(player, field::PressedActions);
    const auto throttle = features::StickThrottle(Read<float>(player, field::LeftStick),
                                                 Read<float>(player, field::LeftStick + sizeof(float)));
    const auto stroke = features::SwimWithStick(state, now, underwater, entry, Scope.player == player,
                                               throttle, ((held | pressed) & 1) != 0, (pressed & 8) != 0);
    const auto frames = Read<unsigned>(player, field::HeldAFrames);
    // A is an input to our cooldown policy. Hide physical A from the native
    // stroke gates during cooldown; restore it after this synchronous scope.
    Write(player, field::HeldActions, (held & ~1ull) | std::uint64_t(stroke.active));
    Write(player, field::PressedActions, (pressed & ~1ull) | std::uint64_t(stroke.pulse));
    Write(player, field::HeldAFrames, (stroke.armEntry || state.armPending) ? 0u : 31u);
    const auto previous = Scope;
    Scope = {player, &state, throttle, stroke.boost};
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
            Scope.state->cruiseSpeed = features::LearnCruiseSpeed(nativeSpeed, Scope.state->armAnimation);
    }
    // Replace the animation's start/stop speed curve with analog propulsion
    // plus a time-based stroke impulse. Neutral A boosts use facing.
    const float speed = features::SwimmingPropulsion(Scope.state->cruiseSpeed, Scope.throttle, Scope.boost, StrokeBoost);
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
bool InstallAutoSwim(bool strokeBoost) {
    StrokeBoost = strokeBoost;
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
        Logging.Error("[QoL Enhancements/Controls] auto swim: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        return false;
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<AutoSwimSurfaceWait>("AutoSwimSurfaceWait", address::SurfaceWait, {0xd104c3ff, 0xfd0073e8, 0xa90f7bfd, 0xa9105ffc, 0xa91157f6}, true),
        qol::core::Site<AutoSwimSurfaceStride>("AutoSwimSurfaceStride", address::SurfaceStride, {0xd101c3ff, 0xfd001be8, 0xa9047bfd, 0xa90557f6, 0xa9064ff4}, true),
        qol::core::Site<AutoSwimSurfaceEntry>("AutoSwimSurfaceEntry", address::SurfaceEntry, {0xd10143ff, 0xfd0013e8, 0xf9001ff5, 0xa902fbfd, 0xa9044ff4}, true),
        qol::core::Site<AutoSwimDiveWait>("AutoSwimDiveWait", address::DiveWait, {0xd10503ff, 0x6d0f23e9, 0xa9107bfd, 0xa9115ffc, 0xa91257f6}, true),
        qol::core::Site<AutoSwimDiveStride>("AutoSwimDiveStride", address::DiveStride, {0xd101c3ff, 0x6d0323e9, 0xa9047bfd, 0xa90557f6, 0xa9064ff4}, true),
        qol::core::Site<AutoSwimDiveEntry>("AutoSwimDiveEntry", address::DiveEntry, {0xd101c3ff, 0x6d0123e9, 0xf9001bf9, 0xa9027bfd, 0xa9045ff8}, true),
        qol::core::Site<AutoSwimWaitMovement>("AutoSwimWaitMovement", address::WaitMovement, {0xd105c3ff, 0x6d1023e9, 0xa9117bfd, 0xa9126ffc, 0xa91367fa}, true),
        qol::core::Site<AutoSwimStrideMovement>("AutoSwimStrideMovement", address::StrideMovement, {0xd10683ff, 0x6d1423e9, 0xf900b3fc, 0xa9157bfd, 0xa9175ff8}, true),
    };
    if (!hookPlan) return false;
    if (!hookPlan.Publish<AutoSwimSurfaceWait>(address::SurfaceWait)) return false;
    if (!hookPlan.Publish<AutoSwimSurfaceStride>(address::SurfaceStride)) return false;
    if (!hookPlan.Publish<AutoSwimSurfaceEntry>(address::SurfaceEntry)) return false;
    if (!hookPlan.Publish<AutoSwimDiveWait>(address::DiveWait)) return false;
    if (!hookPlan.Publish<AutoSwimDiveStride>(address::DiveStride)) return false;
    if (!hookPlan.Publish<AutoSwimDiveEntry>(address::DiveEntry)) return false;
    if (!hookPlan.Publish<AutoSwimWaitMovement>(address::WaitMovement)) return false;
    if (!hookPlan.Publish<AutoSwimStrideMovement>(address::StrideMovement)) return false;
    Logging.Debug("[QoL Enhancements/Controls] auto swim: READY; analog speed; delayed 1.5s 62.5 percent boosts=%s; interruptible strokes\n",
                  StrokeBoost ? "true" : "false");
    return true;
}
}
