#pragma once
#include "v303/inventory.hpp"
namespace qol::game::inventory {
using Item = v303::inventory::Item;
struct ItemValue { bool present{}; Item value{}; };
// Borrowed resident identity, valid only during the caller's supported game/UI
// callback. Callers supply the resident byte from a verified controller or a
// local copy. No view or native item pointer may survive that callback.
class ResidentView {
    std::uintptr_t main_;
    const unsigned char *resident_;
public:
    ResidentView(std::uintptr_t main, const unsigned char *resident) : main_(main), resident_(resident) {}
    unsigned Capacity() const { return v303::inventory::Capacity.Resolve(main_)(resident_); }
    ItemValue ItemAt(unsigned slot) const {
        // Existing callers validate capacity and slot before this access. Native
        // accessors own their read locks; copy all eight bytes immediately.
        const auto *item = v303::inventory::ItemAt.Resolve(main_)(resident_, slot);
        ItemValue result{.present = item != nullptr};
        if (item) std::memcpy(&result.value, item, sizeof(result.value));
        return result;
    }
    bool IsEquipped(Item item, unsigned slot) const {
        return v303::inventory::IsEquipped.Resolve(main_)(resident_, &item, slot);
    }
    Item Held() const { return v303::inventory::Held.Resolve(main_)(resident_).value; }
};
inline ResidentView ForResident(std::uintptr_t main, const unsigned char &resident) {
    return {main, &resident};
}
ResidentView ForResident(std::uintptr_t, const unsigned char &&) = delete;
inline unsigned ToolKind(std::uintptr_t main, Item item) {
    if (static_cast<unsigned short>(item) >= 0xfffd) return 0;
    std::array<std::uintptr_t, 3> parameter{};
    v303::inventory::ItemParameter.Resolve(main)(parameter.data(), &item);
    return v303::inventory::ToolKind.Resolve(main)(parameter.data());
}
} // namespace qol::game::inventory
