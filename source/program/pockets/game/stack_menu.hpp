#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace qol_pockets::game {
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

class StackMenu {
    ActionList &list_;
public:
    explicit StackMenu(ActionList &list) : list_(list) {}
    bool Valid() const { return list_.count >= 0 && list_.count <= 7; }
    bool Contains(Action action) const {
        return Valid() && std::find(list_.items, list_.items + list_.count, action) !=
            list_.items + list_.count;
    }
    unsigned Space() const { return Valid() ? 7 - list_.count : 0; }
    // Reject missing/ambiguous handles, duplicate new IDs and full/invalid lists.
    // Existing action handles survive insertion; references/indices do not.
    bool InsertAfter(Action existing, Action addition) {
        if (!Space() || Contains(addition)) return false;
        auto *end = list_.items + list_.count;
        auto *position = std::find(list_.items, end, existing);
        if (position == end || std::find(position + 1, end, existing) != end) return false;
        ++position;
        std::move_backward(position, end, end + 1);
        *position = addition;
        ++list_.count;
        return true;
    }
};
inline int QuantityForAction(int action) {
    return action == static_cast<int>(Action::GrabFive)  ? 5
           : action == static_cast<int>(Action::GrabTen) ? 10
                                                         : 0;
}
} // namespace qol_pockets::game
