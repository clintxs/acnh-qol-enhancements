#pragma once
#include "program/fitting/game/item.hpp"
#include <array>
#include <cstdint>
#include <cstddef>

namespace able_bag {
struct Entry { Item item{}; int price{}; };
enum class ToggleResult { Added, Removed, Full, Invalid };
class Bag {
public:
    static constexpr unsigned Limit = 256;
    std::array<Entry, Limit> entries{};
    unsigned count{};
    void Clear();
    int Find(const Item &item) const;
    ToggleResult Toggle(Item item, int price, bool fits);
    static bool Contains(const Item *items, unsigned size, const Item &item);
    unsigned ExtraCount(const Item *outfit, unsigned size) const;
    int ExtraPrice(const Item *outfit, unsigned size) const;
};
// Reserve enough room for either native delivery choice, without counting any
// pocket slot that payment might free. Overlapping exact items are charged once.
bool Fits(unsigned storage, unsigned pockets, unsigned oldClothes,
          unsigned purchases, unsigned bagExtras);
struct PressEdge {
    bool seen{}, down{};
    bool Update(bool held);
    void Reset();
};
} // namespace able_bag
