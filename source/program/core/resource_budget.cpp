#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "resource_budget.hpp"
#include <array>
#include <cstring>

namespace qol::core {
namespace {
constexpr std::uintptr_t ResourceSizeQuery = 0x03e3cad0;
bool shoppingResources{}, fittingResources{};
template<class T> T Read(std::uintptr_t pointer, std::uintptr_t offset = 0) {
    return *reinterpret_cast<const T *>(pointer + offset);
}
// Native 03e3cad0 returns whether RSTB/RSTC supplied a size and writes a
// uint32 size plus one metadata byte. Preserve the original lookup and flags,
// then raise only an installed module's exact normalized path. A missing table entry gets the same
// safe floor; larger allocations from other mods are never reduced. Installing
// one shared hook avoids competing hooks or a conflicting global RSTC file.
struct NativeResourceSize {
    std::uint32_t bytes;
    std::uint8_t flags;
};
HOOK_DEFINE_TRAMPOLINE(CounterResourceBudget) {
    static bool Callback(std::uintptr_t manager, NativeResourceSize *size, std::uintptr_t path) {
        const bool found = Orig(manager, size, path);
        const auto text = path ? Read<const char *>(path, 8) : nullptr;
        if (!size || !text) return found;
        const auto minimum = shoppingResources && !std::strcmp(text, CounterResourcePath)
            ? CounterResourceMinimumBytes
            : fittingResources && !std::strcmp(text, FittingResourcePath)
            ? FittingResourceMinimumBytes : 0;
        if (!minimum) return found;
        if (!found) {
            size->bytes = minimum;
            size->flags = 0;
        } else if (size->bytes < minimum) {
            size->bytes = minimum;
        }
        return true;
    }
};
bool InstallResourceBudget() {
    HookPlan plan{Site<CounterResourceBudget>("CounterResourceBudget", ResourceSizeQuery,
        {0xa9bb7bfd, 0xa90167fa, 0xa9025ff8, 0xa90357f6, 0xa9044ff4})};
    if (!plan || !plan.Publish<CounterResourceBudget>(ResourceSizeQuery)) {
        Logging.Error("[QoL Enhancements/Resources] DISABLED: resource size instruction mismatch\n");
        return false;
    }
    return true;
}

} // namespace
bool PrepareShoppingResources() {
    if (!InstallResourceBudget()) return false;
    if (!shoppingResources) RecordMutation();
    shoppingResources = true;
    return true;
}
bool PrepareFittingResources() {
    if (!InstallResourceBudget()) return false;
    if (!fittingResources) RecordMutation();
    fittingResources = true;
    return true;
}
} // namespace qol::core
