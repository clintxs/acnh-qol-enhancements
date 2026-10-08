#pragma once
#include <cstdint>
#include <string_view>

namespace qol_pockets::features {
// This is the storage menu's own vanilla command number, not a pocket action ID.
inline constexpr std::int32_t MoveToPockets = 0;
inline bool IsOnlyMoveToPockets(int count, int action, const char *bank, const char *key,
                                std::uint32_t flags, std::uint32_t extra) {
    return count == 1 && action == MoveToPockets && flags == 0 && extra == 0 && bank && key &&
           std::string_view(bank) == "CmnSubWin" && std::string_view(key) == "0202";
}
} // namespace qol_pockets::features
