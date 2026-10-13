#include "preview_context.hpp"
#include "program/core/hook_plan.hpp"
namespace qol::crafting::preview_internal {
using Phase = preview::Phase;
struct NativeSession {
    void *phone{}, *layout{}, *surface{}, *capture{}, *gridPane{}, *scrollPane{}, *cursorPane{};
    void *controller{}, *parameters{};
    void *focusedRow{};
    recipe_banner::Placement hotelBanner{};
    preview::Session lifecycle;
    unsigned footerKind{};
    bool refreshFooter{};
    unsigned rejectedPresses{}, inputReports{}, stickReports{}, rotationReports{};
    unsigned char gridFlags{}, scrollFlags{}, cursorFlags{}, savedHidden{};
    std::array<std::byte, 0x28> savedParameters{};
    std::uint64_t item{};
    std::array<std::uint32_t, 3> initialAngles{};
    int inputRow{-1};
    int savedModelMode{-1};
    bool backHeld{}, confirmHeld{true}, ownsRenderer{}, fixedPose{};
};
std::array<NativeSession, 8> Sessions{};
NativeSession *ForPhone(void *phone) {
    if (!phone) return nullptr;
    const auto resident = At<unsigned char>(phone, field::Resident);
    if (resident >= Sessions.size()) return nullptr;
    auto &s = Sessions[resident]; return s.phone == phone ? &s : nullptr;
}
void BuildPhoneRecipeHints::Callback(void *phone, unsigned kind){
        Orig(phone, kind);
        auto session = ForPhone(phone);
        if (session) { session->footerKind = kind; session->refreshFooter = false; }
        // Native kind 1 is category focus. Only advertise A on the recipe
        // list; the preview keeps the normal Sort/Favorite/Close hints.
        const auto count = At<unsigned>(phone, field::Recipes);
        if (features::ShowRecipePreviewHint(kind & 1, count, At<unsigned>(phone, field::Selection),
            session ? session->lifecycle.State() : Phase::Idle))
            recipe_hint::Append(Main, recipe_hint::Footer(Main, UiContext()),
                                At<unsigned char>(phone, field::Resident));
    }

void RefreshRecipeHints(NativeSession &session) {
    if (!session.refreshFooter) return;
    auto footer = recipe_hint::Footer(Main, UiContext());
    // KeyGuide consumes its pending records once per UI update (02DE3E80).
    // Wait for an empty batch rather than duplicating or freeing native hints.
    if (!footer || At<int>(footer, recipe_hint::PendingCount) != 0) return;
    BuildPhoneRecipeHints::Callback(session.phone, session.footerKind);
}
int PhoneRendererMode(void *phone) {
    // The native base frame (02b0c720) asks virtual +200 AFTER PhoneFrame.
    // It normally reserves mode 26/state 3, even with no model. While the
    // preview owns mode 2, report that profile so the base cannot reclaim it.
    // Replace only MenuRecipe's writable vtable slot, not the shared tiny
    // constant-return function (a trampoline would overlap its next function).
    auto s = ForPhone(phone);
    return s && s->ownsRenderer ? DiyModelMode : PhoneModelMode;
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
bool RecipeRow(NativeSession &s, void *source) {
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
    profile::RecipeFocus.Resolve(Main)(phone, event);
    if (auto s = ForPhone(phone)) {
        auto source = event ? At<void *>(event, 8) : nullptr;
        s->focusedRow = RecipeRow(*s, source) ? source : nullptr;
    }
}
void LeaveRecipe(void *phone, void *event) {
    profile::RecipeLeave.Resolve(Main)(phone, event);
    if (auto s = ForPhone(phone); s && event && s->focusedRow == At<void *>(event, 8))
        s->focusedRow = nullptr;
}
bool InputBlocked(NativeSession &s, unsigned blocked) {
    // Mirror 03c4b880's page/global blocking before taking a new A press.
    auto input = At<void *>(s.layout, field::PageInputContext);
    if ((blocked & 1) || !input || At<unsigned char>(input, field::InputDisabled)) return true;
    const auto mask = At<unsigned>(input, field::InputBlockMask);
    if (!mask) return false;
    auto app = game::preview_native::Application(Main);
    auto masks = app ? At<void *>(app, field::ApplicationMasks) : nullptr;
    return !masks || (mask & At<unsigned>(masks, field::GlobalBlockMask));
}
void Close(NativeSession &s) {
    recipe_banner::Restore(s.hotelBanner);
    s.lifecycle.ResetNavigation();
    s.lifecycle.CancelRotationInput();
    if (s.ownsRenderer) {
        ReportRendererTrace();
        PreviewScenePose.store(false, std::memory_order_release);
        ClearPublishedPose();
        for (auto &carrier : PreviewCarriers) carrier.store(0, std::memory_order_release);
        PreviewController.store(nullptr, std::memory_order_release);
        PreviewFixedPose.store(false, std::memory_order_release);
        PreviewClearColor.store(nullptr, std::memory_order_release);
        // Release only the handle/mode acquired by this phone. The native
        // state-1 exit balances camera/layers before state 0 resets ownership.
        auto context = UiContext();
        const bool stillOwned = context && At<void *>(context, field::Controller) == s.controller &&
            At<unsigned>(s.controller, field::ModelMode) == DiyModelMode;
        if (stillOwned)
            profile::ReleaseModel.Resolve(Main)(s.controller, DiyModelMode);
        if (stillOwned && At<void *>(context, field::Parameters) == s.parameters) {
            std::memcpy(static_cast<std::byte *>(s.parameters) + 0x2c, s.savedParameters.data(), s.savedParameters.size());
            At<unsigned char>(s.parameters, 0x12) = s.savedHidden;
            At<unsigned>(s.parameters, 0x2140) = 1;
        }
        if (stillOwned && s.savedModelMode == PhoneModelMode) {
            At<int>(s.controller, field::ModelMode) = PhoneModelMode;
            // State 1 exit and state 3 entry balance the camera reference
            // counts. Restore the phone reservation before its next frame.
            profile::ChangeState.Resolve(Main)(
                static_cast<std::byte *>(s.controller) + 8, PhoneModelState);
        }
    }
    if (s.ownsRenderer && s.gridPane) At<unsigned char>(s.gridPane, field::PaneFlags) = s.gridFlags;
    if (s.ownsRenderer && s.scrollPane) At<unsigned char>(s.scrollPane, field::PaneFlags) = s.scrollFlags;
    if (s.ownsRenderer && s.cursorPane) At<unsigned char>(s.cursorPane, field::PaneFlags) = s.cursorFlags;
    Visible(s.surface, false);
    Visible(s.capture, false);
    s.controller = s.parameters = nullptr; s.ownsRenderer = false;
    s.lifecycle.FinishClose();
    s.refreshFooter = true;
    Logging.Debug("[QoL Enhancements/Crafting] DIY preview: grid restored; renderer released\n");
}
bool QueuePreview(NativeSession &s) {
    struct Recipes : game::RecipeOpening {
        NativeSession &s;
        explicit Recipes(NativeSession &session) : s(session) {}
        unsigned Count() override { return At<unsigned>(s.phone, field::Recipes); }
        unsigned Selected() override { return At<unsigned>(s.phone, field::Selection); }
        bool FocusedRecipeRow() override { return RecipeRow(s, s.focusedRow); }
        bool PhoneReady() override { return preview_internal::PhoneReady(s.phone); }
        void **Rows() const { return At<void **>(s.phone, field::Recipes + 8); }
        bool HasRecipeList() override { return Rows(); }
        bool HasRecipe(unsigned selected) override { return Rows()[selected]; }
        bool HasValidItem(unsigned selected) override { return At<std::uint16_t>(Rows()[selected], 8) < 0xfffe; }
        bool HasSurface() override { return s.surface; }
        bool HasGrid() override { return s.gridPane; }
        std::uint64_t CopyItem(unsigned selected) override { return At<std::uint64_t>(Rows()[selected], 8); }
        void ReportDeclined(bool row, unsigned selected, unsigned count) override {
            if (s.rejectedPresses++ < 8)
                Logging.Debug("[QoL Enhancements/Crafting] DIY preview: A declined; recipe_row=%u selected=%u count=%u surface=%p grid=%p input_row=%d\n",
                              unsigned(row), selected, count, s.surface, s.gridPane, s.inputRow);
        }
        void ReportQueued(unsigned selected) override {
            s.refreshFooter = true;
            Logging.Debug("[QoL Enhancements/Crafting] DIY preview: A queued from input; recipe_index=%u item=%u\n", selected, unsigned(s.item & 0xffff));
        }
    } recipes{s};
    return features::QueueRecipePreview(s.lifecycle, recipes, s.item);
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
void BindPhoneRecipes::Callback(void *phone){
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
            if (auto typed = profile::CapturePicture.Resolve(Main)(picture))
                captureBound = At<void *>(typed, 0x100) == s.capture;
            else if (auto typed = profile::AlternateCapturePicture.Resolve(Main)(picture))
                captureBound = At<void *>(typed, 0x130) == s.capture;
        }
        if (!captureBound) s.surface = nullptr;
        s.gridPane = Find(root, "L_BtnList_00");
        s.scrollPane = Find(root, "L_ScrollBar_00");
        s.cursorPane = Find(root, "L_Cursor_00");
        s.hotelBanner.pane = Find(root, "N_TradeBox_00");
        s.hotelBanner.background = Find(s.hotelBanner.pane, "P_TradeBoxBase_00");
        if (s.gridPane) s.gridFlags = At<unsigned char>(s.gridPane, field::PaneFlags);
        if (s.scrollPane) s.scrollFlags = At<unsigned char>(s.scrollPane, field::PaneFlags);
        if (s.cursorPane) s.cursorFlags = At<unsigned char>(s.cursorPane, field::PaneFlags);
        s.inputRow = ShortcutInputRow(s.layout);
        const auto focusBindings = ObserveFocus(s.layout);
        Logging.Debug("[QoL Enhancements/Crafting] DIY preview: app bound; surface=%p grid=%p input_row=%d focus_bindings=%u\n",
                    s.surface, s.gridPane, s.inputRow, focusBindings);
    }

bool WorkbenchTiltInput(void *page, void *snapshot, unsigned blocked) {
    auto context = UiContext();
    auto controller = context ? At<void *>(context, field::Controller) : nullptr;
    // Log before ownership/state/consumption guards, once for each page/mode
    // combination with vertical input (bounded globally to 16). This reveals
    // rejected native bindings without changing or consuming their input.
    struct InputProbe { std::uintptr_t pageClass{}; unsigned mode{}; };
    static std::array<InputProbe, 16> probes{};
    static unsigned probeCount{};
    if (page && controller && snapshot && probeCount < probes.size()) {
        const auto count = At<unsigned>(snapshot, 8);
        auto rows = At<std::byte *>(snapshot, 0x10);
        const auto pageClass = At<std::uintptr_t>(page);
        const auto mode = At<unsigned>(controller, field::ModelMode);
        bool seen = false;
        for (unsigned i = 0; i < probeCount; ++i) seen |= probes[i].pageClass == pageClass && probes[i].mode == mode;
        if (!seen && rows && count <= 8) for (unsigned i = 0; i < count; ++i) {
            auto row = rows + i * 0x48;
            if (!features::PreviewStickAxis(At<float>(row, field::RightStick + 4))) continue;
            probes[probeCount++] = {pageClass, mode};
            Logging.Debug("[QoL Enhancements/Crafting] workbench tilt: input probe; page=main+%lx mode=%u state=%d handle=%u flags=%u row=%u y=%d consumed=%u blocked=%u\n",
                        pageClass - Main, mode, At<int>(controller, field::ModelState),
                        At<unsigned>(controller, field::ModelHandle), At<unsigned char>(controller, field::ModelFlags),
                        i, int(At<float>(row, field::RightStick + 4) * 1000), At<unsigned>(row, field::ConsumedAxes), blocked);
            break;
        }
    }
    if (!page || !controller || controller == PreviewController.load(std::memory_order_acquire) ||
        !IsItemModelProfile(At<unsigned>(controller, field::ModelMode)) ||
        At<int>(controller, field::ModelState) != 1) return false;
    // WorkCraftInfo's page is constructed with this vtable by 0346E7E8 and
    // dispatches through SharedPageInput. 034951BC selects background page 3,
    // not the item controller's profile; retain the native-selected profile.
    // Controller +60 is the nested /L_LoadingIcon_00 (0346E9C0), NOT one of
    // page +3F8's top-level wrappers. The former array check rejected it.
    if (At<std::uintptr_t>(page) != Main + address::WorkbenchPageVtable) return false;
    const auto handle = At<unsigned>(controller, field::ModelHandle);
    const auto mode = At<unsigned>(controller, field::ModelMode);
    const auto item = At<std::uint64_t>(controller, field::CompletedItem); // Native completed item request.
    auto &state = WorkbenchInputState;
    const bool pending = At<unsigned char>(controller, field::ModelFlags) & MaterialPending;
    const bool bindingChanged = state.controller != controller || state.page != page || state.handle != handle ||
        state.mode != mode || (!pending && state.item != item);
    if (bindingChanged || !handle)
        state = {controller, page, handle, mode, item, 0, 0};
    {
        PublicationClock::Write publish{WorkbenchPublication};
        WorkbenchTiltAngle.store(static_cast<unsigned>(static_cast<std::int32_t>(state.tilt * AngleUnitsPerDegree)), std::memory_order_seq_cst);
        WorkbenchHandle.store(handle, std::memory_order_seq_cst);
        WorkbenchMode.store(mode, std::memory_order_seq_cst);
        WorkbenchController.store(controller, std::memory_order_seq_cst);
    }
    auto hint = At<void *>(page, field::ShortcutHint);
    auto key = hint ? At<void *>(hint, field::HintInput) : nullptr;
    const auto rowIndex = key ? At<unsigned>(key, field::InputRow) : 8;
    NativeSession inputScope{}; inputScope.layout = page;
    const bool inputBlocked = InputBlocked(inputScope, blocked);
    if (bindingChanged)
        Logging.Debug("[QoL Enhancements/Crafting] workbench tilt: bound; mode=%u handle=%u row=%u flags=%u blocked=%u\n",
                    mode, handle, rowIndex, At<unsigned char>(controller, field::ModelFlags), unsigned(inputBlocked));
    if (!handle || !snapshot || rowIndex >= 8 || inputBlocked ||
        (At<unsigned char>(controller, field::ModelFlags) & MaterialPending)) return false;
    const auto count = At<unsigned>(snapshot, 8);
    auto rows = At<std::byte *>(snapshot, 0x10);
    if (!rows || count > 8 || rowIndex >= count) return false;
    auto row = rows + rowIndex * 0x48;
    if (At<unsigned>(row, field::ConsumedAxes) & RightStickMask) return false;
    const float y = features::PreviewStickAxis(At<float>(row, field::RightStick + 4));
    state.tilt = features::TiltWorkbench(state.tilt, y);
    {
        PublicationClock::Write publish{WorkbenchPublication};
        WorkbenchTiltAngle.store(static_cast<unsigned>(static_cast<std::int32_t>(state.tilt * AngleUnitsPerDegree)));
    }
    if (y && state.rotationReports++ < 4)
        Logging.Debug("[QoL Enhancements/Crafting] workbench tilt: input; row=%u pitch=%08x handle=%u\n",
                    rowIndex, WorkbenchTiltAngle.load(std::memory_order_relaxed), handle);
    // Leave native dispatch and X intact: horizontal rotation, scroll, A/B,
    // recipe switching and crafting remain owned by the workbench.
    return false;
}
core::InputOutcome WorkbenchPageInput(const core::PageInputContext &input) {
    return WorkbenchTiltInput(input.page, input.snapshot, input.blocked) ? core::InputOutcome::Consume : core::InputOutcome::PassThrough;
}
void RotatePreview(NativeSession &s) {
    struct Renderer : game::PreviewRotation {
        NativeSession &s;
        explicit Renderer(NativeSession &session) : s(session) {}
        bool FixedPose() override { return s.fixedPose; }
        bool RendererReady() override {
            auto context = UiContext();
            return s.ownsRenderer && context && At<void *>(context, field::Controller) == s.controller &&
                At<unsigned>(s.controller, field::ModelMode) == DiyModelMode &&
                At<int>(s.controller, field::ModelState) == 1 && At<unsigned>(s.controller, field::ModelHandle) &&
                !(At<unsigned char>(s.controller, field::ModelFlags) & MaterialPending);
        }
        void QueueRotation(float yawDelta, float tilt) override {
            std::array<std::uint32_t, 3> angles{};
            std::memcpy(angles.data(), static_cast<std::byte *>(s.controller) + field::ModelAngles, sizeof(angles));
            angles[0] = s.initialAngles[0] + static_cast<std::uint32_t>(static_cast<std::int32_t>(tilt * AngleUnitsPerDegree));
            angles[1] += static_cast<std::uint32_t>(static_cast<std::int32_t>(yawDelta * AngleUnitsPerDegree));
            // Native late entity phase owns application. +398 gates material
            // publication, not transform dirtiness; keep the setter protection.
            std::memcpy(static_cast<std::byte *>(s.controller) + field::ModelAngles, angles.data(), sizeof(angles));
            At<unsigned char>(s.controller, field::ModelFlags) |= TransformPending;
            PublishPose(s.controller);
            if (s.rotationReports++ < 4)
                Logging.Debug("[QoL Enhancements/Crafting] DIY preview: rotation queued; yaw=%08x pitch=%08x ready=%u flags=%u\n",
                              angles[1], angles[0], At<unsigned char>(s.controller, field::ModelReady),
                              At<unsigned char>(s.controller, field::ModelFlags));
        }
    } renderer{s};
    features::RotateRecipePreview(s.lifecycle, renderer);
}
bool SelectRecipe(NativeSession &s, unsigned selected, unsigned count) {
    auto grid = At<void *>(s.layout, field::Grid);
    if (!grid || At<unsigned>(grid, field::GridItems) != count ||
        At<unsigned>(grid, field::GridState) != 2) return false;
    const auto columns = At<int>(grid, field::GridColumns);
    const auto visible = At<int>(grid, field::GridVisibleItems);
    if (columns < 1 || columns > 64 || visible < columns || visible > 512) return false;
    // Retain the viewport until the selection leaves it. The native reset
    // clamps the top row at the final partial page.
    const int selectedRow = selected / columns;
    const int visibleRows = std::max(1, visible / columns);
    const int firstRow = std::clamp(At<int>(grid, field::GridTopRow),
                                  std::max(0, selectedRow - visibleRows + 1), selectedRow);
    GridRequest request;
    request.vtable = Main + address::GridRequestVtable;
    request.count = count; request.first = firstRow * columns; request.selected = selected;
    request.resident = At<unsigned char>(s.phone, field::Resident);
    profile::ResetRecipeGrid.Resolve(Main)(grid, &request);
    profile::ActivateRecipeGrid.Resolve(Main)(grid, nullptr);
    // State-2 focus uses the native input context and cursor path. Its normal
    // enter delegate updates phone +218 and materials through FocusRecipe.
    profile::FocusRecipeGrid.Resolve(Main)(grid, false);
    auto row = profile::GridRowForIndex.Resolve(Main)(grid, selected);
    if (!RecipeRow(s, row)) return false;
    // Focus can already belong to this pooled row after a viewport rebuild.
    // Refresh the app details explicitly through the same native callback.
    struct { unsigned input{}, padding{}; void *source{}; bool instant{}; } event;
    event.input = s.inputRow; event.source = row;
    FocusRecipe(s.phone, &event);
    return At<unsigned>(s.phone, field::Selection) == selected;
}
preview::NavigationProgress NavigatePreview(NativeSession &s, int direction) {
    struct Recipes : game::RecipeNavigation {
        NativeSession &s;
        explicit Recipes(NativeSession &session) : s(session) {}
        bool RendererReady() override {
            auto context = UiContext();
            return PhoneReady(s.phone) && context && At<void *>(context, field::Controller) == s.controller &&
                At<void *>(context, field::Parameters) == s.parameters && s.ownsRenderer &&
                At<unsigned>(s.controller, field::ModelMode) == DiyModelMode &&
                At<int>(s.controller, field::ModelState) == 1 && At<unsigned>(s.controller, field::ModelHandle) &&
                !(At<unsigned char>(s.controller, field::ModelFlags) & MaterialPending);
        }
        unsigned Count() override { return At<unsigned>(s.phone, field::Recipes); }
        unsigned Selected() override { return At<unsigned>(s.phone, field::Selection); }
        void **Rows() const { return At<void **>(s.phone, field::Recipes + 8); }
        bool HasRecipeList() override { return Rows(); }
        bool HasValidItem(unsigned selected) override { return Rows()[selected] && At<std::uint16_t>(Rows()[selected], 8) < 0xfffe; }
        std::uint64_t CopyItem(unsigned selected) override { return At<std::uint64_t>(Rows()[selected], 8); }
        void RestoreBanner() override { recipe_banner::Restore(s.hotelBanner); }
        bool SelectRecipe(unsigned selected, unsigned count) override { return preview_internal::SelectRecipe(s, selected, count); }
        void SetItem(std::uint64_t item) override {
            s.item = item;
            s.fixedPose = UsesFixedPreviewPose(static_cast<std::uint16_t>(item));
        }
        void ReplaceModel(unsigned selected, unsigned count) override {
            // Preserve displayed scene/carrier ownership until native replacement
            // completes, avoiding a capture of the old mesh with reset angles.
            std::memcpy(static_cast<std::byte *>(s.controller) + field::ModelAngles,
                        s.initialAngles.data(), sizeof(s.initialAngles));
            ResetPoseOrigin(s.controller, s.item, s.fixedPose);
            At<std::uint64_t>(s.parameters, 0x2c) = s.item;
            At<unsigned>(s.parameters, 0x2140) = 1;
            struct { std::uint64_t item; std::uint16_t recipe{0xfffe}, padding{}; } request{s.item};
            // Native existing-model replacement, not CreateItemModel (which
            // silently refuses an already allocated model handle).
            profile::ChangeItemModel.Resolve(Main)(s.controller, &request);
            Visible(s.gridPane, false); Visible(s.scrollPane, false); Visible(s.cursorPane, false);
            Logging.Debug("[QoL Enhancements/Crafting] DIY preview: navigated; recipe_index=%u count=%u item=%u fixed=%u\n",
                          selected, count, unsigned(s.item & 0xffff), unsigned(s.fixedPose));
        }
    } recipes{s};
    return features::NavigateRecipePreview(s.lifecycle, recipes, direction);
}
bool PreviewInput(void *layout, void *snapshot, unsigned blocked) {
    for (auto &s : Sessions) if (s.layout == layout) {
        struct Input : game::PreviewInput {
            NativeSession &s; void *snapshot, *row{}; unsigned blocked, buttons{};
            Input(NativeSession &session, void *sample, unsigned suppression) : s(session), snapshot(sample), blocked(suppression) {}
            void RefreshInputRow() override { s.inputRow = ShortcutInputRow(s.layout); }
            bool ValidSnapshot() override {
                if (!snapshot || s.inputRow < 0) return false;
                const auto count = At<unsigned>(snapshot, 8);
                auto rows = At<std::byte *>(snapshot, 0x10);
                if (!rows || count > 8 || unsigned(s.inputRow) >= count) return false;
                row = rows + s.inputRow * 0x48;
                buttons = At<unsigned>(row);
                return true;
            }
            bool ConfirmHeld() override { return buttons & ConfirmMask; }
            bool BackHeld() override { return At<unsigned>(row) & BackMask; }
            bool ConfirmConsumed() override { return At<unsigned>(row, 0x3c) & ConfirmMask; }
            bool BackConsumed() override { return At<unsigned>(row, 0x3c) & BackMask; }
            bool PageBlocked() override { return InputBlocked(s, blocked); }
            bool DispatchBlocked() override { return blocked & 1; }
            void ReportConfirm() override {
                if (s.inputReports++ < 8)
                    Logging.Debug("[QoL Enhancements/Crafting] DIY preview: A input; state=%d pending=%d/%d focus=%p blocked=%u consumed=%x\n",
                                  At<int>(s.phone, field::State), At<int>(s.phone, field::Requests), At<int>(s.phone, field::Requests + 4),
                                  s.focusedRow, unsigned(InputBlocked(s, blocked)), At<unsigned>(row, 0x3c));
            }
            bool QueueSelectedPreview() override { return QueuePreview(s); }
            void ConsumeConfirm() override { At<unsigned>(row, 0x3c) |= ConfirmMask; }
            void ConsumeBack() override { At<unsigned>(row, 0x3c) |= BackMask; }
            unsigned UnconsumedButtons() const { return buttons & ~At<unsigned>(row, 0x3c); }
            bool LeftUnconsumed() override { return UnconsumedButtons() & LeftMask; }
            bool RightUnconsumed() override { return UnconsumedButtons() & RightMask; }
            bool LeftStickConsumed() override { return At<unsigned>(row, field::ConsumedAxes) & LeftStickMask; }
            bool RightStickConsumed() override { return At<unsigned>(row, field::ConsumedAxes) & RightStickMask; }
            game::Stick LeftStick() override { return {At<float>(row, field::LeftStick), At<float>(row, field::LeftStick + 4)}; }
            // Native axis kind 1 is the right stick. +40 bit 1 is consumption.
            game::Stick RightStick() override { return {At<float>(row, field::RightStick), At<float>(row, field::RightStick + 4)}; }
            void ConsumeNavigation() override {
                At<unsigned>(row, 0x3c) |= buttons & (LeftMask | RightMask);
                At<unsigned>(row, field::ConsumedAxes) |= LeftStickMask;
            }
            void ConsumeRotation() override { At<unsigned>(row, field::ConsumedAxes) |= RightStickMask; }
            void ReportRotationSample(float x, float y, bool inputBlocked) override {
                if ((x != 0 || y != 0) && s.stickReports++ < 4)
                    Logging.Debug("[QoL Enhancements/Crafting] DIY preview: right-stick snapshot; row=%d x=%08x y=%08x consumed=%x blocked=%u\n",
                                  s.inputRow, At<unsigned>(row, field::RightStick), At<unsigned>(row, field::RightStick + 4),
                                  At<unsigned>(row, field::ConsumedAxes), unsigned(inputBlocked));
            }
        } input{s, snapshot, blocked};
        return features::HandleRecipePreviewInput(s.lifecycle, s.confirmHeld, s.backHeld, input);
    }
    return false;
}
core::InputOutcome PreviewPageInput(const core::PageInputContext &input) {
    return PreviewInput(input.page, input.snapshot, input.blocked) ? core::InputOutcome::Consume : core::InputOutcome::PassThrough;
}
bool Begin(NativeSession &s) {
    if (!PhoneReady(s.phone)) return false;
    if (s.gridPane) s.gridFlags = At<unsigned char>(s.gridPane, field::PaneFlags);
    if (s.scrollPane) s.scrollFlags = At<unsigned char>(s.scrollPane, field::PaneFlags);
    if (s.cursorPane) s.cursorFlags = At<unsigned char>(s.cursorPane, field::PaneFlags);
    auto context = UiContext();
    auto controller = context ? At<void *>(context, field::Controller) : nullptr;
    auto parameters = context ? At<void *>(context, field::Parameters) : nullptr;
    if (!controller || !parameters) {
        Logging.Debug("[QoL Enhancements/Crafting] DIY preview: missing renderer; context=%p controller=%p parameters=%p\n",
                    context, controller, parameters);
        return false;
    }
    const auto mode = At<int>(controller, field::ModelMode);
    const auto state = At<int>(controller, field::ModelState);
    const bool available = (mode == -1 && state == 0) || (mode == PhoneModelMode && state == PhoneModelState);
    if (!available || At<unsigned>(controller, field::ModelHandle) ||
        At<void *>(controller, field::BackgroundPage) || At<void *>(controller, field::VisibilityPage)) {
        Logging.Debug("[QoL Enhancements/Crafting] DIY preview: renderer refused; mode=%d state=%d handle=%u background=%p visibility=%p\n",
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
    At<unsigned>(controller, field::ModelMode) = DiyModelMode;
    At<void *>(controller, field::BackgroundPage) = nullptr; // No background page owner.
    At<unsigned char>(controller, 0x68) = 0;
    s.ownsRenderer = true;
    TransformCalls.store(0, std::memory_order_relaxed);
    TransformAttempts.store(0, std::memory_order_relaxed);
    TransformApplied.store(0, std::memory_order_relaxed);
    ClearCalls.store(0, std::memory_order_relaxed);
    ClearMatches.store(0, std::memory_order_relaxed);
    MatrixCalls.store(0, std::memory_order_relaxed);
    MatrixMatches.store(0, std::memory_order_relaxed);
    BackgroundApplied.store(0, std::memory_order_relaxed);
    s.fixedPose = UsesFixedPreviewPose(static_cast<std::uint16_t>(s.item));
    PreviewScenePose.store(false, std::memory_order_release);
    PreviewFixedPose.store(s.fixedPose, std::memory_order_release);
    {
        PublicationClock::Write publish{PosePublication};
        PreviewRequestedFixedPose.store(s.fixedPose);
    }
    PreviewController.store(controller, std::memory_order_release);
    Logging.Debug("[QoL Enhancements/Crafting] DIY preview: native fixed pose=%u item=%u\n",
                unsigned(s.fixedPose), unsigned(s.item & 0xffff));
    const auto color = ItemLayerClearColor();
    PreviewClearColor.store(color, std::memory_order_release);
    Logging.Debug("[QoL Enhancements/Crafting] DIY preview: background #E5DFC7; layer_color=%p\n", color);
    profile::ChangeState.Resolve(Main)(static_cast<std::byte *>(controller) + 8, 1);

    Logging.Debug("[QoL Enhancements/Crafting] DIY preview: renderer acquired; previous_mode=%d previous_state=%d\n", mode, state);
    return true;
}
preview::ModelProgress CreatePreviewModel(NativeSession &s) {
    auto context = UiContext();
    if (!s.ownsRenderer || !context || At<void *>(context, field::Controller) != s.controller ||
        At<void *>(context, field::Parameters) != s.parameters ||
        At<unsigned>(s.controller, field::ModelMode) != DiyModelMode) return preview::ModelProgress::Failed;
    if (At<int>(s.controller, 0x10) != 1) return preview::ModelProgress::Waiting;
    ItemRequest request;
    request.itemLow = s.item; request.itemHigh = s.item >> 32;
    profile::CreateItemModel.Resolve(Main)(s.controller, &request);
    if (!At<unsigned>(s.controller, field::ModelHandle)) return preview::ModelProgress::Failed;
    std::memcpy(s.initialAngles.data(), static_cast<std::byte *>(s.controller) + field::ModelAngles, sizeof(s.initialAngles));
    ResetPoseOrigin(s.controller, s.item, s.fixedPose);
    return preview::ModelProgress::Created;
}
struct PhoneRuntime : game::PreviewRuntime {
    NativeSession &s;
    explicit PhoneRuntime(NativeSession &session) : s(session) {}
    void RefreshHints() override { RefreshRecipeHints(s); }
    bool AcquireRenderer() override { return preview_internal::Begin(s); }
    void OpenDeclined() override {
        s.refreshFooter = true;
        Logging.Debug("[QoL Enhancements/Crafting] DIY preview: renderer busy/unavailable; grid retained\n");
    }
    void ReleaseRenderer() override { preview_internal::Close(s); }
    preview::ModelProgress CreateItemModel() override { return CreatePreviewModel(s); }
    void ShowPreview() override {
        Visible(s.gridPane, false); Visible(s.scrollPane, false); Visible(s.cursorPane, false);
        Visible(s.capture, true); Visible(s.surface, true);
        if (recipe_banner::Place(s.hotelBanner, s.surface))
            Logging.Debug("[QoL Enhancements/Crafting] DIY preview: Hotel banner over preview bottom-right\n");
        Logging.Debug("[QoL Enhancements/Crafting] DIY preview: item model created; handle=%u\n", At<unsigned>(s.controller, field::ModelHandle));
    }
    preview::NavigationProgress Navigate(int direction) override { return NavigatePreview(s, direction); }
    void PlaceBanner() override { recipe_banner::Place(s.hotelBanner, s.surface); }
    void Rotate() override { RotatePreview(s); }
    void ReportShowing() override {
        ReportRendererTrace();
        Logging.Debug("[QoL Enhancements/Crafting] DIY preview: renderer status; handle=%u flags=%u state=%d\n",
                      At<unsigned>(s.controller, field::ModelHandle), At<unsigned char>(s.controller, 0x74), At<int>(s.controller, 0x10));
    }
};
void UpdatePhoneRecipes::Callback(void *phone) {
    if (auto s = ForPhone(phone)) recipe_banner::Restore(s->hotelBanner);
    Orig(phone);
    auto s = ForPhone(phone);
    if (!s) return;
    PhoneRuntime runtime{*s};
    s->lifecycle.Frame(runtime);
}
void DeactivatePhoneRecipes::Callback(void *phone){
        if (auto s = ForPhone(phone)) {
            PhoneRuntime runtime{*s};
            features::DeactivateRecipePreview(s->lifecycle, s->confirmHeld, s->backHeld, runtime);
        }
        Orig(phone);
    }

bool InstallPhonePreview() {
    qol::core::HookPlan hookPlan{
        qol::core::Site<BuildRecipeLayout>("BuildRecipeLayout", address::BuildLayout, profile::HookWords(address::BuildLayout), true),
        qol::core::Site<BuildPhoneRecipeHints>("BuildPhoneRecipeHints", recipe_hint::BuildFooter, profile::HookWords(recipe_hint::BuildFooter), true),
        qol::core::Site<BindPhoneRecipes>("BindPhoneRecipes", address::PhoneBind, profile::HookWords(address::PhoneBind), true),
        qol::core::Site<UpdatePhoneRecipes>("UpdatePhoneRecipes", address::PhoneFrame, profile::HookWords(address::PhoneFrame), true),
        qol::core::Site<DeactivatePhoneRecipes>("DeactivatePhoneRecipes", address::PhoneDeactivate, profile::HookWords(address::PhoneDeactivate), true),
        qol::core::Site<DrawPreviewLayer>("DrawPreviewLayer", address::DrawModelLayer, profile::HookWords(address::DrawModelLayer), true),
        qol::core::Site<SetPreviewRotation>("SetPreviewRotation", address::SetModelRotation, profile::HookWords(address::SetModelRotation), true),
        qol::core::Site<TraceModelTransform>("TraceModelTransform", address::UpdateModelTransform, profile::HookWords(address::UpdateModelTransform), true),
        qol::core::Site<UpdatePreviewActor>("UpdatePreviewActor", address::UpdateItemActor, profile::HookWords(address::UpdateItemActor), true),
        qol::core::Site<BuildPreviewMatrix>("BuildPreviewMatrix", address::BuildModelMatrix, profile::HookWords(address::BuildModelMatrix), true),
        qol::core::Site<DrawPreviewContents>("DrawPreviewContents", address::DrawLayerContents, profile::HookWords(address::DrawLayerContents), true),
    };
    if (!hookPlan) {
        Logging.Error("[QoL Enhancements/Crafting] DIY preview: DISABLED; hook preparation rejected\n");
        return false;
    }

    const auto &main = exl::util::GetMainModuleInfo();
    const auto executable = native::MainExecutable();
    for (const auto &f : profile::DiyChecks) if (const auto result = executable.Check(f); !hookPlan.Contains(f.offset) && !result) {
        native::ReportMismatch(result);
        Logging.Error("[QoL Enhancements/Crafting] DIY preview: DISABLED; instruction mismatch at main+0x%lx\n", f.offset); return false;
    }
    if (!game::preview_native::GlobalsAvailable()) return false;
    Main = main.m_Text.m_Start;
    const auto modeSlot = Main + address::PhoneRendererModeSlot;
    if (!native::MainDataContains(address::PhoneRendererModeSlot, sizeof(std::uintptr_t)) ||
        *reinterpret_cast<const std::uintptr_t *>(modeSlot) != Main + address::PhoneRendererMode) {
        Logging.Error("[QoL Enhancements/Crafting] DIY preview: DISABLED; renderer mode slot mismatch\n");
        return false;
    }
    // Pockets may already own the shared trampoline. Its manager validates
    // pristine instructions on first registration and reuses that owned hook;
    // checking the original bytes again here would reject our own Pockets patch.
    if (!core::RegisterPageInput<PreviewPageInput>(core::InputPriority::Preview)) {
        Logging.Error("[QoL Enhancements/Crafting] DIY preview: DISABLED; shared page input unavailable\n");
        return false;
    }
    auto slot = reinterpret_cast<volatile std::uintptr_t *>(modeSlot);
    const auto replacement = reinterpret_cast<std::uintptr_t>(&PhoneRendererMode);
    qol::core::RecordMutation();
    *slot = replacement;
    if (*slot != replacement) {
        qol::core::ReportActivationFailure(qol::core::FeatureState::Failed, "renderer mode slot write failed");
        Logging.Error("[QoL Enhancements/Crafting] DIY preview: DISABLED; renderer mode slot write failed\n");
        return false;
    }
    if (!hookPlan.Publish<BuildRecipeLayout>(address::BuildLayout)) return false;
    if (!hookPlan.Publish<BuildPhoneRecipeHints>(recipe_hint::BuildFooter)) return false;
    if (!hookPlan.Publish<BindPhoneRecipes>(address::PhoneBind)) return false;
    if (!hookPlan.Publish<UpdatePhoneRecipes>(address::PhoneFrame)) return false;
    if (!hookPlan.Publish<DeactivatePhoneRecipes>(address::PhoneDeactivate)) return false;
    if (!hookPlan.Publish<DrawPreviewLayer>(address::DrawModelLayer)) return false;
    if (!hookPlan.Publish<SetPreviewRotation>(address::SetModelRotation)) return false;
    if (!hookPlan.Publish<TraceModelTransform>(address::UpdateModelTransform)) return false;
    if (!hookPlan.Publish<UpdatePreviewActor>(address::UpdateItemActor)) return false;
    if (!hookPlan.Publish<BuildPreviewMatrix>(address::BuildModelMatrix)) return false;
    if (!hookPlan.Publish<DrawPreviewContents>(address::DrawLayerContents)) return false;
    Logging.Debug("[QoL Enhancements/Crafting] DIY preview: READY; native starting poses; primary scene rotation; horizontal recipe navigation; A opens, B returns\n");
    return true;
}
bool InstallWorkbenchTilt() {
    qol::core::HookPlan hookPlan{
        qol::core::Site<UpdatePreviewActor>("UpdatePreviewActor", address::UpdateItemActor, profile::HookWords(address::UpdateItemActor), true),
    };
    if (!hookPlan) {
        Logging.Error("[QoL Enhancements/Crafting] workbench tilt: DISABLED; hook preparation rejected\n");
        return false;
    }

    const auto &main = exl::util::GetMainModuleInfo();
    const auto executable = native::MainExecutable();
    for (const auto &f : profile::WorkbenchChecks) {
        // The plan verified our actor hook's identity and installed bytes.
        if (hookPlan.Contains(f.offset)) continue;
        if (f.offset == address::BuildModelMatrix && qol::core::OwnedHooks.Contains(f.offset)) {
            if (qol::core::OwnedHooks.Owned(qol::core::Site<BuildPreviewMatrix>("BuildPreviewMatrix", f.offset, f.words), executable)) continue;
            qol::core::ReportActivationFailure(qol::core::FeatureState::Unsupported, "shared preview matrix hook changed");
            Logging.Error("[QoL Enhancements/Crafting] workbench tilt: DISABLED; shared matrix ownership mismatch\n");
            return false;
        }
        if (const auto result = executable.Check(f); !result) {
            native::ReportMismatch(result);
            Logging.Error("[QoL Enhancements/Crafting] workbench tilt: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
            return false;
        }
    }
    if (!game::preview_native::GlobalsAvailable()) return false;
    Main = main.m_Text.m_Start;
    if (!core::RegisterPageInput<WorkbenchPageInput>(core::InputPriority::Workbench)) return false;
    WorkbenchTiltEnabled.store(true, std::memory_order_release);
    if (!hookPlan.Publish<UpdatePreviewActor>(address::UpdateItemActor)) return false;
    Logging.Debug("[QoL Enhancements/Crafting] workbench tilt: READY; native-selected item profile; input binding probes; right-stick up/down\n");
    return true;
}

} // namespace qol::crafting::preview_internal
