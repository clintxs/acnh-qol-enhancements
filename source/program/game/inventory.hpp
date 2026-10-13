#pragma once
#include "v303/inventory.hpp"
namespace qol::game::inventory {
using Item = v303::inventory::Item;
// Exact native value, including customization, count and durability bits.
// This identity owns bytes, never a borrowed ItemAt pointer or just an item ID.
struct ItemIdentity {
    Item value{};
    constexpr bool operator==(const ItemIdentity &) const = default;
};
static_assert(sizeof(ItemIdentity) == 8);
struct ItemValue {
    bool present{};
    Item value{};
    constexpr ItemIdentity Identity() const { return {value}; }
};
// The verified 3.0.3 accessors read the first resident byte as an input ID;
// they do not retain it. Owning that byte prevents a dangling local argument,
// but does not establish player/controller lifetime or native registry validity.
class ResidentView;
class PocketSlots {
    std::uintptr_t main_;
    unsigned char resident_;
    unsigned capacity_;
    friend class ResidentView;
    PocketSlots(std::uintptr_t main, unsigned char resident, unsigned capacity)
        : main_(main), resident_(resident), capacity_(capacity) {}
public:
    unsigned Capacity() const { return capacity_; }
    ItemValue TryItemAt(unsigned slot) const {
        if (slot >= capacity_ || slot >= 40) return {};
        const auto *item = v303::inventory::ItemAt.Resolve(main_)(&resident_, slot);
        ItemValue result{.present = item != nullptr};
        if (item) std::memcpy(&result.value, item, sizeof(result.value));
        return result;
    }
    bool IsEquipped(Item item, unsigned slot) const {
        return slot < capacity_ && slot < 40 &&
            v303::inventory::IsEquipped.Resolve(main_)(&resident_, &item, slot);
    }
};
// Resolve inside a supported callback. Slots snapshots capacity once for a
// synchronous traversal; do not retain this view across native lifecycle work.
class ResidentView {
    std::uintptr_t main_;
    unsigned char resident_;
public:
    ResidentView(std::uintptr_t main, unsigned char resident) : main_(main), resident_(resident) {}
    PocketSlots Slots() const {
        return {main_, resident_, v303::inventory::Capacity.Resolve(main_)(&resident_)};
    }
    Item Held() const { return v303::inventory::Held.Resolve(main_)(&resident_).value; }
};
inline ResidentView ForResident(std::uintptr_t main, unsigned char resident) { return {main, resident}; }
inline unsigned ToolKind(std::uintptr_t main, Item item) {
    if (static_cast<unsigned short>(item) >= 0xfffd) return 0;
    std::array<std::uintptr_t, 3> parameter{};
    v303::inventory::ItemParameter.Resolve(main)(parameter.data(), &item);
    return v303::inventory::ToolKind.Resolve(main)(parameter.data());
}
} // namespace qol::game::inventory
