#include "lib.hpp"
#include <cstring>

namespace qol::amiibo {
namespace {
constexpr std::uintptr_t ScanTimeoutAssignment = 0x00bc4144;
constexpr std::uint32_t FiveMinuteUpdates = 9000;
HOOK_DEFINE_INLINE(ScanTimeout) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // Native BC4140 has loaded the 450-update default into w8. The
        // original halfword store at BC4148 consumes this register next.
        registers->W[8] = FiveMinuteUpdates;
    }
};
} // namespace
bool Install() {
    const auto &main = exl::util::GetMainModuleInfo();
    constexpr std::uint32_t original[]{0x52803848, 0xb900f67f, 0x7901f268,
                                       0xf94002a8, 0xb4000048, 0xf9401508};
    constexpr auto start = ScanTimeoutAssignment - sizeof(std::uint32_t);
    if (start + sizeof(original) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + start), original, sizeof(original))) {
        Logging.Log("[QoL Enhancements/Amiibo] DISABLED: scan timeout instruction mismatch\n");
        return false;
    }
    ScanTimeout::InstallAtOffset(ScanTimeoutAssignment);
    Logging.Log("[QoL Enhancements/Amiibo] READY: C++ scan timeout hook; updates=%u\n", FiveMinuteUpdates);
    return true;
}
} // namespace qol::amiibo
