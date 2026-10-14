#pragma once
#include <cstdint>
#include "program/pockets/game/item_menu.hpp"
#include "program/pockets/game/pocket_footer.hpp"
#include "program/pockets/game/stack_menu.hpp"
#include "program/core/feature_activation.hpp"

namespace qol_pockets::features {
// Pockets behavior starts here. These are the handlers used by production:
// adapters supply native availability, validate requests and perform execution.

// Native Grab 1 availability is authoritative, including turnip exclusion.
// Do not reproduce item-type/unlock checks here.
inline void AddGrabFiveAndTen(game::StackMenu &menu, int stackSize, bool hasFreePocket) {
    using game::Action;
    if (!hasFreePocket || stackSize <= 5 || !menu.Contains(Action::GrabOne) ||
        menu.Contains(Action::GrabFive) || !menu.Space()) return;
    // With one free row, preserve the shipped preference for Grab 5.
    const bool addTen = stackSize > 10 && menu.Space() >= 2;
    if (!menu.InsertAfter(Action::GrabOne, Action::GrabFive)) return;
    if (addTen) menu.InsertAfter(Action::GrabFive, Action::GrabTen);
}

using game::ItemAction;
using game::MenuEntries;

inline std::optional<std::size_t> DropOrRelease(MenuEntries entries) {
    for (std::size_t i = 0; i < entries.size(); ++i)
        if (entries[i].executable && (entries[i].action == ItemAction::Drop ||
                                     entries[i].action == ItemAction::Release)) return i;
    return std::nullopt;
}
inline std::optional<std::size_t> EquipOrConsume(MenuEntries entries) {
    // Preserve native order within each group. Equipment wins even when a
    // consumable action appears earlier in the native menu.
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto &entry = entries[i];
        if (entry.executable && (entry.action == ItemAction::Wear ||
            entry.action == ItemAction::Hold || entry.action == ItemAction::PutAway)) return i;
    }
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto &entry = entries[i];
        if (entry.executable && (entry.action == ItemAction::Eat ||
            entry.action == ItemAction::EatOne || entry.action == ItemAction::Drink)) return i;
    }
    return std::nullopt; // Eat 10 Turnips has no supported consume meaning.
}
inline std::optional<std::size_t> StoreAtHome(MenuEntries entries) {
    for (std::size_t i = 0; i < entries.size(); ++i)
        if (entries[i].executable && entries[i].action == ItemAction::Store) return i;
    return std::nullopt;
}
inline std::optional<std::size_t> OpenContainerDirectly(MenuEntries entries) {
    if (entries.size() != 1) return std::nullopt;
    const auto &entry = entries.front();
    if (!entry.executable || entry.action != ItemAction::Open) return std::nullopt;
    return 0;
}
inline std::optional<std::size_t> WithdrawDirectly(MenuEntries entries) {
    if (entries.size() != 1) return std::nullopt;
    const auto &entry = entries.front();
    if (!entry.executable || entry.action != ItemAction::MoveToPockets) return std::nullopt;
    return 0;
}
inline std::optional<std::size_t> ClaimDirectly(MenuEntries entries) {
    if (entries.size() != 1) return std::nullopt;
    const auto &entry = entries.front();
    if (!entry.executable || entry.action != ItemAction::Claim) return std::nullopt;
    return 0;
}

inline bool CanHoldSelect(game::SelectionContext context, int selectionLimit) {
    return (context == game::SelectionContext::ItemSelection || context == game::SelectionContext::HomeStorage) &&
        (selectionLimit == -1 || selectionLimit > 1);
}

// One gesture repeats the first press's selection intent. Native browsing still
// toggles items and enforces capacity/selection limits; we only request a click.
class HoldSelection {
    bool released_{}, holding_{}, selecting_{};
    std::uint64_t startedAt_{};
public:
    static constexpr std::uint64_t StartDelayTicks = 19'200'000 / 4; // 250 ms, independent of FPS.
    void Block() { released_ = holding_ = false; }
    void Release() { released_ = true; holding_ = false; }
    bool CanStart() const { return released_; }
    bool Holding() const { return holding_; }
    bool Begin(bool itemSelected, std::uint64_t now) {
        const bool starting = !holding_;
        if (starting) {
            selecting_ = !itemSelected;
            startedAt_ = now;
        }
        holding_ = true;
        return starting;
    }
    // Navigation and the initial A press can share a frame; use the actual
    // native clicked item before it toggles, rather than the old cursor item.
    void InitialClick(bool itemSelected) { selecting_ = !itemSelected; }
    bool WantsToggle(bool itemSelected, bool cursorMoved, std::uint64_t now) const {
        return holding_ && now - startedAt_ >= StartDelayTicks &&
            cursorMoved && itemSelected != selecting_;
    }
};

// Fixed hints describe the shortcuts; they are not per-item availability.
inline void AddShortcutHints(game::PocketFooter &footer, bool atHome) {
    if (atHome) footer.Append(game::PocketHint::Store);
    footer.Append(game::PocketHint::Drop);
    footer.Append(game::PocketHint::Use);
}

// Registers the handlers above and their supported native lifecycles. Settings,
// caption providers and profile/dependency failures are assembled in pockets.cpp.
qol::core::InstallResult InstallPockets();
}
