#include "preview_context.hpp"
namespace qol::crafting::preview_internal {
void *Find(void *root, const char *name) {
    return root ? profile::FindPane.Resolve(Main)(root, name, true) : nullptr;
}
void *PhoneRoot(void *page) {
    // Page +3f8 is an array of layout-instance wrappers, not ui2d Layouts.
    // Follow the accessor used by native 03c6627c: instance +58, virtual +28
    // writes the drawable layout to an output pointer. Wrapper +18 is a
    // SafeString virtual table; using it as a Pane crashed in FindPane.
    auto instances = At<void **>(page, field::LayoutInstances);
    auto instance = instances ? instances[0] : nullptr;
    auto accessor = instance ? At<void *>(instance, field::LayoutAccessor) : nullptr;
    auto methods = accessor ? At<void *>(accessor) : nullptr;
    auto resolve = methods ? At<void (*)(void *, void **)>(methods, field::GetNativeLayout) : nullptr;
    void *nativeLayout{};
    if (resolve) resolve(accessor, &nativeLayout);
    auto root = nativeLayout ? At<void *>(nativeLayout, field::LayoutRoot) : nullptr;
    // The resource patch requires this root name. Decline a missing/unbuilt
    // layout before native recursion can follow an invalid child list.
    if (!root || std::memcmp(static_cast<std::byte *>(root) + 0xb0, "RootPane\0", 9) ||
        !At<void *>(root, 0x20) || !At<void *>(root, 0x28)) return nullptr;
    return root;
}
void Visible(void *pane, bool visible) {
    if (pane) At<unsigned char>(pane, field::PaneFlags) =
        (At<unsigned char>(pane, field::PaneFlags) & ~1u) | unsigned(visible);
}
struct LayoutResource {
    alignas(16) std::array<std::byte, recipe_layout::MaximumBytes> bytes{};
    unsigned inputSize{};
    std::uint64_t hash{};
};
std::array<LayoutResource, 16> LayoutResources{};
std::atomic_flag ResourceLock = ATOMIC_FLAG_INIT;
const void *WithSurface(const void *resource) {
    if (!resource) return resource;
    const auto *input = static_cast<const std::byte *>(resource);
    if (std::memcmp(input, "FLYT\xff\xfe\x14\0", 8)) return resource;
    const auto size = recipe_layout::Read<unsigned>(input, 12);
    // Identify the layout before touching the cache. Name resides in lyt1;
    // this is a format check, not an assumption about the caller's UI page.
    if (size < 60 || size > recipe_layout::MaximumBytes - 1024 ||
        std::memcmp(input + 20, "lyt1", 4) ||
        recipe_layout::Read<unsigned>(input, 24) < 39 ||
        std::memcmp(input + 48, "MenuRecipe\0", 11)) return resource;
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned i = 0; i < size; ++i) hash = (hash ^ std::to_integer<unsigned char>(input[i])) * 1099511628211ull;
    // Loading can occur on UI worker threads. Do not wait inside an allocator
    // callback; if another load is patching, leave this instance unmodified.
    if (ResourceLock.test_and_set(std::memory_order_acquire)) return resource;
    const void *result = resource;
    LayoutResource *free = nullptr;
    for (auto &entry : LayoutResources) {
        if (entry.inputSize == size && entry.hash == hash) { result = entry.bytes.data(); break; }
        if (!entry.inputSize && !free) free = &entry;
    }
    if (result == resource && free && recipe_layout::AddSurface({input, size}, free->bytes)) {
        free->hash = hash; free->inputSize = size; result = free->bytes.data();
        Logging.Debug("[QoL Enhancements/Crafting] DIY preview: capture surface added to loaded MenuRecipe\n");
    }
    ResourceLock.clear(std::memory_order_release);
    return result;
}
bool BuildRecipeLayout::Callback(void *layout, void *result, void *device, void *accessor,
                         void *factory, void *parts, const void *resource, void *options, bool check){
        return Orig(layout, result, device, accessor, factory, parts, WithSurface(resource), options, check);
    }

}
