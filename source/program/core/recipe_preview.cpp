#include "lib.hpp"
#include "recipe_preview.hpp"
#include "recipe_preview_layout.hpp"
#include "page_input.hpp"
#include <array>
#include <atomic>
#include <cstring>

namespace qol::core {
namespace {
namespace address {
constexpr std::uintptr_t BuildLayout = 0x00267d64, FindPane = 0x0027d1d0;
constexpr std::uintptr_t PhoneBind = 0x02e9cde0, PhoneFrame = 0x02e9fbb0;
constexpr std::uintptr_t PhoneDeactivate = 0x02e9d538, PhonePress = 0x02e99d94;
constexpr std::uintptr_t PageInput = 0x03c58d24, UiContext = 0x04ab2230;
constexpr std::uintptr_t CreateItemModel = 0x032bd990, ReleaseModel = 0x032ba5e4;
constexpr std::uintptr_t ChangeState = 0x03dbb160;
constexpr std::uintptr_t CapturePicture = 0x03a17390, AlternateCapturePicture = 0x03a174f0;
}
namespace field {
constexpr std::size_t Resident = 0x1f8, PhoneLayout = 0x210, Selection = 0x218;
constexpr std::size_t Recipes = 0x8a350, State = 0x1b0, Requests = 0x1f0;
constexpr std::size_t LayoutInstances = 0x3f8, LayoutAccessor = 0x58;
constexpr std::size_t GetNativeLayout = 0x28, LayoutRoot = 0x18, Grid = 0x580;
constexpr std::size_t Rows = 0x210, RowCount = 0x208, PaneFlags = 0x58;
constexpr std::size_t ShortcutHint = 0x570, HintInput = 0xa0;
constexpr std::size_t InputRow = 0x10, InputKind = 0x14;
constexpr std::size_t Controller = 0x30, Parameters = 0x28;
constexpr std::size_t ModelMode = 0x50, ModelHandle = 0x6c;
}
constexpr unsigned WorkbenchModelMode = 2, BackMask = 0x20, PhoneShortcutKind = 5;
std::uintptr_t Main{};
template<class T> T &At(void *object, std::size_t offset = 0) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(object) + offset);
}
template<class F> F Native(std::uintptr_t offset) { return reinterpret_cast<F>(Main + offset); }
void *UiContext() {
    auto storage = *Native<void **>(address::UiContext);
    return storage ? At<void *>(storage) : nullptr;
}
void *Find(void *root, const char *name) {
    return root ? Native<void *(*)(void *, const char *, bool)>(address::FindPane)(root, name, true) : nullptr;
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
// Layout::Build retains its resource pointer. Never give it a temporary vector
// or overwrite a buffer that an existing layout can still reference. Capacity
// exhaustion declines the feature, leaving the original resource untouched.
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
        Logging.Log("[QoL Enhancements/Core] DIY preview: capture surface added to loaded MenuRecipe\n");
    }
    ResourceLock.clear(std::memory_order_release);
    return result;
}
HOOK_DEFINE_TRAMPOLINE(BuildRecipeLayout) {
    static bool Callback(void *layout, void *result, void *device, void *accessor,
                         void *factory, void *parts, const void *resource, void *options, bool check) {
        return Orig(layout, result, device, accessor, factory, parts, WithSurface(resource), options, check);
    }
};

enum class Phase { Idle, Queued, Starting, Showing, Closing, DrainBack };
struct Session {
    void *phone{}, *layout{}, *surface{}, *gridPane{}, *scrollPane{}, *cursorPane{};
    void *controller{}, *parameters{};
    Phase phase{};
    unsigned frames{};
    unsigned rejectedPresses{};
    unsigned char gridFlags{}, scrollFlags{}, cursorFlags{}, savedHidden{};
    std::array<std::byte, 0x28> savedParameters{};
    std::uint64_t item{};
    int inputRow{-1};
    bool backHeld{}, ownsRenderer{};
};
std::array<Session, 8> Sessions{};
Session *ForPhone(void *phone) {
    if (!phone) return nullptr;
    const auto resident = At<unsigned char>(phone, field::Resident);
    if (resident >= Sessions.size()) return nullptr;
    auto &s = Sessions[resident]; return s.phone == phone ? &s : nullptr;
}
bool PhoneReady(void *phone) {
    return At<int>(phone, field::State) == 3 && At<int>(phone, field::Requests) == -1 &&
           At<int>(phone, field::Requests + 4) == -1;
}
int ShortcutInputRow(void *layout) {
    auto hint = At<void *>(layout, field::ShortcutHint);
    auto input = hint ? At<void *>(hint, field::HintInput) : nullptr;
    // MenuRecipePage creates this shortcut with kind 5 in native 02e936ec.
    // It is not the Back hint used by the pocket page (kind 3). The row is
    // shared by all buttons; B itself is read from the snapshot's BackMask.
    return input && At<unsigned>(input, field::InputKind) == PhoneShortcutKind &&
           At<unsigned>(input, field::InputRow) < 8 ? At<int>(input, field::InputRow) : -1;
}
void Close(Session &s) {
    if (s.ownsRenderer) {
        // Release only the handle/mode acquired by this phone. The native
        // state-1 exit balances camera/layers before state 0 resets ownership.
        auto context = UiContext();
        if (context && At<void *>(context, field::Controller) == s.controller &&
            At<unsigned>(s.controller, field::ModelMode) == WorkbenchModelMode)
            Native<void (*)(void *, unsigned)>(address::ReleaseModel)(s.controller, WorkbenchModelMode);
        if (context && At<void *>(context, field::Parameters) == s.parameters) {
            std::memcpy(static_cast<std::byte *>(s.parameters) + 0x2c, s.savedParameters.data(), s.savedParameters.size());
            At<unsigned char>(s.parameters, 0x12) = s.savedHidden;
            At<unsigned>(s.parameters, 0x2140) = 1;
        }
    }
    if (s.ownsRenderer && s.gridPane) At<unsigned char>(s.gridPane, field::PaneFlags) = s.gridFlags;
    if (s.ownsRenderer && s.scrollPane) At<unsigned char>(s.scrollPane, field::PaneFlags) = s.scrollFlags;
    if (s.ownsRenderer && s.cursorPane) At<unsigned char>(s.cursorPane, field::PaneFlags) = s.cursorFlags;
    Visible(s.surface, false);
    s.controller = s.parameters = nullptr; s.ownsRenderer = false;
    s.phase = Phase::DrainBack;
    Logging.Log("[QoL Enhancements/Core] DIY preview: grid restored; renderer released\n");
}
void PressRecipe(void *phone, void *event) {
    auto s = ForPhone(phone);
    if (!s || s->phase != Phase::Idle || !PhoneReady(phone) || !event) return;
    // Refresh on press as shortcut routing can be assigned after binding.
    s->inputRow = ShortcutInputRow(s->layout);
    // Button row 5 is press; row 1 is focus-leave. Confirm only an actual
    // recipe row, never category tabs, sort controls or an unrelated button.
    auto grid = At<void *>(s->layout, field::Grid);
    auto source = At<void *>(event, 8);
    if (!grid || !source) return;
    const auto rowCount = At<unsigned>(grid, field::RowCount);
    const auto rows = At<void **>(grid, field::Rows);
    if (!rows || !rowCount || rowCount > 512) return;
    bool recipeRow = false;
    for (unsigned i = 0; i < rowCount; ++i) if (rows[i] == source) { recipeRow = true; break; }
    const auto count = At<unsigned>(phone, field::Recipes);
    const auto selected = At<unsigned>(phone, field::Selection);
    const auto recipes = At<void **>(phone, field::Recipes + 8);
    if (!recipeRow || selected >= count || count > 4096 || !recipes || !recipes[selected] ||
        At<std::uint16_t>(recipes[selected], 8) >= 0xfffe || !s->surface || !s->gridPane || s->inputRow < 0) {
        if (s->rejectedPresses++ < 8)
            Logging.Log("[QoL Enhancements/Core] DIY preview: A declined; recipe_row=%u selected=%u count=%u surface=%p grid=%p input_row=%d\n",
                        unsigned(recipeRow), selected, count, s->surface, s->gridPane, s->inputRow);
        return;
    }
    s->item = At<std::uint64_t>(recipes[selected], 8);
    s->phase = Phase::Queued;
    Logging.Log("[QoL Enhancements/Core] DIY preview: A queued; recipe_index=%u item=%u\n", selected, unsigned(s->item & 0xffff));
}
unsigned ReplacePress(void *layout) {
    const auto columns = At<unsigned>(layout, 0x430), rows = At<unsigned>(layout, 0x434);
    const auto entries = At<std::byte *>(layout, 0x438);
    if (!entries || columns > 64 || rows != 0x23) return 0;
    unsigned replaced{};
    for (unsigned column = 0; column < columns; ++column) {
        auto &callback = At<std::uintptr_t>(entries + (5 * columns + column) * 0x20, 0x10);
        if (callback == Main + address::PhonePress) {
            callback = reinterpret_cast<std::uintptr_t>(&PressRecipe); ++replaced;
        } else if (callback == reinterpret_cast<std::uintptr_t>(&PressRecipe)) ++replaced;
    }
    return replaced;
}
HOOK_DEFINE_TRAMPOLINE(BindPhoneRecipes) {
    static void Callback(void *phone) {
        Orig(phone);
        const auto resident = At<unsigned char>(phone, field::Resident);
        if (resident >= Sessions.size()) return;
        auto &s = Sessions[resident]; s = {}; s.phone = phone;
        s.layout = At<void *>(phone, field::PhoneLayout);
        if (!s.layout) return;
        const auto root = PhoneRoot(s.layout);
        s.surface = Find(root, recipe_layout::SurfaceName);
        auto capture = Find(root, recipe_layout::CaptureName);
        auto picture = Find(root, recipe_layout::PictureName);
        bool captureBound = false;
        if (capture && picture) {
            if (auto typed = Native<void *(*)(void *)>(address::CapturePicture)(picture))
                captureBound = At<void *>(typed, 0x100) == capture;
            else if (auto typed = Native<void *(*)(void *)>(address::AlternateCapturePicture)(picture))
                captureBound = At<void *>(typed, 0x130) == capture;
        }
        if (!captureBound) s.surface = nullptr;
        s.gridPane = Find(root, "L_BtnList_00");
        s.scrollPane = Find(root, "L_ScrollBar_00");
        s.cursorPane = Find(root, "L_Cursor_00");
        if (s.gridPane) s.gridFlags = At<unsigned char>(s.gridPane, field::PaneFlags);
        if (s.scrollPane) s.scrollFlags = At<unsigned char>(s.scrollPane, field::PaneFlags);
        if (s.cursorPane) s.cursorFlags = At<unsigned char>(s.cursorPane, field::PaneFlags);
        s.inputRow = ShortcutInputRow(s.layout);
        const auto pressBindings = ReplacePress(s.layout);
        Logging.Log("[QoL Enhancements/Core] DIY preview: app bound; surface=%p grid=%p input_row=%d press_bindings=%u\n",
                    s.surface, s.gridPane, s.inputRow, pressBindings);
    }
};
bool PreviewInput(void *layout, void *snapshot, unsigned blocked) {
    for (auto &s : Sessions) if (s.layout == layout && s.phase != Phase::Idle) {
        if (!snapshot || s.inputRow < 0) return true;
        const auto count = At<unsigned>(snapshot, 8);
        auto rows = At<std::byte *>(snapshot, 0x10);
        if (!rows || count > 8 || unsigned(s.inputRow) >= count) return true;
        auto row = rows + s.inputRow * 0x48;
        const bool held = (At<unsigned>(row) & BackMask) != 0;
        const bool press = held && !s.backHeld; s.backHeld = held;
        if (s.phase == Phase::DrainBack) {
            if (!held) s.phase = Phase::Idle;
        } else if (press && !(blocked & 1) && !(At<unsigned>(row, 0x3c) & BackMask)) {
            s.phase = Phase::Closing;
            At<unsigned>(row, 0x3c) |= BackMask;
        }
        // Keep all native grid/category/sort input frozen. The input
        // context remains enabled, so B can still be observed here.
        return true;
    }
    return false;
}
bool Begin(Session &s) {
    if (!PhoneReady(s.phone)) return false;
    if (s.gridPane) s.gridFlags = At<unsigned char>(s.gridPane, field::PaneFlags);
    if (s.scrollPane) s.scrollFlags = At<unsigned char>(s.scrollPane, field::PaneFlags);
    if (s.cursorPane) s.cursorFlags = At<unsigned char>(s.cursorPane, field::PaneFlags);
    auto context = UiContext();
    auto controller = context ? At<void *>(context, field::Controller) : nullptr;
    auto parameters = context ? At<void *>(context, field::Parameters) : nullptr;
    if (!controller || !parameters || At<int>(controller, field::ModelMode) != -1 ||
        At<unsigned>(controller, field::ModelHandle) || At<int>(controller, 0x10) != 0) return false;
    s.controller = controller; s.parameters = parameters;
    s.savedHidden = At<unsigned char>(parameters, 0x12);
    std::memcpy(s.savedParameters.data(), static_cast<std::byte *>(parameters) + 0x2c, s.savedParameters.size());
    At<unsigned char>(parameters, 0x12) = 0;
    At<std::uint64_t>(parameters, 0x2c) = s.item;
    At<std::uint16_t>(parameters, 0x40) = 0x100;
    At<std::uint16_t>(parameters, 0x42) = 0xfffe;
    At<unsigned char>(parameters, 0x44) = 0;
    At<unsigned>(parameters, 0x48) = 1;
    At<unsigned>(parameters, 0x2140) = 1;
    At<unsigned>(controller, field::ModelMode) = WorkbenchModelMode;
    At<void *>(controller, 0x58) = nullptr; // No background page owner.
    At<unsigned char>(controller, 0x68) = 0;
    s.ownsRenderer = true; s.frames = 0;
    Native<void (*)(void *, unsigned)>(address::ChangeState)(static_cast<std::byte *>(controller) + 8, 1);
    s.phase = Phase::Starting;
    return true;
}
HOOK_DEFINE_TRAMPOLINE(UpdatePhoneRecipes) {
    static void Callback(void *phone) {
        Orig(phone);
        auto s = ForPhone(phone); if (!s) return;
        if (s->phase == Phase::Closing) { Close(*s); return; }
        if (s->phase == Phase::Queued) {
            if (!Begin(*s)) { s->phase = Phase::Idle; Logging.Log("[QoL Enhancements/Core] DIY preview: renderer busy/unavailable; grid retained\n"); }
            return;
        }
        if (s->phase == Phase::Showing && ++s->frames == 30) {
            Logging.Log("[QoL Enhancements/Core] DIY preview: renderer status; handle=%u flags=%u state=%d\n",
                        At<unsigned>(s->controller, field::ModelHandle),
                        At<unsigned char>(s->controller, 0x74), At<int>(s->controller, 0x10));
        }
        if (s->phase != Phase::Starting) return;
        if (++s->frames > 120) { Close(*s); return; }
        if (At<int>(s->controller, 0x10) != 1) return;
        struct ItemRequest {
            unsigned display{};
            std::uint32_t itemLow{}, itemHigh{};
            std::uint16_t recipe{0xfffe}, padding{};
            void *visibilityPage{};
        } request;
        static_assert(sizeof(ItemRequest) == 24 && offsetof(ItemRequest, visibilityPage) == 16);
        request.itemLow = s->item; request.itemHigh = s->item >> 32;
        Native<void (*)(void *, const ItemRequest *)>(address::CreateItemModel)(s->controller, &request);
        if (!At<unsigned>(s->controller, field::ModelHandle)) { Close(*s); return; }
        Visible(s->gridPane, false); Visible(s->scrollPane, false); Visible(s->cursorPane, false);
        Visible(s->surface, true); s->phase = Phase::Showing;
        Logging.Log("[QoL Enhancements/Core] DIY preview: stationary model created; handle=%u\n", At<unsigned>(s->controller, field::ModelHandle));
    }
};
HOOK_DEFINE_TRAMPOLINE(DeactivatePhoneRecipes) {
    static void Callback(void *phone) {
        if (auto s = ForPhone(phone)) {
            if (s->phase != Phase::Idle) Close(*s);
            s->phase = Phase::Idle; s->backHeld = false;
        }
        Orig(phone);
    }
};
} // namespace
bool InstallRecipePreview() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
        {0x00267d64, {0xa9ba7bfd, 0xa9016ffc, 0x910003fd, 0xa90267fa, 0xa9035ff8}},
        {0x02e9cde0, {0xd10243ff, 0xa9037bfd, 0xa9046ffc, 0xa90567fa, 0xa9065ff8}},
        {0x02e9fbb0, {0xa9bb7bfd, 0xa90167fa, 0xa9025ff8, 0xa90357f6, 0xa9044ff4}},
        {0x02e9d538, {0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd, 0xf9410814}},
        {0x03c58d24, {0xf9410800, 0xb4000060, 0x12000042, 0x17ffcad4, 0xd65f03c0}},
        {0x032bd990, {0x6db923e9, 0xa9017bfd, 0x910043fd, 0xf90013fc, 0xa90367fa}},
        {0x032ba5e4, {0xd100c3ff, 0xa9017bfd, 0xf90013f3, 0x910043fd, 0xb9405008}},
        {0x03dbb160, {0xa9be7bfd, 0xa9014ff4, 0x910003fd, 0xb9400808, 0x2a0103f3}},
        {0x03a17390, {0xa9bd7bfd, 0xa90157f6, 0xa9024ff4, 0x910003fd, 0xd00085c8}},
        {0x03a174f0, {0xa9bd7bfd, 0xa90157f6, 0xa9024ff4, 0x910003fd, 0xd00085c8}},
        {0x0027d1d0, {0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd, 0xaa0003f4}},
        {0x03c66298, {0xf9402c00, 0xaa0203f5, 0xaa0103f4, 0xf9000fbf, 0xb40000a0}},
        {0x02e93798, {0x528000a8, 0x9a9f1102, 0x94231d66, 0x395ed2a8, 0xf902ba60}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &f : fingerprints) if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + f.offset), f.words, sizeof(f.words))) {
        Logging.Log("[QoL Enhancements/Core] DIY preview: DISABLED; instruction mismatch at main+0x%lx\n", f.offset); return false;
    }
    Main = main.m_Text.m_Start;
    if (!RegisterPageInput(PreviewInput)) return false;
    BuildRecipeLayout::InstallAtOffset(address::BuildLayout);
    BindPhoneRecipes::InstallAtOffset(address::PhoneBind);
    UpdatePhoneRecipes::InstallAtOffset(address::PhoneFrame);
    DeactivatePhoneRecipes::InstallAtOffset(address::PhoneDeactivate);
    Logging.Log("[QoL Enhancements/Core] DIY preview: READY; renderer-only stationary prototype; A opens, B returns\n");
    return true;
}
} // namespace qol::core
