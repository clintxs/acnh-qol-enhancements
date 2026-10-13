#pragma once
#include <cstdint>
#include <string_view>
namespace qol_pockets::game {
// Internal native storage command number, distinct from pocket action IDs.
inline constexpr std::int32_t MoveToPockets = 0;
inline bool IsMoveToPockets(int action, const char *bank, const char *key) {
    return action == MoveToPockets && bank && key &&
        std::string_view(bank) == "CmnSubWin" && std::string_view(key) == "0202";
}
}
