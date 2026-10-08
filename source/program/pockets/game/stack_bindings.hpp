#pragma once
#include "lib.hpp"
#include "program/pockets/features/stack_menu.hpp"

namespace qol_pockets::game {
namespace stack_offsets {
inline constexpr std::uintptr_t BuildMenu = 0x0307e7e0;
inline constexpr std::uintptr_t MapAction = 0x0307d490;
inline constexpr std::uintptr_t DispatchCall = 0x0308c764;
inline constexpr std::uintptr_t CallbackTable = 0x04a06740;
inline constexpr std::uintptr_t FallbackCallback = 0x030742d0;
inline constexpr std::uintptr_t Split = 0x03076348;
inline constexpr std::uintptr_t SubtractOne = 0x03076480;
inline constexpr std::uintptr_t SetDestinationCount = 0x030764f0;
inline constexpr std::uintptr_t ResolveCaption = 0x019f59b0;
inline constexpr std::uintptr_t PostAction = 0x019f5b80;
inline constexpr std::uintptr_t GetStackCount = 0x019f02d0;
inline constexpr std::uintptr_t HasFreePocket = 0x02572fb0;
inline constexpr std::uintptr_t SafeStringVtableGot = 0x04ab2020;
} // namespace stack_offsets
inline std::uintptr_t MainAddress(std::uintptr_t offset) {
    return exl::util::modules::GetTargetStart() + offset;
}
template <typename Function> inline Function FunctionAt(std::uintptr_t offset) {
    return reinterpret_cast<Function>(MainAddress(offset));
}
struct MenuContext {
    void *selectedItem;
    void *inventory;
    // Only these first two verified fields are accessed; the object is larger.
};
inline int StackCount(void *item) {
    return FunctionAt<int (*)(void *)>(stack_offsets::GetStackCount)(item);
}
inline bool HasFreePocket(void *inventory) {
    return (FunctionAt<unsigned (*)(void *)>(stack_offsets::HasFreePocket)(inventory) & 1) != 0;
}
struct Caption {
    std::uintptr_t vtable;
    const char *key;
    // ACNH returns SafeString through hidden register x8, not registers x0/x1.
    // A non-trivial destructor requests that AAPCS64 indirect-return convention.
    ~Caption() {}
};
static_assert(sizeof(Caption) == 16);
} // namespace qol_pockets::game
