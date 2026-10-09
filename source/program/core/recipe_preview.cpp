#include "lib.hpp"
#include "recipe_preview.hpp"
#include "recipe_preview_layout.hpp"
#include "page_input.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

namespace qol::core {
namespace {
namespace address {
constexpr std::uintptr_t BuildLayout = 0x00267d64, FindPane = 0x0027d1d0;
constexpr std::uintptr_t PhoneBind = 0x02e9cde0, PhoneFrame = 0x02e9fbb0;
constexpr std::uintptr_t PhoneDeactivate = 0x02e9d538;
constexpr std::uintptr_t PhoneRendererModeSlot = 0x049e8378, PhoneRendererMode = 0x02ea0754;
constexpr std::uintptr_t RecipeFocus = 0x02e96a80, RecipeLeave = 0x02e99d90;
constexpr std::uintptr_t PageInput = 0x03c58d24, UiContext = 0x04ab2230;
constexpr std::uintptr_t Application = 0x059f83f0;
constexpr std::uintptr_t CreateItemModel = 0x032bd990, ReleaseModel = 0x032ba5e4;
constexpr std::uintptr_t ChangeState = 0x03dbb160;
constexpr std::uintptr_t DrawModelLayer = 0x039392b0, RenderLayers = 0x04abdb78;
constexpr std::uintptr_t SetModelRotation = 0x032b9e08;
constexpr std::uintptr_t BuildModelMatrix = 0x006021a4, DrawLayerContents = 0x039f63f0;
constexpr std::uintptr_t UpdateModelTransform = 0x032b9fcc, EntityManager = 0x04ab27c0;
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
constexpr std::size_t PageInputContext = 0x210, InputDisabled = 0x54, InputBlockMask = 0x58;
constexpr std::size_t ApplicationMasks = 0x418, GlobalBlockMask = 0x10;
constexpr std::size_t Controller = 0x30, Parameters = 0x28;
constexpr std::size_t ModelMode = 0x50, ModelHandle = 0x6c;
constexpr std::size_t ModelState = 0x10, BackgroundPage = 0x58, VisibilityPage = 0x60;
constexpr std::size_t ModelReady = 0x69, ModelFlags = 0x74, ModelAngles = 0x80;
constexpr std::size_t RightStick = 0x0c, ConsumedAxes = 0x40;
constexpr std::size_t LayerCount = 0x20, Layers = 0x28, LayerClearColor = 0x9c;
constexpr std::size_t DrawLayer = 0x3d0, RendererState = 0x248, DrawInfo = 0x18, DrawView = 8;
constexpr std::size_t ComponentOwner = 0x48, ComponentAngles = 0x2bc, ComponentPoseSize = 0x2c8;
constexpr std::size_t EntityHandle = 0x230, ParentHandle = 0x234;
constexpr std::size_t EntityRegistry = 0x1c0, RegistryEntries = 8, EntityFlags = 0x248;
constexpr std::size_t EntityKind = 0x15c, ItemChildren = 0x274, ItemChildCount = 0x394;
constexpr std::size_t EntityPhase = 0x288, EntityStage = 0x5e8;
}
constexpr unsigned WorkbenchModelMode = 2, ConfirmMask = 0x10, BackMask = 0x20, PhoneShortcutKind = 5;
constexpr unsigned RightStickMask = 2;
constexpr unsigned TransformPending = 1, MaterialPending = 6;
constexpr float StickDeadZone = 0.15f, DegreesPerUpdate = 3.0f, MaximumTilt = 60.0f;
// Native 032B74F0 converts radians to Angle32 (one turn = 2^32).
constexpr float AngleUnitsPerDegree = 4294967296.0f / 360.0f;
constexpr int PhoneModelMode = 26, PhoneModelState = 3;
constexpr unsigned ItemRenderLayer = 2;
constexpr unsigned PreviewActorKind = 0x42, ItemDrawableKind = 0x6f;
// Scene clear colors pass through the 3D color pipeline before UI capture.
// An sRGB-normalized E5DFC7 clear measured CCC9C0 in the 0.3.42 screenshot.
// These HDR values fit an inverse filmic curve to that observation. This is
// an approximate display-color correction, pending live visual validation.
constexpr std::array<float, 4> PreviewBackground{1.5950871f, 1.3879482f, 0.8751216f, 1.0f};
// Layer drawing runs on a render worker. Publish only the layer identity;
// that worker must not inspect the UI thread's mutable Session objects.
std::atomic<const void *> PreviewClearColor{};
std::atomic<void *> PreviewController{};
std::atomic<unsigned> PreviewHandle{}, PreviewAngleZ{}, MatrixCalls{}, MatrixMatches{}, BackgroundApplied{};
std::atomic<std::uint64_t> PreviewAngleXY{};
// Type 42 creates up to four type-6 item carriers; each carrier creates the
// type-6F drawable. Publish full generation-bearing handles on the native
// update thread instead of traversing the entity registry on a render worker.
std::array<std::atomic<unsigned>, 4> PreviewCarriers{};
// A command buffer is serially recorded. Scope the downstream color override
// to this layer's synchronous call, using atomic slots for parallel workers.
struct LayerDrawScope {
    std::atomic<void *> command{}, renderer{}, info{};
    std::atomic<unsigned> view{};
};
std::array<LayerDrawScope, 8> LayerDrawScopes{};
std::atomic<unsigned> TransformCalls{}, TransformAttempts{}, TransformApplied{}, ClearCalls{}, ClearMatches{};

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
const void *ItemLayerClearColor() {
    auto storage = *Native<void **>(address::RenderLayers);
    auto manager = storage ? At<void *>(storage) : nullptr;
    if (!manager) return nullptr;
    const auto count = At<unsigned>(manager, field::LayerCount);
    auto layers = At<void **>(manager, field::Layers);
    if (count <= ItemRenderLayer || count > 64 || !layers || !layers[ItemRenderLayer]) return nullptr;
    return static_cast<std::byte *>(layers[ItemRenderLayer]) + field::LayerClearColor;
}
HOOK_DEFINE_TRAMPOLINE(DrawPreviewLayer) {
    static void Callback(void *renderer, void *command, void *context, const void *options) {
        const auto published = PreviewClearColor.load(std::memory_order_acquire);
        const auto layer = context ? At<void *>(context, field::DrawLayer) : nullptr;
        const bool matches = published && layer &&
            static_cast<std::byte *>(layer) + field::LayerClearColor == published;
        if (published) ClearCalls.fetch_add(1, std::memory_order_relaxed);
        LayerDrawScope *scope = nullptr;
        if (matches && command) for (auto &entry : LayerDrawScopes) {
            void *empty = nullptr;
            if (!entry.command.compare_exchange_strong(empty, reinterpret_cast<void *>(1), std::memory_order_acquire)) continue;
            entry.renderer.store(At<void *>(renderer, field::RendererState), std::memory_order_relaxed);
            auto info = At<void *>(context, field::DrawInfo);
            entry.info.store(info ? static_cast<std::byte *>(info) + 0x60 : nullptr, std::memory_order_relaxed);
            entry.view.store(At<unsigned char>(context, field::DrawView), std::memory_order_relaxed);
            entry.command.store(command, std::memory_order_release);
            scope = &entry; break;
        }
        if (matches) ClearMatches.fetch_add(1, std::memory_order_relaxed);
        Orig(renderer, command, context, options); // Preserve native draw/clear flags.
        if (scope) scope->command.store(nullptr, std::memory_order_release);
    }
};
HOOK_DEFINE_TRAMPOLINE(DrawPreviewContents) {
    static void Callback(void *renderer, void *command, unsigned view, unsigned flags,
                         const void *color, void *info) {
        for (auto &entry : LayerDrawScopes) {
            if (entry.command.load(std::memory_order_acquire) != command || !command ||
                entry.renderer.load(std::memory_order_relaxed) != renderer ||
                entry.info.load(std::memory_order_relaxed) != info ||
                entry.view.load(std::memory_order_relaxed) != view) continue;
            if (BackgroundApplied.fetch_add(1, std::memory_order_relaxed) == 0)
                Logging.Log("[QoL Enhancements/Core] DIY preview: HDR background applied for #E5DFC7 target\n");
            color = PreviewBackground.data(); break;
        }
        Orig(renderer, command, view, flags, color, info);
    }
};
void PublishPose(void *controller) {
    const auto angles = static_cast<std::byte *>(controller) + field::ModelAngles;
    PreviewAngleZ.store(At<unsigned>(angles, 8), std::memory_order_relaxed);
    PreviewAngleXY.store(At<std::uint64_t>(angles), std::memory_order_release);
    PreviewHandle.store(At<unsigned>(controller, field::ModelHandle), std::memory_order_release);
}
void PublishCarriers(void *controller) {
    const auto handle = PreviewHandle.load(std::memory_order_acquire);
    if (!handle || At<unsigned>(controller, field::ModelHandle) != handle) {
        for (auto &carrier : PreviewCarriers) carrier.store(0, std::memory_order_release);
        return;
    }
    auto storage = *Native<void **>(address::EntityManager);
    auto manager = storage ? At<void *>(storage) : nullptr;
    auto registry = manager ? At<void *>(manager, field::EntityRegistry) : nullptr;
    auto entries = registry ? At<void *>(registry, field::RegistryEntries) : nullptr;
    auto actor = entries ? At<void *>(entries, (handle >> 17) & 0x7ff8u) : nullptr;
    const bool valid = actor && At<unsigned>(actor, field::EntityHandle) == handle &&
        !(At<unsigned>(actor, field::EntityFlags) & 2) && At<unsigned>(actor, field::EntityKind) == PreviewActorKind;
    const auto count = valid ? At<unsigned>(actor, field::ItemChildCount) : 0;
    for (unsigned i = 0; i < PreviewCarriers.size(); ++i) {
        const auto child = count <= PreviewCarriers.size() && i < count ?
            At<unsigned>(actor, field::ItemChildren + i * sizeof(unsigned)) : 0;
        const auto previous = PreviewCarriers[i].exchange(child, std::memory_order_release);
        if (child && child != previous)
            Logging.Log("[QoL Enhancements/Core] DIY preview: mesh carrier published; slot=%u handle=%u\n", i, child);
    }
}
struct ModelMatrix { float values[12]; }; // ARM64 returns this 48-byte object through x8.
HOOK_DEFINE_TRAMPOLINE(BuildPreviewMatrix) {
    static ModelMatrix Callback(void *component) {
        const auto handle = PreviewHandle.load(std::memory_order_acquire);
        if (!handle || !component) return Orig(component);
        MatrixCalls.fetch_add(1, std::memory_order_relaxed);
        auto owner = At<void *>(component, field::ComponentOwner);
        if (!owner) return Orig(component);
        const auto parent = At<unsigned>(owner, field::ParentHandle);
        bool matches = At<unsigned>(owner, field::EntityHandle) == handle || parent == handle;
        if (!matches && parent && At<unsigned>(owner, field::EntityKind) == ItemDrawableKind)
            for (auto &carrier : PreviewCarriers)
                matches |= parent == carrier.load(std::memory_order_acquire);
        if (!matches) return Orig(component);
        // 0192A1F0 -> type 6 -> 019362F4 -> type 6F. Its 019282FC
        // transform copies the carrier, not the controller actor. Substitute
        // the owned pose at the final matrix boundary without scene mutation.
        alignas(16) std::array<std::byte, field::ComponentPoseSize> pose{};
        std::memcpy(pose.data(), component, pose.size());
        At<std::uint64_t>(pose.data(), field::ComponentAngles) = PreviewAngleXY.load(std::memory_order_acquire);
        At<unsigned>(pose.data(), field::ComponentAngles + 8) = PreviewAngleZ.load(std::memory_order_relaxed);
        if (PreviewHandle.load(std::memory_order_acquire) != handle) return Orig(component);
        if (MatrixMatches.fetch_add(1, std::memory_order_relaxed) == 0)
            Logging.Log("[QoL Enhancements/Core] DIY preview: drawable matrix matched; entity=%u parent=%u\n",
                        At<unsigned>(owner, field::EntityHandle), At<unsigned>(owner, field::ParentHandle));
        return Orig(pose.data());
    }
};
HOOK_DEFINE_TRAMPOLINE(SetPreviewRotation) {
    static bool Callback(void *controller, const void *angles) {
        // 032B9510 can reapply the native parameter animation through this
        // setter AFTER a queued transform succeeds. Keep the preview pose at
        // that final boundary too; other controllers retain the supplied pose.
        if (controller && controller == PreviewController.load(std::memory_order_acquire) &&
            At<unsigned>(controller, field::ModelMode) == WorkbenchModelMode &&
            At<int>(controller, field::ModelState) == 1 &&
            At<unsigned>(controller, field::ModelHandle))
            angles = static_cast<std::byte *>(controller) + field::ModelAngles;
        return Orig(controller, angles); // Preserve native entity/phase/handle validation.
    }
};
HOOK_DEFINE_TRAMPOLINE(TraceModelTransform) {
    static void Callback(void *controller) {
        const bool owned = controller && controller == PreviewController.load(std::memory_order_acquire);
        const auto before = owned ? At<unsigned char>(controller, field::ModelFlags) : 0;
        if (owned) TransformCalls.fetch_add(1, std::memory_order_relaxed);
        Orig(controller); // Observe native scheduling; never force an entity phase or retry.
        if (!owned) return;
        PublishCarriers(controller); // Child allocation can complete after model creation.
        if (!(before & TransformPending)) return;
        const auto after = At<unsigned char>(controller, field::ModelFlags);
        if (!(after & TransformPending)) TransformApplied.fetch_add(1, std::memory_order_relaxed);
        if (TransformAttempts.fetch_add(1, std::memory_order_relaxed) >= 8) return;
        auto storage = *Native<void **>(address::EntityManager);
        auto manager = storage ? At<void *>(storage) : nullptr;
        Logging.Log("[QoL Enhancements/Core] DIY preview: native transform; flags=%u->%u ready=%u entity_phase=%d entity_stage=%u\n",
                    unsigned(before), unsigned(after), At<unsigned char>(controller, field::ModelReady),
                    manager ? At<int>(manager, field::EntityPhase) : -99,
                    manager ? At<unsigned>(manager, field::EntityStage) : 0xffffffffu);
    }
};
void ReportRendererTrace() {
    Logging.Log("[QoL Enhancements/Core] DIY preview: render trace; native_updates=%u pending_attempts=%u applied=%u layer_draws=%u matched_layers=%u matrices=%u matched_matrices=%u backgrounds=%u\n",
                TransformCalls.load(std::memory_order_relaxed), TransformAttempts.load(std::memory_order_relaxed),
                TransformApplied.load(std::memory_order_relaxed), ClearCalls.load(std::memory_order_relaxed),
                ClearMatches.load(std::memory_order_relaxed), MatrixCalls.load(std::memory_order_relaxed),
                MatrixMatches.load(std::memory_order_relaxed), BackgroundApplied.load(std::memory_order_relaxed));
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
    void *phone{}, *layout{}, *surface{}, *capture{}, *gridPane{}, *scrollPane{}, *cursorPane{};
    void *controller{}, *parameters{};
    void *focusedRow{};
    Phase phase{};
    unsigned frames{};
    unsigned rejectedPresses{}, inputReports{}, stickReports{}, rotationReports{};
    unsigned char gridFlags{}, scrollFlags{}, cursorFlags{}, savedHidden{};
    std::array<std::byte, 0x28> savedParameters{};
    std::uint64_t item{};
    std::array<std::uint32_t, 3> initialAngles{};
    float stickX{}, stickY{}, tilt{};
    int inputRow{-1};
    int savedModelMode{-1};
    bool backHeld{}, confirmHeld{true}, ownsRenderer{};
};
std::array<Session, 8> Sessions{};
Session *ForPhone(void *phone) {
    if (!phone) return nullptr;
    const auto resident = At<unsigned char>(phone, field::Resident);
    if (resident >= Sessions.size()) return nullptr;
    auto &s = Sessions[resident]; return s.phone == phone ? &s : nullptr;
}
int PhoneRendererMode(void *phone) {
    // The native base frame (02b0c720) asks virtual +200 AFTER PhoneFrame.
    // It normally reserves mode 26/state 3, even with no model. While the
    // preview owns mode 2, report that profile so the base cannot reclaim it.
    // Replace only MenuRecipe's writable vtable slot, not the shared tiny
    // constant-return function (a trampoline would overlap its next function).
    auto s = ForPhone(phone);
    return s && s->ownsRenderer ? WorkbenchModelMode : PhoneModelMode;
}
bool PhoneReady(void *phone) {
    return At<int>(phone, field::State) == 3 && At<int>(phone, field::Requests) == -1 &&
           At<int>(phone, field::Requests + 4) == -1;
}
int ShortcutInputRow(void *layout) {
    auto hint = At<void *>(layout, field::ShortcutHint);
    auto input = hint ? At<void *>(hint, field::HintInput) : nullptr;
    // MenuRecipePage creates this shortcut with kind 5 in native 02e936ec.
    // Requiring the pocket-page kind 3 here silently declined app previews.
    // The row is shared by all buttons; B is read from the snapshot's BackMask.
    return input && At<unsigned>(input, field::InputKind) == PhoneShortcutKind &&
           At<unsigned>(input, field::InputRow) < 8 ? At<int>(input, field::InputRow) : -1;
}
bool RecipeRow(Session &s, void *source) {
    auto grid = At<void *>(s.layout, field::Grid);
    if (!grid || !source) return false;
    const auto count = At<unsigned>(grid, field::RowCount);
    const auto rows = At<void **>(grid, field::Rows);
    if (!rows || !count || count > 512) return false;
    for (unsigned i = 0; i < count; ++i) if (rows[i] == source) return true;
    return false;
}
void FocusRecipe(void *phone, void *event) {
    // Preserve the native scrolling-row -> recipe-index mapping and materials.
    Native<void (*)(void *, void *)>(address::RecipeFocus)(phone, event);
    if (auto s = ForPhone(phone)) {
        auto source = event ? At<void *>(event, 8) : nullptr;
        s->focusedRow = RecipeRow(*s, source) ? source : nullptr;
    }
}
void LeaveRecipe(void *phone, void *event) {
    Native<void (*)(void *, void *)>(address::RecipeLeave)(phone, event);
    if (auto s = ForPhone(phone); s && event && s->focusedRow == At<void *>(event, 8))
        s->focusedRow = nullptr;
}
bool InputBlocked(Session &s, unsigned blocked) {
    // Mirror 03c4b880's page/global blocking before taking a new A press.
    auto input = At<void *>(s.layout, field::PageInputContext);
    if ((blocked & 1) || !input || At<unsigned char>(input, field::InputDisabled)) return true;
    const auto mask = At<unsigned>(input, field::InputBlockMask);
    if (!mask) return false;
    auto app = *Native<void **>(address::Application);
    auto masks = app ? At<void *>(app, field::ApplicationMasks) : nullptr;
    return !masks || (mask & At<unsigned>(masks, field::GlobalBlockMask));
}
void Close(Session &s) {
    s.stickX = s.stickY = 0;
    if (s.ownsRenderer) {
        ReportRendererTrace();
        PreviewHandle.store(0, std::memory_order_release);
        for (auto &carrier : PreviewCarriers) carrier.store(0, std::memory_order_release);
        PreviewController.store(nullptr, std::memory_order_release);
        PreviewClearColor.store(nullptr, std::memory_order_release);
        // Release only the handle/mode acquired by this phone. The native
        // state-1 exit balances camera/layers before state 0 resets ownership.
        auto context = UiContext();
        const bool stillOwned = context && At<void *>(context, field::Controller) == s.controller &&
            At<unsigned>(s.controller, field::ModelMode) == WorkbenchModelMode;
        if (stillOwned)
            Native<void (*)(void *, unsigned)>(address::ReleaseModel)(s.controller, WorkbenchModelMode);
        if (stillOwned && At<void *>(context, field::Parameters) == s.parameters) {
            std::memcpy(static_cast<std::byte *>(s.parameters) + 0x2c, s.savedParameters.data(), s.savedParameters.size());
            At<unsigned char>(s.parameters, 0x12) = s.savedHidden;
            At<unsigned>(s.parameters, 0x2140) = 1;
        }
        if (stillOwned && s.savedModelMode == PhoneModelMode) {
            At<int>(s.controller, field::ModelMode) = PhoneModelMode;
            // State 1 exit and state 3 entry balance the camera reference
            // counts. Restore the phone reservation before its next frame.
            Native<void (*)(void *, unsigned)>(address::ChangeState)(
                static_cast<std::byte *>(s.controller) + 8, PhoneModelState);
        }
    }
    if (s.ownsRenderer && s.gridPane) At<unsigned char>(s.gridPane, field::PaneFlags) = s.gridFlags;
    if (s.ownsRenderer && s.scrollPane) At<unsigned char>(s.scrollPane, field::PaneFlags) = s.scrollFlags;
    if (s.ownsRenderer && s.cursorPane) At<unsigned char>(s.cursorPane, field::PaneFlags) = s.cursorFlags;
    Visible(s.surface, false);
    Visible(s.capture, false);
    s.controller = s.parameters = nullptr; s.ownsRenderer = false;
    s.phase = Phase::DrainBack;
    Logging.Log("[QoL Enhancements/Core] DIY preview: grid restored; renderer released\n");
}
bool QueuePreview(Session &s) {
    const auto count = At<unsigned>(s.phone, field::Recipes);
    const auto selected = At<unsigned>(s.phone, field::Selection);
    const auto recipes = At<void **>(s.phone, field::Recipes + 8);
    const bool recipeRow = RecipeRow(s, s.focusedRow);
    if (!PhoneReady(s.phone) || !recipeRow || selected >= count || count > 4096 || !recipes || !recipes[selected] ||
        At<std::uint16_t>(recipes[selected], 8) >= 0xfffe || !s.surface || !s.gridPane) {
        if (s.rejectedPresses++ < 8)
            Logging.Log("[QoL Enhancements/Core] DIY preview: A declined; recipe_row=%u selected=%u count=%u surface=%p grid=%p input_row=%d\n",
                        unsigned(recipeRow), selected, count, s.surface, s.gridPane, s.inputRow);
        return false;
    }
    s.item = At<std::uint64_t>(recipes[selected], 8);
    s.phase = Phase::Queued;
    Logging.Log("[QoL Enhancements/Core] DIY preview: A queued from input; recipe_index=%u item=%u\n", selected, unsigned(s.item & 0xffff));
    return true;
}
unsigned ObserveFocus(void *layout) {
    const auto columns = At<unsigned>(layout, 0x430), rows = At<unsigned>(layout, 0x434);
    const auto entries = At<std::byte *>(layout, 0x438);
    if (!entries || columns > 64 || rows != 0x23) return 0;
    unsigned replaced{};
    for (unsigned row = 0; row < 2; ++row) for (unsigned column = 0; column < columns; ++column) {
        auto &callback = At<std::uintptr_t>(entries + (row * columns + column) * 0x20, 0x10);
        const auto native = Main + (row ? address::RecipeLeave : address::RecipeFocus);
        const auto observer = reinterpret_cast<std::uintptr_t>(row ? &LeaveRecipe : &FocusRecipe);
        if (callback == native) { callback = observer; ++replaced; }
        else if (callback == observer) ++replaced;
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
        s.capture = Find(root, recipe_layout::CaptureName);
        auto picture = Find(root, recipe_layout::PictureName);
        bool captureBound = false;
        if (s.capture && picture) {
            if (auto typed = Native<void *(*)(void *)>(address::CapturePicture)(picture))
                captureBound = At<void *>(typed, 0x100) == s.capture;
            else if (auto typed = Native<void *(*)(void *)>(address::AlternateCapturePicture)(picture))
                captureBound = At<void *>(typed, 0x130) == s.capture;
        }
        if (!captureBound) s.surface = nullptr;
        s.gridPane = Find(root, "L_BtnList_00");
        s.scrollPane = Find(root, "L_ScrollBar_00");
        s.cursorPane = Find(root, "L_Cursor_00");
        if (s.gridPane) s.gridFlags = At<unsigned char>(s.gridPane, field::PaneFlags);
        if (s.scrollPane) s.scrollFlags = At<unsigned char>(s.scrollPane, field::PaneFlags);
        if (s.cursorPane) s.cursorFlags = At<unsigned char>(s.cursorPane, field::PaneFlags);
        s.inputRow = ShortcutInputRow(s.layout);
        const auto focusBindings = ObserveFocus(s.layout);
        Logging.Log("[QoL Enhancements/Core] DIY preview: app bound; surface=%p grid=%p input_row=%d focus_bindings=%u\n",
                    s.surface, s.gridPane, s.inputRow, focusBindings);
    }
};
float StickAxis(float value) {
    // Reject NaN/infinity and apply a dead zone to avoid controller drift.
    if (!(value >= -1.0f && value <= 1.0f)) return 0;
    if (value > StickDeadZone) return (value - StickDeadZone) / (1.0f - StickDeadZone);
    if (value < -StickDeadZone) return (value + StickDeadZone) / (1.0f - StickDeadZone);
    return 0;
}
void RotatePreview(Session &s) {
    const auto x = s.stickX, y = s.stickY;
    s.stickX = s.stickY = 0; // A missing/blocked snapshot must never keep spinning.
    if (x == 0 && y == 0) return;
    auto context = UiContext();
    if (!s.ownsRenderer || !context || At<void *>(context, field::Controller) != s.controller ||
        At<unsigned>(s.controller, field::ModelMode) != WorkbenchModelMode ||
        At<int>(s.controller, field::ModelState) != 1 || !At<unsigned>(s.controller, field::ModelHandle) ||
        (At<unsigned char>(s.controller, field::ModelFlags) & MaterialPending)) return;
    std::array<std::uint32_t, 3> angles{};
    std::memcpy(angles.data(), static_cast<std::byte *>(s.controller) + field::ModelAngles, sizeof(angles));
    const float tilt = std::clamp(s.tilt - y * DegreesPerUpdate, -MaximumTilt, MaximumTilt);
    angles[0] = s.initialAngles[0] + static_cast<std::uint32_t>(static_cast<std::int32_t>(tilt * AngleUnitsPerDegree));
    angles[1] -= static_cast<std::uint32_t>(static_cast<std::int32_t>(x * DegreesPerUpdate * AngleUnitsPerDegree));
    // Queue for the native late entity phase. The setter hook also protects
    // this pose from parameter animation writes in 032B9510. 032BE2B0 sets
    // the material-publication gate (+398); it is not a transform dirty flag.
    std::memcpy(static_cast<std::byte *>(s.controller) + field::ModelAngles, angles.data(), sizeof(angles));
    At<unsigned char>(s.controller, field::ModelFlags) |= TransformPending;
    s.tilt = tilt;
    PublishPose(s.controller);
    if (s.rotationReports++ < 4)
        Logging.Log("[QoL Enhancements/Core] DIY preview: rotation queued; yaw=%08x pitch=%08x ready=%u flags=%u\n",
                    angles[1], angles[0], At<unsigned char>(s.controller, field::ModelReady),
                    At<unsigned char>(s.controller, field::ModelFlags));
}
bool PreviewInput(void *layout, void *snapshot, unsigned blocked) {
    for (auto &s : Sessions) if (s.layout == layout) {
        if (s.phase != Phase::Showing) s.stickX = s.stickY = 0;
        if (s.phase == Phase::Idle) s.inputRow = ShortcutInputRow(s.layout);
        if (!snapshot || s.inputRow < 0) { s.stickX = s.stickY = 0; return s.phase != Phase::Idle; }
        const auto count = At<unsigned>(snapshot, 8);
        auto rows = At<std::byte *>(snapshot, 0x10);
        if (!rows || count > 8 || unsigned(s.inputRow) >= count) {
            s.stickX = s.stickY = 0; return s.phase != Phase::Idle;
        }
        auto row = rows + s.inputRow * 0x48;
        const auto buttons = At<unsigned>(row);
        const bool confirm = (buttons & ConfirmMask) != 0;
        const bool confirmPress = confirm && !s.confirmHeld; s.confirmHeld = confirm;
        if (s.phase == Phase::Idle) {
            if (!confirmPress) return false;
            if (s.inputReports++ < 8)
                Logging.Log("[QoL Enhancements/Core] DIY preview: A input; state=%d pending=%d/%d focus=%p blocked=%u consumed=%x\n",
                            At<int>(s.phone, field::State), At<int>(s.phone, field::Requests), At<int>(s.phone, field::Requests + 4),
                            s.focusedRow, unsigned(InputBlocked(s, blocked)), At<unsigned>(row, 0x3c));
            if (InputBlocked(s, blocked) || (At<unsigned>(row, 0x3c) & ConfirmMask) || !QueuePreview(s)) return false;
            At<unsigned>(row, 0x3c) |= ConfirmMask;
            return true;
        }
        const bool held = (At<unsigned>(row) & BackMask) != 0;
        const bool press = held && !s.backHeld; s.backHeld = held;
        if (s.phase == Phase::DrainBack) {
            if (!held) s.phase = Phase::Idle;
        } else if (press && !(blocked & 1) && !(At<unsigned>(row, 0x3c) & BackMask)) {
            s.phase = Phase::Closing;
            At<unsigned>(row, 0x3c) |= BackMask;
        }
        // Native 03C4BAC4 reads float axis pairs at row +4 + kind*8.
        // Kind 1 is the right stick; +40 bit 1 records its consumption.
        if (s.phase == Phase::Showing) {
            const bool inputBlocked = InputBlocked(s, blocked);
            const auto x = StickAxis(At<float>(row, field::RightStick));
            const auto y = StickAxis(At<float>(row, field::RightStick + 4));
            if ((x != 0 || y != 0) && s.stickReports++ < 4)
                Logging.Log("[QoL Enhancements/Core] DIY preview: right-stick snapshot; row=%d x=%08x y=%08x consumed=%x blocked=%u\n",
                            s.inputRow, At<unsigned>(row, field::RightStick), At<unsigned>(row, field::RightStick + 4),
                            At<unsigned>(row, field::ConsumedAxes), unsigned(inputBlocked));
            if (inputBlocked) s.stickX = s.stickY = 0;
            else if (!(At<unsigned>(row, field::ConsumedAxes) & RightStickMask)) {
                s.stickX = x; s.stickY = y;
                At<unsigned>(row, field::ConsumedAxes) |= RightStickMask;
            }
            // A second dispatch of the already-consumed snapshot must not
            // erase input queued for this frame. RotatePreview drains it.
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
    if (!controller || !parameters) {
        Logging.Log("[QoL Enhancements/Core] DIY preview: missing renderer; context=%p controller=%p parameters=%p\n",
                    context, controller, parameters);
        return false;
    }
    const auto mode = At<int>(controller, field::ModelMode);
    const auto state = At<int>(controller, field::ModelState);
    const bool available = (mode == -1 && state == 0) || (mode == PhoneModelMode && state == PhoneModelState);
    if (!available || At<unsigned>(controller, field::ModelHandle) ||
        At<void *>(controller, field::BackgroundPage) || At<void *>(controller, field::VisibilityPage)) {
        Logging.Log("[QoL Enhancements/Core] DIY preview: renderer refused; mode=%d state=%d handle=%u background=%p visibility=%p\n",
                    mode, state, At<unsigned>(controller, field::ModelHandle),
                    At<void *>(controller, field::BackgroundPage), At<void *>(controller, field::VisibilityPage));
        return false;
    }
    s.controller = controller; s.parameters = parameters;
    s.savedModelMode = mode;
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
    At<void *>(controller, field::BackgroundPage) = nullptr; // No background page owner.
    At<unsigned char>(controller, 0x68) = 0;
    s.ownsRenderer = true; s.frames = 0;
    TransformCalls.store(0, std::memory_order_relaxed);
    TransformAttempts.store(0, std::memory_order_relaxed);
    TransformApplied.store(0, std::memory_order_relaxed);
    ClearCalls.store(0, std::memory_order_relaxed);
    ClearMatches.store(0, std::memory_order_relaxed);
    MatrixCalls.store(0, std::memory_order_relaxed);
    MatrixMatches.store(0, std::memory_order_relaxed);
    BackgroundApplied.store(0, std::memory_order_relaxed);
    PreviewController.store(controller, std::memory_order_release);
    const auto color = ItemLayerClearColor();
    PreviewClearColor.store(color, std::memory_order_release);
    Logging.Log("[QoL Enhancements/Core] DIY preview: background #E5DFC7; layer_color=%p\n", color);
    Native<void (*)(void *, unsigned)>(address::ChangeState)(static_cast<std::byte *>(controller) + 8, 1);
    s.phase = Phase::Starting;
    Logging.Log("[QoL Enhancements/Core] DIY preview: renderer acquired; previous_mode=%d previous_state=%d\n", mode, state);
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
        if (s->phase == Phase::Showing) RotatePreview(*s);
        if (s->phase == Phase::Showing && ++s->frames == 30) {
            ReportRendererTrace();
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
        std::memcpy(s->initialAngles.data(), static_cast<std::byte *>(s->controller) + field::ModelAngles, sizeof(s->initialAngles));
        PublishPose(s->controller);
        s->tilt = s->stickX = s->stickY = 0;
        Visible(s->gridPane, false); Visible(s->scrollPane, false); Visible(s->cursorPane, false);
        Visible(s->capture, true); Visible(s->surface, true); s->phase = Phase::Showing;
        Logging.Log("[QoL Enhancements/Core] DIY preview: item model created; handle=%u\n", At<unsigned>(s->controller, field::ModelHandle));
    }
};
HOOK_DEFINE_TRAMPOLINE(DeactivatePhoneRecipes) {
    static void Callback(void *phone) {
        if (auto s = ForPhone(phone)) {
            if (s->phase != Phase::Idle) Close(*s);
            s->phase = Phase::Idle; s->backHeld = false; s->confirmHeld = true;
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
        {0x02e96a80, {0xf9410808, 0xf942c109, 0xb4000789, 0xf940042b, 0xb400074b}},
        {0x03c4b880, {0xd10203ff, 0xa9027bfd, 0xa9036ffc, 0xa90467fa, 0xa9055ff8}},
        {0x02b0c810, {0xf9400268, 0xaa1303e0, 0xf940d108, 0xd63f0100, 0xf9400268}},
        {0x02ea0754, {0x52800340, 0xd65f03c0, 0xf9400408, 0xb4000148, 0xa9412802}},
        {0x032b9e08, {0xa9bc7bfd, 0xf9000bf7, 0xa90257f6, 0xa9034ff4, 0x910003fd}},
        {0x032ba000, {0x91020261, 0xaa1303e0, 0x97ffff80, 0x36000200, 0xb9405268}},
        {0x03c4bac4, {0xbd400100, 0xbc038c20, 0xbd400500, 0xf8418c08, 0xf90003fb}},
        {0x032b9fcc, {0xd100c3ff, 0xa9017bfd, 0xa9024ff4, 0x910043fd, 0x3941d008}},
        {0x02b0e20c, {0xf0015948, 0x91064108, 0xf9400108, 0xf9401900, 0x141eaf6c}},
        {0x039392b0, {0xd10203ff, 0x6d032beb, 0x6d0423e9, 0xa9057bfd, 0xa90657f6}},
        {0x006021a4, {0xb942b00c, 0x12005d8f, 0xb942bc09, 0xb942c00b, 0xb942c40a}},
        {0x039f63f0, {0x6db63bef, 0x6d0133ed, 0x6d022beb, 0x6d0323e9, 0xa9047bfd}},
        {0x032b9750, {0xaa1403e1, 0x940001ad, 0x9000f3c8, 0x91240108, 0xf9400108}},
        {0x01513a48, {0xd101c3ff, 0x6d012beb, 0x6d0223e9, 0xa9037bfd, 0xa9045ff8}},
        {0x032be2b0, {0xb9406c09, 0x34000629, 0xa9be7bfd, 0xa9014ff4, 0x910003fd}},
        {0x032ba034, {0xaa1303e0, 0x9400109e, 0x36000080, 0x3941d268, 0x121f1908}},
        {0x0192a1f0, {0xa9bb7bfd, 0xf9000bf9, 0xa9025ff8, 0xa90357f6, 0xa9044ff4}},
        {0x019362f4, {0xd101c3ff, 0xa9027bfd, 0xf9001bf9, 0xa9045ff8, 0xa90557f6}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &f : fingerprints) if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + f.offset), f.words, sizeof(f.words))) {
        Logging.Log("[QoL Enhancements/Core] DIY preview: DISABLED; instruction mismatch at main+0x%lx\n", f.offset); return false;
    }
    Main = main.m_Text.m_Start;
    const auto modeSlot = Main + address::PhoneRendererModeSlot;
    if (!main.m_Data.InRange(modeSlot) || !main.m_Data.InRange(modeSlot + sizeof(std::uintptr_t) - 1) ||
        *reinterpret_cast<const std::uintptr_t *>(modeSlot) != Main + address::PhoneRendererMode) {
        Logging.Log("[QoL Enhancements/Core] DIY preview: DISABLED; renderer mode slot mismatch\n");
        return false;
    }
    if (!RegisterPageInput(PreviewInput)) return false;
    auto slot = reinterpret_cast<volatile std::uintptr_t *>(modeSlot);
    const auto replacement = reinterpret_cast<std::uintptr_t>(&PhoneRendererMode);
    *slot = replacement;
    if (*slot != replacement) {
        Logging.Log("[QoL Enhancements/Core] DIY preview: DISABLED; renderer mode slot write failed\n");
        return false;
    }
    BuildRecipeLayout::InstallAtOffset(address::BuildLayout);
    BindPhoneRecipes::InstallAtOffset(address::PhoneBind);
    UpdatePhoneRecipes::InstallAtOffset(address::PhoneFrame);
    DeactivatePhoneRecipes::InstallAtOffset(address::PhoneDeactivate);
    DrawPreviewLayer::InstallAtOffset(address::DrawModelLayer);
    SetPreviewRotation::InstallAtOffset(address::SetModelRotation);
    TraceModelTransform::InstallAtOffset(address::UpdateModelTransform);
    BuildPreviewMatrix::InstallAtOffset(address::BuildModelMatrix);
    DrawPreviewContents::InstallAtOffset(address::DrawLayerContents);
    Logging.Log("[QoL Enhancements/Core] DIY preview: READY; carrier-scoped drawable pose; A opens, B returns\n");
    return true;
}
} // namespace qol::core
