#include "lib.hpp"
#include "auto_swim.hpp"
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
constexpr float StrongStick = .85f;
// Horizon system ticks are 19.2 MHz. Cadence does not depend on frame rate.
constexpr std::uint64_t StrokeInterval = 19'200'000ull * 2;
struct SwimState {
    const void *player{};
    unsigned handle{};
    std::uint64_t lastStroke{};
    float cruiseSpeed{};
    bool moving{}, underwater{}, armPending{};
};
std::array<SwimState, 8> Swimmers{};
struct MovementScope { void *player{}; SwimState *state{}; bool strong{}, underwater{}; };
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
    // Limit synthetic input to these synchronous native swimming routines.
    // Physical A always retains its original behavior, including light tilt.
    if (!player || Read<unsigned>(player, field::UpdateMode) != 0 ||
        !Read<unsigned char>(player, field::InputEnabled) ||
        Read<unsigned char>(player, field::InputLocked)) { original(player); return; }
    const auto resident = Read<unsigned char>(player, field::Resident);
    if (resident >= Swimmers.size()) { original(player); return; }
    auto &state = Swimmers[resident];
    const auto handle = Read<unsigned>(player, field::Handle);
    if (state.player != player || state.handle != handle || state.underwater != underwater)
        state = {player, handle, 0, 0.f, false, underwater};
    const auto held = Read<std::uint64_t>(player, field::HeldActions);
    const auto pressed = Read<std::uint64_t>(player, field::PressedActions);
    if (Scope.player != player && ((held | pressed) & 1)) {
        state.moving = false;
        state.cruiseSpeed = 0.f;
        state.armPending = false;
        original(player);
        return;
    }
    const auto x = Read<float>(player, field::LeftStick);
    const auto y = Read<float>(player, field::LeftStick + sizeof(float));
    const float magnitudeSquared = x*x + y*y;
    const bool strong = magnitudeSquared >= StrongStick*StrongStick && magnitudeSquared <= 2.f;
    const auto now = svcGetSystemTick();
    // A native stride may reject A until its current animation reaches the
    // next transition. Keep the stroke requested until stride entry consumes
    // it; a single-frame press / short animation window can miss that gate.
    if (!strong) {
        state.armPending = false;
        state.cruiseSpeed = 0.f;
    } else if (!state.moving || now - state.lastStroke >= StrokeInterval) {
        state.armPending = true;
    }
    state.moving = strong;
    const bool armEntry = entry && state.armPending;
    const bool pulse = strong && !entry && state.armPending;
    const auto frames = Read<unsigned>(player, field::HeldAFrames);
    if (strong) {
        Write(player, field::HeldActions, held | 1);
        Write(player, field::PressedActions, pressed | std::uint64_t(pulse));
        // Native stride entry chooses FlapFoot after >30 held updates. A
        // pending stroke must select arms even when entry arrives much later.
        Write(player, field::HeldAFrames, state.armPending ? 0u : 31u);
    }
    const auto previous = Scope;
    Scope = {player, &state, strong, underwater};
    original(player);
    Scope = previous;
    if (armEntry) {
        state.armPending = false;
        state.lastStroke = now;
    }
    // Restore only the borrowed A bits, preserving other native input writes.
    Write(player, field::HeldActions, (Read<std::uint64_t>(player, field::HeldActions) & ~1ull) | (held & 1));
    Write(player, field::PressedActions, (Read<std::uint64_t>(player, field::PressedActions) & ~1ull) | (pressed & 1));
    Write(player, field::HeldAFrames, frames);
}
void AdjustMovement(void *player, const void *parameters, bool stride) {
    if (Scope.player != player || !Scope.state) return;
    if (!Scope.strong) {
        // Native steering still ran. Remove propulsion immediately so a light
        // tilt can turn in place, even during a stroke's deceleration phase.
        Write(player, field::Speed, 0.f);
        Write(player, field::TargetSpeed, 0.f);
    } else if (Scope.underwater) {
        if (stride) {
            // StrideMovement receives the native slow cruise speed (already
            // scaled for FlapFoot). Use it as a floor between animation pulses.
            const float speed = Read<float>(parameters, 0);
            if (speed > 0.f && speed < 10.f) Scope.state->cruiseSpeed = speed;
        }
        const float cruise = Scope.state->cruiseSpeed;
        if (cruise > 0.f) {
            for (const auto offset : {field::Speed, field::TargetSpeed})
                if (Read<float>(player, offset) < cruise) Write(player, offset, cruise);
        }
    }
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
        return result;
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
        Logging.Error("[QoL Enhancements/Controls] auto swim: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
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
    Logging.Debug("[QoL Enhancements/Controls] auto swim: READY; prototype 4; strong stick; 2s strokes; smooth diving\n");
    return true;
}
}
