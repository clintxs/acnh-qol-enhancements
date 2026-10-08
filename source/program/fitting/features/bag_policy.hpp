#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace able_bag {
struct Item {
    std::uint16_t id{};
    std::uint8_t flags[2]{};
    std::uint32_t extra{};
    bool operator==(const Item &other) const {
        return id == other.id && flags[0] == other.flags[0] &&
               flags[1] == other.flags[1] && extra == other.extra;
    }
};
static_assert(sizeof(Item) == 8);
struct Entry { Item item{}; int price{}; };
enum class ToggleResult { Added, Removed, Full, Invalid };
class Bag {
public:
    static constexpr unsigned Limit = 256;
    std::array<Entry, Limit> entries{};
    unsigned count{};
    void Clear() { count = 0; }
    int Find(const Item &item) const {
        for (unsigned i = 0; i < count; ++i) if (entries[i].item == item) return i;
        return -1;
    }
    ToggleResult Toggle(Item item, int price, bool fits) {
        const int found = Find(item);
        if (found >= 0) {
            for (unsigned i = found + 1; i < count; ++i) entries[i - 1] = entries[i];
            --count;
            return ToggleResult::Removed;
        }
        if (item.id >= 0xfffe || price < 0 || price > 1000000) return ToggleResult::Invalid;
        if (!fits || count == Limit) return ToggleResult::Full;
        entries[count++] = {item, price};
        return ToggleResult::Added;
    }
    static bool Contains(const Item *items, unsigned size, const Item &item) {
        for (unsigned i = 0; i < size; ++i) if (items[i] == item) return true;
        return false;
    }
    unsigned ExtraCount(const Item *outfit, unsigned size) const {
        unsigned result = 0;
        for (unsigned i = 0; i < count; ++i)
            result += !Contains(outfit, size, entries[i].item);
        return result;
    }
    int ExtraPrice(const Item *outfit, unsigned size) const {
        int result = 0;
        for (unsigned i = 0; i < count; ++i)
            if (!Contains(outfit, size, entries[i].item)) result += entries[i].price;
        return result;
    }
};
// Reserve enough room for either checkout choice (wear the outfit or send it).
// The same selected purchase in both lists is charged/delivered only once.
inline bool Fits(unsigned storage, unsigned pockets, unsigned oldClothes,
                 unsigned purchases, unsigned bagExtras) {
    const unsigned base = oldClothes > purchases ? oldClothes : purchases;
    return std::uint64_t(base) + bagExtras <= std::uint64_t(storage) + pockets;
}
struct PressEdge {
    bool seen{}, down{};
    bool Update(bool held) {
        const bool fresh = seen && held && !down;
        seen = true; down = held;
        return fresh;
    }
    void Reset() { seen = down = false; }
};
} // namespace able_bag
