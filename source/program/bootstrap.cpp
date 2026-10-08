#include "lib.hpp"
#include "core/modules.hpp"
#include "core/release.hpp"
#include <array>
#include <cstring>

namespace {
constexpr std::uintptr_t ContentMountCall = 0x037b5460;
constexpr std::uintptr_t ContentMounted = ContentMountCall + 4;
// Original call: nn::fs::MountRom("content", cache, size). Activate after it,
// before startup continues initializing resources. Preserve all GPRs and SIMD
// registers while installing features; the adapter runs displaced instructions.
HOOK_DEFINE_INLINE(AfterContentMount) {
    static void Callback(exl::hook::InlineFloatCtx *) {
        qol::core::ActivateModules();
    }
};
}
extern "C" void exl_main(void *, void *) {
    Logging.Log("[QoL Enhancements] ENTRY Core %s; slot=subsdk6\n", qol::core::ReleaseVersion);
    const auto &main = exl::util::GetMainModuleInfo();
    constexpr std::array<std::uint32_t, 6> expected{0x941d4094, 0xf94002d5, 0xb5fff9f5, 0x52801100, 0x972a4914, 0xaa1403e1};
    if (ContentMountCall + sizeof(expected) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + ContentMountCall),
                    expected.data(), sizeof(expected)) != 0) {
        Logging.Log("[QoL Enhancements] PROFILE_MISMATCH: unsupported or patched startup; Core disabled\n");
        return;
    }
    exl::hook::Initialize();
    AfterContentMount::InstallAtOffset(ContentMounted);
    Logging.Log("[QoL Enhancements] WAITING_FOR_CONTENT: module configs load after the game mounts RomFS\n");
}
extern "C" NORETURN void exl_exception_entry() {
    EXL_ABORT("QoL Enhancements: unexpected exception entry");
}
