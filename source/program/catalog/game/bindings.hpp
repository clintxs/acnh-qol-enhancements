#pragma once
#include "lib.hpp"
namespace catalog_aware_shops::game {
// Resolve a verified 3.0.3 offset against the current ASLR load address. These
// helpers do not validate arbitrary addresses: callers must pass build guards.
inline std::uintptr_t MainAddress(std::uintptr_t offset) {
    return exl::util::modules::GetTargetStart() + offset;
}
template <typename Function> inline Function FunctionAt(std::uintptr_t offset) {
    return reinterpret_cast<Function>(MainAddress(offset));
}
} // namespace catalog_aware_shops::game
