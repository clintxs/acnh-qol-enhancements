#pragma once
#include "stack_bindings.hpp"
#include "quick_store_profile.hpp"
#include "program/game/inventory.hpp"
namespace qol_pockets::game::quick_store_native {
template <typename T> T &Field(void *controllerObject, std::size_t offset) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(controllerObject) + offset);
}
using v303::quick_store::MenuDescription;
using v303::quick_store::SafeString;
inline int ItemFeedbackValidity(void *widget) {
    return v303::quick_store::FeedbackValidityBinding.Resolve(MainAddress(0))(widget);
}
inline void ButtonFeedback(void *widget, const SafeString *event, void *context) {
    v303::quick_store::FeedbackBinding.Resolve(MainAddress(0))(widget, event, context);
}
inline void BuildMenu(void *pocket, MenuDescription *description) {
    v303::quick_store::MenuBinding.Resolve(MainAddress(0))(pocket, description);
}
inline bool OwnHome(void *verifiedPocket) {
    const auto *resident = reinterpret_cast<const unsigned char *>(
        static_cast<std::byte *>(verifiedPocket) + v303::quick_store::layout::PocketInventory);
    return (v303::quick_store::HomeBinding.Resolve(MainAddress(0))(resident) & 1) != 0;
}
inline qol::game::inventory::ResidentView InventoryForPocket(void *verifiedPocket) {
    return qol::game::inventory::ForResident(MainAddress(0),
        *reinterpret_cast<const unsigned char *>(static_cast<std::byte *>(verifiedPocket) +
                                                v303::quick_store::layout::PocketInventory));
}
} // namespace qol_pockets::game::quick_store_native
