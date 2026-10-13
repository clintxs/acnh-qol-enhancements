#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "settings.hpp"
#include <cstring>

namespace qol::core {
namespace {
constexpr std::uintptr_t ScanTimeoutAssignment = 0x00bc4144;
unsigned TimeoutFrames{};
HOOK_DEFINE_INLINE(ScanTimeout) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // Native BC4140 has loaded the 450-update default into w8. The
        // original halfword store at BC4148 consumes this register next.
        registers->W[8] = TimeoutFrames;
    }
};
} // namespace
bool InstallAmiiboTimeout(unsigned frames) {
    if (!frames) {
        Logging.Debug("[QoL Enhancements/Core] Amiibo timeout: disabled; native behavior retained\n");
        return true;
    }
    if (frames > MaxAmiiboTimeoutFrames) return false;
    const auto &main = exl::util::GetMainModuleInfo();
    constexpr std::uint32_t original[]{0x52803848, 0xb900f67f, 0x7901f268,
                                       0xf94002a8, 0xb4000048, 0xf9401508};
    constexpr auto start = ScanTimeoutAssignment - sizeof(std::uint32_t);
    if (start + sizeof(original) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + start), original, sizeof(original))) {
        Logging.Error("[QoL Enhancements/Core] Amiibo timeout: DISABLED; instruction mismatch\n");
        return false;
    }
    TimeoutFrames = frames;

    qol::core::HookPlan hookPlan{
        qol::core::Site<ScanTimeout>("ScanTimeout", ScanTimeoutAssignment, {0xb900f67f, 0x7901f268, 0xf94002a8, 0xb4000048, 0xf9401508}, true),
    };
    if (!hookPlan) return false;
    if (!hookPlan.Publish<ScanTimeout>(ScanTimeoutAssignment)) return false;
    Logging.Debug("[QoL Enhancements/Core] Amiibo timeout: READY; C++ hook; frames=%u\n", TimeoutFrames);
    return true;
}
} // namespace qol::core
