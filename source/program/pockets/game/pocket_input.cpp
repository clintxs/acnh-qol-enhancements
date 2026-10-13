#include "pocket_input.hpp"
#include "program/pockets/game/stack_bindings.hpp"

namespace qol_pockets::game::pocket_input {
namespace {
namespace address {
constexpr std::uintptr_t InputBindingVtable = 0x04a070d8;
constexpr std::uintptr_t PocketInputCallback = 0x03088724;
constexpr std::uintptr_t PocketControllerVtable = 0x04a06e28;
}
namespace layout {
constexpr std::size_t PocketPage = 0x210;
constexpr std::size_t BindingColumnCount = 0x468;
constexpr std::size_t InputBindings = 0x470;
constexpr std::size_t BindingRowCount = 0x46c;
constexpr std::size_t BindingFunction = 0x10;
constexpr std::size_t BindingAdjustment = 0x18;
constexpr std::size_t BindingOwner = 8;
constexpr std::size_t InputContext = 0x210;
constexpr std::size_t FocusState = 0x48;
constexpr std::size_t FocusControllerCount = 0x18;
constexpr std::size_t FocusControllers = 0x20;
constexpr std::size_t FocusedPane = 0x10;
constexpr std::size_t ItemPane = 0x218;
constexpr std::size_t ItemWidgets = 0x590;
constexpr std::size_t BindingBytes = 32;
}
constexpr int PocketSlotLimit = 40, InputRowLimit = 8;
template <typename T> T &Field(void *object, std::size_t offset) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(object) + offset);
}
}
void *FindPocketController(void *page) {
    const int count = Field<int>(page, layout::BindingColumnCount);
    auto *bindings = Field<std::byte *>(page, layout::InputBindings);
    if (!bindings || count < 1 || count > InputRowLimit ||
        Field<int>(page, layout::BindingRowCount) < 1) {
        return nullptr;
    }
    for (int i = 0; i < count; ++i) {
        auto *binding = bindings + layout::BindingBytes * i;
        if (Field<std::uintptr_t>(binding, 0) != game::MainAddress(address::InputBindingVtable) ||
            Field<std::uintptr_t>(binding, layout::BindingFunction) !=
                game::MainAddress(address::PocketInputCallback) ||
            Field<std::uintptr_t>(binding, layout::BindingAdjustment) != 0) {
            continue;
        }
        auto *owner = Field<void *>(binding, layout::BindingOwner);
        if (owner &&
            Field<std::uintptr_t>(owner, 0) == game::MainAddress(address::PocketControllerVtable) &&
            Field<void *>(owner, layout::PocketPage) == page) {
            return owner;
        }
    }
    return nullptr;
}

// Native focus management (031017e0 / 03c48b78) stores the current pane
// separately from +1c518, which is only a transient click/drag event payload.
int FindFocusedItemSlot(void *page, int row) {
    auto *context = Field<void *>(page, layout::InputContext);
    auto *focus = context ? Field<void *>(context, layout::FocusState) : nullptr;
    if (!focus || row < 0 || row >= Field<int>(focus, layout::FocusControllerCount)) {
        return -1;
    }
    auto **controllers = Field<void **>(focus, layout::FocusControllers);
    auto *controller = controllers ? controllers[row] : nullptr;
    auto *pane = controller ? Field<void *>(controller, layout::FocusedPane) : nullptr;
    if (!pane) {
        return -1;
    }
    for (int slot = 0; slot < PocketSlotLimit; ++slot) {
        auto *widget = Field<void *>(page, layout::ItemWidgets + sizeof(void *) * slot);
        if (widget && Field<void *>(widget, layout::ItemPane) == pane) {
            return slot;
        }
    }
    return -1; // Includes focus on money/clothing/other non-item controls.
}

}
