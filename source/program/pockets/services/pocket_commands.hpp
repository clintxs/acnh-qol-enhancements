#pragma once
#include <span>
#include "program/pockets/game/item_menu.hpp"

namespace qol_pockets::commands {
// These are UI snapshot bits, deliberately not HID button masks.
enum class UiButton : unsigned { Y = 1u << 7, ZL = 1u << 10, ZR = 1u << 11 };
enum class PocketAction { PutInStorage, DropOrRelease, EquipOrConsume, OpenContainer };
struct ShortcutRule { UiButton button; PocketAction action; };
constexpr bool ValidShortcuts(std::span<const ShortcutRule> rules) {
    if (rules.empty() || rules.size() > 3) return false;
    unsigned seen = 0;
    for (const auto &rule : rules) {
        if (rule.button != UiButton::Y && rule.button != UiButton::ZL && rule.button != UiButton::ZR) return false;
        if (rule.action != PocketAction::PutInStorage && rule.action != PocketAction::DropOrRelease &&
            rule.action != PocketAction::EquipOrConsume) return false;
        const auto bit = static_cast<unsigned>(rule.button);
        if (seen & bit) return false;
        seen |= bit;
    }
    return true;
}
// Valid declarations are copied into three owned slots at startup. Identical
// repeats revalidate hooks; conflicts leave the active table unchanged.
// Installation owns its common input loop and verified native phase hooks.
struct MenuPolicy {
    game::ChooseMenuEntry choose{};
    game::FeedbackTiming feedback{};
    bool cursorTap{};
    bool operator==(const MenuPolicy &) const = default;
    bool Valid() const {
        return choose && (feedback == game::FeedbackTiming::None ||
            feedback == game::FeedbackTiming::BeforeExecution || feedback == game::FeedbackTiming::AfterSuccess);
    }
};
struct MenuPolicies {
    MenuPolicy store{}, drop{}, use{}, open{};
    bool operator==(const MenuPolicies &) const = default;
    bool Valid() const { return store.Valid() && drop.Valid() && use.Valid() && open.Valid(); }
};
// Feature composition supplies policy; the service owns deferred execution.
bool InstallCommandHandlers(std::span<const ShortcutRule> rules, MenuPolicies policies);
bool InstallShortcuts(std::span<const ShortcutRule> rules);
} // namespace qol_pockets::commands
