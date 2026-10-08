#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace qol_pockets::features {
// Private convention, not a game-reserved range: ASCII "OU" plus local IDs.
// Keep the sign bit clear. Game-facing hooks must intercept these before any
// vanilla action metadata lookup; they are identifiers, never table indexes.
enum class Action : std::int32_t {
    GrabOne = 46,
    TurnipGrabTen = 47,
    End = 63,
    GrabFive = 0x4F550001,
    GrabTen = 0x4F550002
};
// Separate subrange for dispatch tokens; no vanilla callback table is extended.
enum class CallbackId : std::int32_t { GrabFive = 0x4F551001, GrabTen = 0x4F551002 };
struct ActionList {
    Action items[7];
    std::int32_t count;
};
static_assert(sizeof(ActionList) == 32 && offsetof(ActionList, count) == 28);

inline bool ContainsGrabOne(const ActionList &list) {
    return list.count >= 0 && list.count <= 7 &&
           std::find(list.items, list.items + list.count, Action::GrabOne) !=
               list.items + list.count;
}

// Policy only: no game addresses, item writes, or framework dependencies.
inline void AddStackActions(ActionList &list, int stackSize, bool hasFreePocket) {
    if (!hasFreePocket || stackSize <= 5 || list.count >= 7 || !ContainsGrabOne(list)) {
        return;
    }
    // Idempotent if another call sees an already-extended menu.
    if (std::find(list.items, list.items + list.count, Action::GrabFive) !=
        list.items + list.count) {
        return;
    }
    auto position = std::find(list.items, list.items + list.count, Action::GrabOne) + 1;
    const int additions = (stackSize > 10 && list.count <= 5) ? 2 : 1;
    std::move_backward(position, list.items + list.count, list.items + list.count + additions);
    position[0] = Action::GrabFive;
    if (additions == 2) {
        position[1] = Action::GrabTen;
    }
    list.count += additions;
}
inline int QuantityForAction(int action) {
    return action == static_cast<int>(Action::GrabFive)  ? 5
           : action == static_cast<int>(Action::GrabTen) ? 10
                                                         : 0;
}
} // namespace qol_pockets::features
