#include "lib.hpp"
#include "hint_text.hpp"
#include <array>
#include <cstring>

namespace qol::core {
namespace {
constexpr std::uintptr_t InitializeMessage = 0x01aea210;
constexpr std::uintptr_t SetPaneText = 0x03c1b588, SetGroupText = 0x03c47b90;
struct SafeString { std::uintptr_t vtable; const char *text; };
std::array<HintTextProvider, 4> HintProviders{};
bool Installed{};
template<class T> T &Field(std::uintptr_t object, unsigned offset) {
    return *reinterpret_cast<T *>(object + offset);
}
HOOK_DEFINE_TRAMPOLINE(InitializeHintCaption) {
    static void Callback(std::uintptr_t message, std::uintptr_t binding,
                         const SafeString *bank, const SafeString *key) {
        Orig(message, binding, bank, key);
        if (!message || !key || !key->text) return;
        const char16_t *caption = nullptr;
        for (auto provider : HintProviders) {
            if (provider && (caption = provider(key->text))) break;
        }
        if (!caption) return;
        // Native initialization retains the binding. State 12 skips MSBT
        // resolution; the normal destructor still cleans the message object.
        Field<unsigned>(message, 0xc) = 12;
        const auto adapter = binding ? Field<std::uintptr_t>(binding, 0x48) : 0;
        const auto methods = adapter ? Field<std::uintptr_t>(adapter, 0x18) : 0;
        const auto payload = adapter ? Field<std::uintptr_t>(adapter, 8) : 0;
        const auto setter = methods ? Field<std::uintptr_t>(methods, 0x5e0) : 0;
        const auto base = exl::util::modules::GetTargetStart();
        if (!payload || (setter != base + SetPaneText && setter != base + SetGroupText)) return;
        unsigned length = 0;
        while (caption[length]) ++length;
        reinterpret_cast<void (*)(std::uintptr_t, const char16_t *, unsigned)>(setter)(payload, caption, length);
    }
};
}
bool RegisterHintText(HintTextProvider provider) {
    if (!provider) return false;
    for (auto existing : HintProviders) if (existing == provider) return true;
    for (auto &slot : HintProviders) if (!slot) {
        if (!Installed) {
            constexpr std::array<std::uint32_t, 5> words{
                0xa9bc7bfd, 0xa9015ff8, 0xa90257f6, 0xa9034ff4, 0x910003fd};
            const auto &main = exl::util::GetMainModuleInfo();
            if (InitializeMessage + sizeof(words) > main.m_Text.m_Size ||
                std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + InitializeMessage),
                            words.data(), sizeof(words))) return false;
            InitializeHintCaption::InstallAtOffset(InitializeMessage);
            Installed = true;
        }
        slot = provider;
        return true;
    }
    return false;
}
}
