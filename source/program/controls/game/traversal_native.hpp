#pragma once
#include "traversal_profile.hpp"
#include "program/game/inventory.hpp"
// Traversal's installation guards validate the selected profile before these
// bindings are called. Resident/item access lives in the shared inventory adapter.

namespace qol::controls::game::traversal_native {
template<class T> T Read(const void *object, std::size_t offset = 0) {
    T value;
    std::memcpy(&value, static_cast<const std::byte *>(object) + offset, sizeof(value));
    return value;
}
}
