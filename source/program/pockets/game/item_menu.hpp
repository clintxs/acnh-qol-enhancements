#pragma once
#include <cstddef>
#include <optional>
#include <span>

namespace qol_pockets::game {
// Action meaning is contextual: adapters verify native IDs and message keys.
enum class ItemAction {
    Unknown, Drop, Release, Hold, Wear, PutAway, Eat, EatOne, Drink,
    Open, MoveToPockets, Claim, Store
};
struct MenuEntry {
    ItemAction action = ItemAction::Unknown;
    // A known name alone is insufficient. The context adapter must verify
    // the row supports its existing ordinary execution path.
    bool executable = false;
};
using MenuEntries = std::span<const MenuEntry>;
enum class FeedbackTiming { None, BeforeExecution, AfterSuccess };
enum class SelectionContext { Other, ItemSelection, HomeStorage };

// Synchronous borrow. Return an index into this exact snapshot; adapters reject
// out-of-range or unsupported entries and retain only copied native evidence.
// This chooses an entry; it does not execute or promise success. Each context's
// adapter defines its supported checks; this is not a general engine menu API.
using ChooseMenuEntry = std::optional<std::size_t> (*)(MenuEntries);
}
