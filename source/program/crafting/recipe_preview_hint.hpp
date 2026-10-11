#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace qol::crafting::recipe_hint {
inline constexpr std::uintptr_t BuildFooter = 0x02e9c670, AppendHint = 0x02de2b00;
inline constexpr std::uintptr_t FooterTypeGot = 0x04ac7278, FooterGuardGot = 0x04ac7280;
inline constexpr std::uintptr_t BufferedStringGot = 0x04ab2bc0;
inline constexpr std::size_t PendingCount = 0x218;
template<class T> T &Field(void *object, std::size_t offset = 0) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(object) + offset);
}
// Native KeyGuide descriptor, copied synchronously by 02DE2B00.
struct Hint {
    int button = 4, secondary = -1, otherButton = -1, otherSecondary = -1;
    std::uintptr_t stringVtable{};
    const char *caption{};
    int capacity = 64;
    char key[64] = "1201"; // Existing localized KeyGuide Select caption.
    std::uint32_t padding{};
    std::uint8_t player = 0xff, playerPadding[3]{};
    int appearance{};
    std::uint8_t alternate{}, alternatePadding[3]{};
    int alternateButton = -1, alternateSecondary = -1;
};
static_assert(sizeof(Hint) == 0x80 && offsetof(Hint, key) == 0x24);
inline void *Footer(std::uintptr_t main, void *context) {
    auto footer = context ? Field<void *>(context, 0x58) : nullptr;
    auto guard = *reinterpret_cast<const std::uint8_t **>(main + FooterGuardGot);
    if (!footer || !guard || !(*guard & 1)) return nullptr;
    auto type = *reinterpret_cast<void **>(main + FooterTypeGot);
    auto isType = Field<unsigned (*)(void *, void *)>(Field<void *>(footer));
    return (isType(footer, type) & 1) ? footer : nullptr;
}
inline void Append(std::uintptr_t main, void *footer, unsigned resident) {
    if (!footer) return;
    const auto count = Field<int>(footer, PendingCount);
    auto entries = Field<Hint **>(footer, 0x220);
    if (count < 0 || count >= 5 || count >= Field<int>(footer, 0x21c) ||
        !entries || !Field<void *>(footer, 0x228)) return;
    for (int i = 0; i < count; ++i)
        if (entries[i] && entries[i]->button == 4 && entries[i]->caption &&
            std::strcmp(entries[i]->caption, "1201") == 0) return;
    Hint hint;
    hint.player = resident < 8 ? resident : 0xff;
    hint.stringVtable = *reinterpret_cast<const std::uintptr_t *>(main + BufferedStringGot) + 16;
    hint.caption = hint.key;
    reinterpret_cast<void (*)(void *, const Hint *, unsigned)>(main + AppendHint)(footer, &hint, 1);
}
} // namespace qol::crafting::recipe_hint
