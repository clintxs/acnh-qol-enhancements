#include "lib.hpp"
#include "auto_swim.hpp"
#include <cstring>

namespace qol::controls {
namespace {
namespace address {
constexpr std::uintptr_t SurfaceStrokeGate = 0x02367e3c;
constexpr std::uintptr_t UnderwaterStrokeGate = 0x0236d558;
constexpr std::uintptr_t PlayerParameters = 0x0241e98c;
}
namespace field {
constexpr std::size_t UpdateMode = 0x178, InputEnabled = 0x12f8;
constexpr std::size_t LeftStick = 0xeec, StickDeadzone = 0x120;
constexpr std::size_t ParameterValue = 0xf0;
}
std::uintptr_t Main{};
template<class T> T Read(const void *object, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const std::byte *>(object) + offset, sizeof(value));
    return value;
}
bool WantsStroke(const void *player) {
    // Match the native swim movement's input-enable flag and parameterized
    // stick deadzone. Remote/replay updates must not manufacture local input.
    if (!player || Read<unsigned>(player, field::UpdateMode) == 1 ||
        !Read<unsigned char>(player, field::InputEnabled)) return false;
    const auto x = Read<float>(player, field::LeftStick);
    const auto y = Read<float>(player, field::LeftStick + sizeof(float));
    const auto parameters = reinterpret_cast<const std::byte *(*)(unsigned)>(Main + address::PlayerParameters)(1);
    if (!parameters) return false;
    const auto parameter = parameters + field::StickDeadzone;
    const auto vtable = Read<const void *>(parameter, 0);
    if (!vtable) return false;
    const auto getter = Read<const float *(*)(const void *)>(vtable, field::ParameterValue);
    const auto value = getter ? getter(parameter) : nullptr;
    if (!value) return false;
    const float deadzone = *value;
    // Ordered comparisons reject NaNs; the upper bound rejects corrupt input.
    const float magnitudeSquared = x * x + y * y;
    return deadzone >= 0.f && deadzone < 1.f &&
           magnitudeSquared > deadzone * deadzone && magnitudeSquared <= 2.f;
}
HOOK_DEFINE_INLINE(AutoSwimSurface) {
    static void Callback(exl::hook::InlineCtx *ctx) {
        if (WantsStroke(reinterpret_cast<const void *>(ctx->X[19]))) ctx->W[9] |= 1;
    }
};
HOOK_DEFINE_INLINE(AutoSwimUnderwater) {
    static void Callback(exl::hook::InlineCtx *ctx) {
        if (WantsStroke(reinterpret_cast<const void *>(ctx->X[19]))) ctx->W[8] |= 1;
    }
};
}
bool InstallAutoSwim() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
        {address::SurfaceStrokeGate, {0x37000569,0xb81fc3bf,0xb85fc3a9,0x7100dd3f,0x54000142}},
        {address::UnderwaterStrokeGate, {0x37000168,0xb81ec3bf,0xb85ec3a8,0x7100dd1f,0x54fffd02}},
        {address::PlayerParameters, {0xd100c3ff,0xa9017bfd,0xf90013f3,0x910043fd,0x7103ac1f}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &f : fingerprints) if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + f.offset), f.words, sizeof(f.words))) {
        Logging.Error("[QoL Enhancements/Controls] auto swim: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        return false;
    }
    Main = main.m_Text.m_Start;
    // These branches are reached only after the native swim-specific input
    // locks. Extend only the stroke test; Y dive/surface, catches, exits and
    // animation timing retain their native input and scheduling.
    AutoSwimSurface::InstallAtOffset(address::SurfaceStrokeGate);
    AutoSwimUnderwater::InstallAtOffset(address::UnderwaterStrokeGate);
    Logging.Debug("[QoL Enhancements/Controls] auto swim: READY; surface and underwater stick strokes\n");
    return true;
}
}
