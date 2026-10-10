#include "lib.hpp"
#include "page_input.hpp"
#include <array>
#include <cstring>

namespace qol::core {
namespace {
constexpr std::uintptr_t InputAddress = 0x03c58d24;
std::array<PageInputHandler, 4> InputHandlers{};
bool Installed{};
HOOK_DEFINE_TRAMPOLINE(SharedPageInput) {
    static void Callback(void *page, void *snapshot, unsigned blocked) {
        for (auto handler : InputHandlers)
            if (handler && handler(page, snapshot, blocked)) return;
        Orig(page, snapshot, blocked);
    }
};
}
bool RegisterPageInput(PageInputHandler handler) {
    if (!handler) return false;
    for (auto existing : InputHandlers) if (existing == handler) return true;
    for (auto &slot : InputHandlers) if (!slot) {
        if (!Installed) {
            constexpr std::array<std::uint32_t, 5> words{
                0xf9410800, 0xb4000060, 0x12000042, 0x17ffcad4, 0xd65f03c0};
            const auto &main = exl::util::GetMainModuleInfo();
            if (InputAddress + sizeof(words) > main.m_Text.m_Size ||
                std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + InputAddress),
                            words.data(), sizeof(words))) return false;
            SharedPageInput::InstallAtOffset(InputAddress);
            Installed = true;
        }
        slot = handler;
        return true;
    }
    return false;
}
}
