#pragma once
#include <cstdint>
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
} // namespace able_bag
