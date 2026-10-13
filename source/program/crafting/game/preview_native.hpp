#pragma once
#include "preview_profile.hpp"
#include "program/core/native/runtime.hpp"
namespace qol::crafting::game::preview_native {
template<class T> T &At(void *object, std::size_t offset = 0) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(object) + offset);
}
// Slots live in main's mapped data; installation checks slot ranges separately
// from instruction fingerprints. Pointees still require existing lifecycle guards.
inline void *Application(std::uintptr_t main) { return *reinterpret_cast<void **>(main + v303::preview::address::Application); }
inline void *EntityManager(std::uintptr_t main) { return *reinterpret_cast<void **>(main + v303::preview::address::EntityManager); }
inline void *ItemParamGlobalSlot(std::uintptr_t main) { return *reinterpret_cast<void **>(main + v303::preview::address::ItemParamGlobalSlot); }
inline void *RenderLayers(std::uintptr_t main) { return *reinterpret_cast<void **>(main + v303::preview::address::RenderLayers); }
inline void *UiContext(std::uintptr_t main) { return *reinterpret_cast<void **>(main + v303::preview::address::UiContext); }
inline bool GlobalsAvailable() {
    struct Slot { const char *name; std::uintptr_t offset; };
    constexpr Slot slots[]{
        {"preview.Application", v303::preview::address::Application},
        {"preview.EntityManager", v303::preview::address::EntityManager},
        {"preview.ItemParamGlobalSlot", v303::preview::address::ItemParamGlobalSlot},
        {"preview.RenderLayers", v303::preview::address::RenderLayers},
        {"preview.UiContext", v303::preview::address::UiContext},
    };
    for (const auto &slot : slots) {
        if (!native::MainDataContains(slot.offset, sizeof(void *))) {
            Logging.Error("[QoL Enhancements/Crafting] native slot=%s main+0x%lx outside mapped data\n",
                          slot.name, slot.offset);
            return false;
        }
    }
    return true;
}
} // namespace qol::crafting::game::preview_native
