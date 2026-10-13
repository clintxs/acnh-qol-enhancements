#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "../camera_tilt.hpp"
#include "../features/controls.hpp"
#include <cstring>

namespace qol::controls {
namespace {
namespace address {
constexpr std::uintptr_t CameraTiltEligibility = 0x0241dab4;
constexpr std::uintptr_t ServiceFlags = 0x0241dac4;
constexpr std::uintptr_t FallbackFlagsAndLockBit = 0x0241dae4;
constexpr std::uintptr_t GlobalLockCheck = 0x0241db08;
constexpr std::uintptr_t SwimStrideCheck = 0x0241dc6c;
constexpr std::uintptr_t ActionService = 0x053f5738;
constexpr std::uintptr_t FallbackActionFlags = 0x053f08d0;
}
constexpr std::size_t ActionFlagsOffset = 0x200;
constexpr unsigned CameraLocked = 1u << 9;
std::uintptr_t Main{};
template<class T> T Read(std::uintptr_t pointer) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void *>(pointer), sizeof(value));
    return value;
}
HOOK_DEFINE_TRAMPOLINE(SwimmingCameraTiltEligibility) {
    static bool Callback() {
        struct Camera {
            bool NativeTiltAllowed() const { return Orig(); }
            bool GloballyLocked() const {
                // Recheck after native snapshot/initialization. Exactly two
                // false exits are fingerprinted: bit 9 and surface SwimStride.
                const auto service = Read<std::uintptr_t>(Main + address::ActionService);
                const auto flags = service ? service + ActionFlagsOffset : Main + address::FallbackActionFlags;
                return Read<unsigned>(flags) & CameraLocked;
            }
        } camera;
        return features::AllowSwimmingCameraTilt(camera);
    }
};
}
bool InstallSwimmingCameraTilt() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
        {address::CameraTiltEligibility, {0xa9be7bfd,0xf9000bf3,0x910003fd,0x90017ec8,0xf9439d09}},
        {address::ServiceFlags, {0xf9439d09,0xb4000069,0x91080129,0x14000007,0xf0017e89}},
        {address::FallbackFlagsAndLockBit, {0xf0017e89,0x91234129,0x5280012a,0xb9001faa,0xb9401faa}},
        {address::GlobalLockCheck, {0xb86bd929,0x1aca2529,0x36000069,0x2a1f03e0,0x14000057}},
        {address::SwimStrideCheck, {0x7103dd1f,0x1a9f07e0,0xf9400bf3,0xa8c27bfd,0xd65f03c0}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &f : fingerprints) if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + f.offset), f.words, sizeof(f.words))) {
        Logging.Error("[QoL Enhancements/Controls] camera tilt: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        return false;
    }
    Main = main.m_Text.m_Start;

    qol::core::HookPlan hookPlan{
        qol::core::Site<SwimmingCameraTiltEligibility>("SwimmingCameraTiltEligibility", address::CameraTiltEligibility, {0xa9be7bfd, 0xf9000bf3, 0x910003fd, 0x90017ec8, 0xf9439d09}, true),
    };
    if (!hookPlan) return false;
    if (!hookPlan.Publish<SwimmingCameraTiltEligibility>(address::CameraTiltEligibility)) return false;
    Logging.Debug("[QoL Enhancements/Controls] camera tilt: READY; native tilt positions while swimming forward\n");
    return true;
}
}
