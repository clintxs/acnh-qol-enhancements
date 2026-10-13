#include "program/core/hook_plan.hpp"
#include "hold_select.hpp"
#include "program/pockets/settings.hpp"
#include "program/pockets/game/pocket_input.hpp"
#include "program/core/page_input.hpp"
#include "program/pockets/game/stack_bindings.hpp"
#include <array>
#include <cstring>

namespace qol_pockets::features {
namespace {
namespace address {
constexpr std::uintptr_t PocketPageVtable = 0x04a06b00;
constexpr std::uintptr_t ApplicationGlobal = 0x059f83f0;
constexpr std::uintptr_t ButtonFeedback = 0x03c66950;
constexpr std::uintptr_t ItemFeedbackValidity = 0x02fbfe5c;
}
namespace layout {
constexpr std::size_t Page = 0x210, Mode = 0x1c4fc, State = 0x1b0;
constexpr std::size_t TransitionA = 0x1f0, TransitionB = 0x1f4;
constexpr std::size_t SelectionLimit = 0x1c4f8, SelectedSlots = 0x1c4e8;
constexpr std::size_t BlockedSlots = 0x1c4f0, ClickSlot = 0x1c518, LogicalInput = 0x1c51a;
constexpr std::size_t BackHint = 0x570, HintInput = 0xa0, ButtonId = 0x14, InputRow = 0x10;
constexpr std::size_t RowCount = 8, Rows = 0x10, RowBytes = 0x48, Consumed = 0x3c;
constexpr std::size_t InputContext = 0x210, InputBlocked = 0x54, InputMask = 0x58;
constexpr std::size_t GlobalInputMasks = 0x418, GlobalInputMask = 0x10;
constexpr std::size_t ItemWidgets = 0x590, ItemFlags = 0x2c3;
constexpr std::size_t FeedbackValidity = 0x2a0, FeedbackRow = 0x2a8;
constexpr std::size_t ParameterValue = 0x40, DecideContext = 0x298;
}
constexpr unsigned NativeA = 1u << 4;
constexpr unsigned OtherActionButtons = (1u << 5) | (1u << 6) | (1u << 7) | // B/X/Y
                                        (1u << 8) | (1u << 9) |          // L/R
                                        (1u << 10) | (1u << 11);         // ZL/ZR
// UI rows use the footer's button IDs, not HID bit positions. Live input also
// carries bit 16 alongside ordinary A/navigation; it is not a trigger action.
constexpr std::uint16_t SelectItem = 1u << 2;
constexpr int Browsing = 3, ItemSelectMode = 1, PutInStorageMode = 9;
template <typename T> T &Field(void *object, std::size_t offset) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(object) + offset);
}
struct HoldGesture {
    void *page{}, *owner{};
    int row{-1}, mode{};
    bool released{}, holding{}, selecting{};
};
HoldGesture gesture{};
struct InputCapture {
    void *owner{};
    int row{}, previousSlot{};
    bool starting{};
};
InputCapture capture{};
struct SafeString { std::uintptr_t vtable; const char *text; };
struct DeferredFeedback { void *widget{}, *context{}; std::uintptr_t stringVtable{}; };
DeferredFeedback deferred{};

HOOK_DEFINE_TRAMPOLINE(HoldSelectFeedback) {
    static void Callback(void *widget, const SafeString *event, void *context) {
        // A real click can follow navigation in the same input pass. Preserve
        // its original Select -> Decide order rather than replaying movement
        // after the confirm sound, and leave no synthetic feedback pending.
        if (deferred.widget == widget && event && event->text &&
            std::strcmp(event->text, "Decide") == 0) {
            const SafeString previous{deferred.stringVtable, "Select"};
            Orig(widget, &previous, deferred.context);
            deferred = {};
        }
        // Select is the UI focus/movement event, not the A-button Decide event.
        // Defer only pocket item focus during this observer's native input pass.
        // Never retain the caller's temporary SafeString or touch other screens.
        if (capture.owner && event && event->text && std::strcmp(event->text, "Select") == 0) {
            auto *page = Field<void *>(capture.owner, layout::Page);
            for (int slot = 0; page && slot < 40; ++slot) {
                if (widget != Field<void *>(page, layout::ItemWidgets + sizeof(void *) * slot)) continue;
                if (deferred.widget) {
                    const SafeString previous{deferred.stringVtable, "Select"};
                    Orig(deferred.widget, &previous, deferred.context);
                }
                deferred = {widget, context, event->vtable};
                return;
            }
        }
        Orig(widget, event, context);
    }
};

void FlushHoldFeedback(void *selectedWidget, int row) {
    const auto pending = deferred;
    deferred = {};
    // No observed movement event means there is nothing to replace. In
    // particular, native A clicks already produced their own Decide feedback.
    if (!pending.widget) return;
    const bool selecting = selectedWidget == pending.widget;
    if (selecting) {
        if (auto *parameter = Field<void *>(selectedWidget, layout::FeedbackValidity))
            Field<int>(parameter, layout::ParameterValue) =
                game::FunctionAt<int (*)(void *)>(address::ItemFeedbackValidity)(selectedWidget);
        if (auto *parameter = Field<void *>(selectedWidget, layout::FeedbackRow))
            Field<int>(parameter, layout::ParameterValue) = row;
    }
    const SafeString event{pending.stringVtable, selecting ? "Decide" : "Select"};
    HoldSelectFeedback::Orig(pending.widget, &event,
        selecting ? Field<void *>(selectedWidget, layout::DecideContext) : pending.context);
}

bool IsMultiSelection(void *owner) {
    const int mode = Field<int>(owner, layout::Mode);
    const int limit = Field<int>(owner, layout::SelectionLimit);
    // 00d0fd10 -> 02d1e950 -> 033598a0: cItemSelect is mode 1 and
    // cPutInStorage is mode 9. Do not enable ordinary pockets or single picks.
    return (mode == ItemSelectMode || mode == PutInStorageMode) &&
           (limit == -1 || limit > 1) && Field<int>(owner, layout::State) == Browsing &&
           Field<int>(owner, layout::TransitionA) == -1 &&
           Field<int>(owner, layout::TransitionB) == -1;
}

void BeforeHoldSelectInput(void *page, void *snapshot, unsigned blocked) {
    capture = {};
    if (!page || !snapshot ||
        Field<std::uintptr_t>(page, 0) != game::MainAddress(address::PocketPageVtable)) {
        // Other pages also receive input in this frame. Their dispatch must
        // not interrupt a pockets gesture; Play entry handles reopening.
        return;
    }
    auto *owner = game::pocket_input::FindPocketController(page);
    auto *hint = Field<void *>(page, layout::BackHint);
    auto *unit = hint ? Field<void *>(hint, layout::HintInput) : nullptr;
    if (!owner || !IsMultiSelection(owner) || !unit || Field<int>(unit, layout::ButtonId) != 5) {
        gesture = {};
        return;
    }
    const int row = Field<int>(unit, layout::InputRow);
    const int count = Field<int>(snapshot, layout::RowCount);
    auto *rows = Field<std::byte *>(snapshot, layout::Rows);
    if (!rows || count < 1 || count > 8 || row < 0 || row >= count) {
        gesture = {};
        return;
    }
    const int mode = Field<int>(owner, layout::Mode);
    if (gesture.page != page || gesture.owner != owner || gesture.row != row || gesture.mode != mode)
        gesture = {page, owner, row, mode, false, false};
    auto *input = rows + row * layout::RowBytes;
    const unsigned buttons = Field<unsigned>(input, 0);
    auto *context = Field<void *>(page, layout::InputContext);
    auto *app = *reinterpret_cast<void **>(game::MainAddress(address::ApplicationGlobal));
    auto *masks = app ? Field<void *>(app, layout::GlobalInputMasks) : nullptr;
    if (!context || !masks || (blocked & 1) || Field<std::uint8_t>(context, layout::InputBlocked) ||
        (Field<unsigned>(context, layout::InputMask) & Field<unsigned>(masks, layout::GlobalInputMask)) ||
        (Field<unsigned>(input, layout::Consumed) & NativeA) || (buttons & OtherActionButtons)) {
        gesture.released = gesture.holding = false;
        return;
    }
    if (!(buttons & NativeA)) {
        gesture.released = true;
        gesture.holding = false;
        return;
    }
    // An A press carried into a new/blocked picker must be released first.
    const int slot = game::pocket_input::FindFocusedItemSlot(page, row);
    if (!gesture.released || slot < 0) return;
    const bool starting = !gesture.holding;
    if (starting) {
        // Read before the native A toggle, then retain this target until release.
        gesture.selecting = !(Field<std::uint64_t>(owner, layout::SelectedSlots) &
                              (std::uint64_t{1} << slot));
    }
    gesture.holding = true;
    capture = {owner, row, slot, starting};
}

void *QueueHeldSelection(void *page, const InputCapture &input) {
    if (!input.owner || !gesture.holding) return nullptr;
    auto *owner = game::pocket_input::FindPocketController(page);
    if (owner != input.owner || !IsMultiSelection(owner) ||
        Field<int>(owner, layout::Mode) != gesture.mode) {
        gesture = {};
        return nullptr;
    }
    if (input.starting && (Field<std::uint16_t>(owner, layout::LogicalInput) & SelectItem)) {
        // Navigation and the first A press can share a frame. Native input
        // identifies which item actually receives that press, before toggling.
        const int clickedSlot = Field<std::int8_t>(owner, layout::ClickSlot);
        if (clickedSlot >= 0 && clickedSlot < 40)
            gesture.selecting = !(Field<std::uint64_t>(owner, layout::SelectedSlots) &
                                  (std::uint64_t{1} << clickedSlot));
    }
    const int slot = game::pocket_input::FindFocusedItemSlot(page, input.row);
    if (slot < 0 || slot == input.previousSlot) return nullptr;
    const auto bit = std::uint64_t{1} << slot;
    const bool selected = (Field<std::uint64_t>(owner, layout::SelectedSlots) & bit) != 0;
    if (selected == gesture.selecting ||
        (Field<std::uint64_t>(owner, layout::BlockedSlots) & bit) ||
        Field<std::uint16_t>(owner, layout::LogicalInput) != 0) return nullptr;
    auto *widget = Field<void *>(page, layout::ItemWidgets + sizeof(void *) * slot);
    if (!widget || !(Field<std::uint8_t>(widget, layout::ItemFlags) & 1)) return nullptr;
    // Mirror native click callback 030884b0. The browsing update 0308b3f0
    // performs the toggle, selection-limit checks, storage-capacity checks and
    // feedback. Only toggle items that differ from this hold's target state,
    // and never overwrite a real A request.
    Field<std::int8_t>(owner, layout::ClickSlot) = slot;
    Field<std::uint16_t>(owner, layout::LogicalInput) |= SelectItem;
    return widget;
}

void AfterHoldSelectInput(void *page, void *, unsigned) {
    const auto input = capture;
    capture = {};
    FlushHoldFeedback(QueueHeldSelection(page, input), input.row);
}
}

void ResetHoldSelect(void *owner) {
    if (gesture.owner == owner) gesture = {};
    if (capture.owner == owner) capture = {};
}

bool InstallHoldSelect() {
    // Skip both the input observer and feedback hook when native selection is requested.
    if (!GetSettings().enableMultiselect) return true;
    // Besides the shared input hook, verify the native request producer and
    // consumer whose field semantics this feature relies on.
    struct Check { std::uintptr_t offset; std::array<std::uint32_t, 5> words; };
    constexpr std::array checks{
        Check{0x030884b0, {0xd100c3ff, 0xa9017bfd, 0xa9024ff4, 0x910043fd, 0xf941080e}},
        Check{0x0308b3f0, {0xd10283ff, 0xfd001be8, 0xa9047bfd, 0xa9056ffc, 0xa90667fa}},
        Check{address::ButtonFeedback, {0xd102c3ff, 0xa9077bfd, 0xf90043f7, 0xa90957f6, 0xa90a4ff4}},
        Check{address::ItemFeedbackValidity, {0x394b0c09, 0x7200013f, 0x394c5408, 0x7a401900, 0x1a9f07e0}}};
    for (const auto &check : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(check.offset)),
                        check.words.data(), sizeof(check.words)) != 0) {
            Logging.Error("[QoL Enhancements/Pockets] HOLD_SELECT_DISABLED fingerprint at %lx\n", check.offset);
            return false;
        }
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<HoldSelectFeedback>("HoldSelectFeedback", address::ButtonFeedback, {0xd102c3ff, 0xa9077bfd, 0xf90043f7, 0xa90957f6, 0xa90a4ff4}, true),
    };
    if (!hookPlan) return false;
    if (!qol::core::RegisterPageInputObserver(BeforeHoldSelectInput, AfterHoldSelectInput)) {
        Logging.Error("[QoL Enhancements/Pockets] HOLD_SELECT_DISABLED shared input unavailable\n");
        return false;
    }
    if (!hookPlan.Publish<HoldSelectFeedback>(address::ButtonFeedback)) return false;
    Logging.Debug("[QoL Enhancements/Pockets] HOLD_SELECT_READY select/deselect with Decide feedback; selling / put in storage\n");
    return true;
}
}
