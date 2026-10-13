#include "pocket_commands_native.hpp"
#include "quick_store_native.hpp"
#include "pocket_input.hpp"
#include "program/pockets/settings.hpp"
#include <cstring>

namespace qol_pockets::game::pocket_commands_native {
namespace {
namespace profile = v303::quick_store;
namespace address = profile::address;
namespace layout = profile::layout;
namespace cursor_layout = profile::cursor_layout;
using quick_store_native::Field;
using quick_store_native::MenuDescription;
using MenuRowId = profile::MenuRowId;
namespace state {
constexpr int Browsing = 3, Command = 7, Refresh = 14, NoQueuedTransition = -1;
constexpr int SelectingCommand = 5;
}
constexpr int PocketSlotLimit = 40, InputRowLimit = 8, NativeBackButtonId = 5;
constexpr unsigned OtherActionButtons = 0x37f;
// Even a sparse declaration must reject Y/trigger chords. Unregistered known
// shortcut buttons are conflicting input, not permission to choose an action.
constexpr unsigned AllShortcutButtons = unsigned(commands::UiButton::Y) |
    unsigned(commands::UiButton::ZL) | unsigned(commands::UiButton::ZR);
void *lastPage{};
int lastRow = -1;
unsigned previousButtons{};
bool hasPreviousInput = false;
namespace open_action {
constexpr int Present = int(MenuRowId::Present), Treasure = int(MenuRowId::Treasure), Bottle = int(MenuRowId::Bottle);
}
const char *eligibilityReason = "not-checked";
struct ItemFeedback {
    void *widget{};
    int validity{}, inputRow{};
};
ItemFeedback CaptureItemFeedback(void *pocket, int slot, int inputRow) {
    auto *page = Field<void *>(pocket, layout::PocketPage);
    auto *widget = page && slot >= 0 && slot < PocketSlotLimit
                       ? Field<void *>(page, layout::ItemWidgets + sizeof(void *) * slot)
                       : nullptr;
    if (!widget) {
        return {};
    }
    return {widget, game::quick_store_native::ItemFeedbackValidity(widget),
            inputRow};
}
void PlayItemFeedback(const ItemFeedback &feedback) {
    auto *widget = feedback.widget;
    if (!widget) {
        return;
    }
    // Native confirm (03c831c0) sets two feedback parameters BEFORE Decide:
    // widget virtual+148 validity and the input row. 0.5.3 omitted these and
    // could play the invalid/empty-slot cue. Preserve pre-removal validity;
    // only publish/queue feedback after success, without clicking the item.
    if (auto *parameter = Field<void *>(widget, layout::FeedbackValidityParameter)) {
        Field<int>(parameter, layout::FeedbackParameterValue) = feedback.validity;
    }
    if (auto *parameter = Field<void *>(widget, layout::FeedbackRowParameter)) {
        Field<int>(parameter, layout::FeedbackParameterValue) = feedback.inputRow;
    }
    const game::quick_store_native::SafeString event{*reinterpret_cast<const std::uintptr_t *>(
                               game::MainAddress(game::stack_offsets::SafeStringVtableGot)) +
                               16,
                           "Decide"};
    game::quick_store_native::ButtonFeedback(
        widget, &event, Field<void *>(widget, layout::FeedbackContext));
}

void PlayCursorTapNow(void *pocket, int slot, int inputRow) {
    if (slot < 0 || slot >= PocketSlotLimit || inputRow != 0) {
        return;
    }
    auto *page = Field<void *>(pocket, layout::PocketPage);
    auto *widget = page ? Field<void *>(page, layout::ItemWidgets + sizeof(void *) * slot) : nullptr;
    if (!widget) {
        return;
    }
    // SelectCursorSequence is UI application slot 18 (+d0), not the input
    // application's global. Only animate the cursor still on this item pane.
    auto **global = *reinterpret_cast<void ***>(game::MainAddress(address::UiApplicationGlobalGot));
    auto *application = global ? *global : nullptr;
    auto *cursor = application ? Field<void *>(application, cursor_layout::ApplicationCursor) : nullptr;
    if (!cursor || Field<std::uintptr_t>(cursor, 0) != game::MainAddress(address::CursorVtable) ||
        !Field<void *>(widget, layout::ItemPane) ||
        Field<void *>(cursor, cursor_layout::FocusedPane) != Field<void *>(widget, layout::ItemPane) ||
        Field<std::uint8_t>(cursor, cursor_layout::HideRequested)) {
        return;
    }
    auto *cursorLayout = Field<void *>(cursor, cursor_layout::OverrideLayout);
    if (!cursorLayout) {
        auto *cursorPage = Field<void *>(cursor, cursor_layout::Page);
        cursorLayout = cursorPage ? Field<void *>(cursorPage, cursor_layout::PageDefaultLayout) : nullptr;
    }
    auto *animation = cursorLayout ? Field<void *>(cursorLayout, cursor_layout::ClickAnimation) : nullptr;
    if (!animation || !Field<void *>(animation, 0) ||
        !Field<void *>(animation, cursor_layout::AnimationTarget) ||
        !Field<void *>(animation, cursor_layout::AnimationImplementation)) {
        return;
    }
    // Exactly the animation-only portion of native cursor event 5,
    // 031007dc..03100834. Do not broadcast a widget click: that also selects
    // the item/opens its menu. Native playback takes speed 1.0 in s0.
    struct AnimationOperation {
        std::uintptr_t vtable;
        int operation;
        int padding;
    };
    const AnimationOperation play{
        *reinterpret_cast<const std::uintptr_t *>(game::MainAddress(address::AnimationOperationVtableGot)) + 16,
        1, 0};
    auto *interface = Field<void *>(animation, 0);
    auto prepare = Field<void (*)(void *, const AnimationOperation *)>(interface, cursor_layout::PrepareOperationVirtual);
    prepare(animation, &play);
    auto *implementation = Field<void *>(animation, cursor_layout::AnimationImplementation);
    auto start = Field<void (*)(void *, float)>(implementation, cursor_layout::PlayVirtual);
    start(Field<void *>(animation, cursor_layout::AnimationTarget), 1.0f);
}

bool HasNoQueuedTransition(void *controllerObject) {
    return Field<int>(controllerObject, layout::TransitionA) == state::NoQueuedTransition &&
           Field<int>(controllerObject, layout::TransitionB) == state::NoQueuedTransition;
}
void QueueControllerState(void *controllerObject, int state) {
    Field<int>(controllerObject,
               layout::TransitionA +
                   sizeof(int) * Field<std::uint8_t>(controllerObject, layout::TransitionSlot)) =
        state;
}

// Build exactly the game's menu for the selected slot. The temporary selection
// is restored before returning; no item is moved by this eligibility probe.
bool FindNativeShortcut(void *pocket, int slot, game::ChooseMenuEntry choose, int &action, bool trace) {
    // Ask the full native menu builder, including item/unlock restrictions.
    // Its error field may be nonzero even for valid menus; inspect actual rows.
    MenuDescription description{};
    description.vtable = game::MainAddress(address::MenuDescriptionVtable);
    description.bankVtable = *reinterpret_cast<const std::uintptr_t *>(
                                 game::MainAddress(game::stack_offsets::SafeStringVtableGot)) +
                             16;
    description.bank = "CmnSubWin";
    for (auto &callback : description.callbacks) {
        callback.vtable = game::MainAddress(address::EmptyCallbackVtable);
    }
    auto &selected = Field<std::int8_t>(pocket, layout::SelectedSlot);
    const auto previous = selected;
    selected = slot;
    game::quick_store_native::BuildMenu(pocket, &description);
    selected = previous;
    if (trace) {
        Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_TRACE built rows=%d error=%d\n",
                    description.count, description.error);
    }
    eligibilityReason = "menu-row-count";
    if (description.count < 1 || description.count > 7) {
        return false;
    }
    eligibilityReason = "no-unflagged-native-shortcut-row";
    if (!choose) return false;
    std::array<game::MenuEntry, 7> entries{};
    for (int i = 0; i < description.count; ++i) {
        const auto &row = description.rows[i];
        if (trace) {
            Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_TRACE row=%d action=%d flags=%x "
                        "extra=%x callback_vt=%lx fields=%lx,%lx,%lx\n",
                        i, row.action, row.flags, row.extra,
                        description.callbacks[i].vtable - game::MainAddress(0),
                        description.callbacks[i].fields[0], description.callbacks[i].fields[1],
                        description.callbacks[i].fields[2]);
        }
        auto meaning = game::ItemAction::Unknown;
        switch (static_cast<MenuRowId>(row.action)) {
        case MenuRowId::PutInStorage: meaning = game::ItemAction::Store; break;
        case MenuRowId::DropItem: meaning = game::ItemAction::Drop; break;
        case MenuRowId::Release: meaning = game::ItemAction::Release; break;
        case MenuRowId::Hold: meaning = game::ItemAction::Hold; break;
        case MenuRowId::Wear: meaning = game::ItemAction::Wear; break;
        case MenuRowId::PutAway: meaning = game::ItemAction::PutAway; break;
        case MenuRowId::Eat: meaning = game::ItemAction::Eat; break;
        case MenuRowId::EatOne: meaning = game::ItemAction::EatOne; break;
        case MenuRowId::Drink: meaning = game::ItemAction::Drink; break;
        default: break;
        }
        const bool open = row.bank && row.key && std::strcmp(row.bank, "CmnSubWin") == 0 &&
            ((row.action == open_action::Present && std::strcmp(row.key, "0113") == 0) ||
             (row.action == open_action::Treasure && std::strcmp(row.key, "0144") == 0) ||
             (row.action == open_action::Bottle && std::strcmp(row.key, "0148") == 0));
        if (open) meaning = game::ItemAction::Open;
        const auto &callback = description.callbacks[i];
        entries[i] = {meaning, meaning != game::ItemAction::Unknown && row.flags == 0 && row.extra == 0 &&
            callback.vtable == game::MainAddress(address::EmptyCallbackVtable) &&
            callback.fields[0] == 0 && callback.fields[1] == 0 && callback.fields[2] == 0};
    }
    const auto selectedEntry = choose({entries.data(), static_cast<std::size_t>(description.count)});
    if (!selectedEntry || *selectedEntry >= static_cast<std::size_t>(description.count) ||
        !entries[*selectedEntry].executable) return false;
    action = description.rows[*selectedEntry].action;
    eligibilityReason = "eligible";
    return true;
}
bool CanUseShortcut(void *pocket, int slot, int state, Action requested, int &action,
                    ItemIdentity &itemIdentity, game::ChooseMenuEntry choose, bool trace = false, bool enteringMenu = false) {
    if (requested == Action::OpenContainer && !GetSettings().bypassMenuOpenContainers) return false;
    // Callers allow browsing (3), first-level A menu (5), or dispatch (7).
    // Drag states 8/9 and second-level menu 6 are never automatic actions.
    // +1c516 is the equipped-item slot, not a drag flag. Leave it to the game.
    // +1c4e8 can retain multi-selection bits in normal mode. Native single-item
    // actions ignore them; require mode 0 below, not an empty selection mask.
    eligibilityReason = "pocket-state";
    if (Field<std::uintptr_t>(pocket, 0) != game::MainAddress(address::PocketControllerVtable) ||
        Field<int>(pocket, layout::PocketMode) != 0 ||
        Field<int>(pocket, layout::CurrentState) != state ||
        Field<std::uint8_t>(pocket, layout::TransitionSlot) > 1 ||
        Field<int>(pocket, layout::PocketError) != 0 || slot < 0 || slot >= PocketSlotLimit ||
        (Field<std::uint64_t>(pocket, layout::BlockedSlots) & (1ull << slot))) {
        return false;
    }
    const unsigned phase = Field<std::uint8_t>(pocket, layout::TransitionSlot);
    const int activeQueue = Field<int>(pocket, layout::TransitionA + 4 * phase);
    const int otherQueue = Field<int>(pocket, layout::TransitionA + 4 * (1 - phase));
    // During ENTRY the previous phase still holds the state being entered.
    if (activeQueue != -1 || (otherQueue != -1 &&
        !(enteringMenu && state == state::SelectingCommand && otherQueue == state))) return false;
    const auto inventory = game::quick_store_native::InventoryForPocket(pocket).Slots();
    eligibilityReason = "capacity";
    if (slot >= static_cast<int>(inventory.Capacity())) {
        return false;
    }
    eligibilityReason = "not-own-home";
    if (requested == Action::PutInStorage &&
        !game::quick_store_native::OwnHome(pocket)) {
        return false;
    }
    const auto item = inventory.TryItemAt(slot);
    eligibilityReason = "empty-item";
    if (!item.present || static_cast<std::uint16_t>(item.value) == 0xfffe) {
        return false;
    }
    itemIdentity = item.Identity();
    auto *menu = Field<void *>(pocket, layout::PocketMenu);
    eligibilityReason = "menu-state-or-binding";
    if (!menu ||
        (Field<int>(menu, layout::CurrentState) != 0 &&
         Field<int>(menu, layout::CurrentState) != 1) ||
        !HasNoQueuedTransition(menu) ||
        Field<std::uintptr_t>(menu, layout::MenuBuilderVtable) !=
            game::MainAddress(address::MenuBuilderVtable) ||
        Field<void *>(menu, layout::MenuBuilderOwner) != pocket ||
        Field<std::uintptr_t>(menu, layout::MenuBuilderFunction) !=
            game::MainAddress(address::BuildMenu) ||
        Field<std::uintptr_t>(menu, layout::MenuBuilderAdjustment) != 0) {
        return false;
    }

    return FindNativeShortcut(pocket, slot, choose, action, trace);
}

} // namespace

// Independent of the action's edge detector: report a shortcut press even if resolving
// the native owner/button binding fails. Bounded, and never changes eligibility.
static bool TracePress(void *page, void *inputSnapshot, unsigned shortcutButtons) {
static void *tracePage{};
static unsigned previous{}, reports{};
    if (tracePage != page) {
        tracePage = page;
        previous = 0;
    }
    const int count = Field<int>(inputSnapshot, layout::SnapshotRowCount);
    auto *rows = Field<std::byte *>(inputSnapshot, layout::SnapshotRows);
    if (!rows || count < 1 || count > InputRowLimit) {
        return false;
    }
    unsigned shortcutRows = 0;
    for (int i = 0; i < count; ++i) {
        if (Field<unsigned>(rows + i * layout::InputRowBytes, 0) & shortcutButtons) {
            shortcutRows |= 1u << i;
        }
    }
    const bool fresh = (shortcutRows & ~previous) != 0;
    previous = shortcutRows;
    if (!fresh || reports >= 64) {
        return false;
    }
    ++reports;
    Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_TRACE press=%u shortcut_rows=%x\n", reports,
                shortcutRows);
    return true;
}

// Diagnostics are observational; keep them separate from input/eligibility.
static void TracePocketBinding(void *page, void *owner, void *backButtonInput) {

    Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_TRACE binding owner=%lx cols=%d rows=%d "
                "unit=%lx button=%d input_row=%d\n",
                reinterpret_cast<std::uintptr_t>(owner),
                Field<int>(page, layout::BindingColumnCount),
                Field<int>(page, layout::BindingRowCount),
                reinterpret_cast<std::uintptr_t>(backButtonInput),
                backButtonInput ? Field<int>(backButtonInput, layout::HintButtonId) : -1,
                backButtonInput ? Field<int>(backButtonInput, layout::HintInputRow) : -1);
    auto *first = Field<void *>(page, layout::InputBindings);
    if (!owner && first && Field<int>(page, layout::BindingColumnCount) > 0 &&
        Field<int>(page, layout::BindingRowCount) > 0) {
        Logging.Debug(
            "[QoL Enhancements/Pockets] QUICK_STORE_TRACE first_binding vt=%lx fn=%lx adjust=%lx\n",
            Field<std::uintptr_t>(first, 0) - game::MainAddress(0),
            Field<std::uintptr_t>(first, layout::BindingFunction) - game::MainAddress(0),
            Field<std::uintptr_t>(first, layout::BindingAdjustment));
    }
    if (owner) {
        Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_TRACE pocket state=%d mode=%d phase=%u "
                    "queues=%d,%d equipped_slot=%d selected_mask=%lx blocked_mask=%lx error=%d "
                    "hover=%d logical=%x\n",
                    Field<int>(owner, layout::CurrentState), Field<int>(owner, layout::PocketMode),
                    unsigned(Field<std::uint8_t>(owner, layout::TransitionSlot)),
                    Field<int>(owner, layout::TransitionA), Field<int>(owner, layout::TransitionB),
                    int(Field<std::int8_t>(owner, layout::EquippedSlot)),
                    Field<std::uint64_t>(owner, layout::SelectedSlots),
                    Field<std::uint64_t>(owner, layout::BlockedSlots),
                    Field<int>(owner, layout::PocketError),
                    int(Field<std::int8_t>(owner, layout::TransientHoverSlot)),
                    unsigned(Field<std::uint16_t>(owner, layout::LogicalInput)));
    }
}

static unsigned FreshShortcutButtons(void *page, int row, unsigned buttons, unsigned shortcutButtons) {
    if (lastPage != page || lastRow != row) {
        lastPage = page;
        lastRow = row;
        hasPreviousInput = false;
    }
    const unsigned fresh = hasPreviousInput ? buttons & ~previousButtons & shortcutButtons : 0;
    previousButtons = buttons;
    hasPreviousInput = true; // Track even when blocked: no delayed held press.
    return fresh;
}

std::optional<PocketInput> ReadPocketInput(void *page, void *inputSnapshot, unsigned blocked,
                                          unsigned shortcutButtons, bool pending) {
    if (!page || Field<std::uintptr_t>(page, 0) != game::MainAddress(address::PocketPageVtable)) return std::nullopt;
    if (!inputSnapshot) { ResetInputEdges(); return std::nullopt; }
    const bool trace = TracePress(page, inputSnapshot, shortcutButtons);
    auto *owner = pocket_input::FindPocketController(page);
    auto *backButtonHint = Field<void *>(page, layout::BackButtonHint);
    auto *backButtonInput =
        backButtonHint ? Field<void *>(backButtonHint, layout::HintInputUnit) : nullptr;
    if (trace) {
        TracePocketBinding(page, owner, backButtonInput);
    }
    // Use the same controller row as the native B binding, not arbitrary row 0.
    if (!owner || !backButtonInput ||
        Field<int>(backButtonInput, layout::HintButtonId) != NativeBackButtonId) {
        ResetInputEdges();
        return std::nullopt;
    }
    const int row = Field<int>(backButtonInput, layout::HintInputRow),
              count = Field<int>(inputSnapshot, layout::SnapshotRowCount);
    auto *rows = Field<std::byte *>(inputSnapshot, layout::SnapshotRows);
    if (!rows || count < 1 || count > InputRowLimit || row < 0 || row >= count) {
        ResetInputEdges();
        return std::nullopt;
    }
    auto *inputRow = rows + row * layout::InputRowBytes;
    const unsigned buttons = Field<unsigned>(inputRow, 0);
    const unsigned pressed = FreshShortcutButtons(page, row, buttons, shortcutButtons);
    const unsigned heldShortcuts = buttons & AllShortcutButtons;
    // Require exactly one held shortcut. Never choose an arbitrary destructive
    // action when two triggers (or Y plus a trigger) arrive together.
    const bool freshSingleShortcut = pressed && pressed == heldShortcuts &&
                            (pressed & (pressed - 1)) == 0;
    auto *context = Field<void *>(page, layout::InputContext);
    auto *application = *reinterpret_cast<void **>(game::MainAddress(address::ApplicationGlobal));
    auto *globalInputMasks =
        application ? Field<void *>(application, layout::GlobalInputMasks) : nullptr;
    if (trace) {
        Logging.Debug(
            "[QoL Enhancements/Pockets] QUICK_STORE_TRACE input fresh=%u held=%x consumed=%x "
            "parent=%u local=%u global=%x mask=%x pending=%u\n",
            unsigned(freshSingleShortcut), buttons, Field<unsigned>(inputRow, layout::ConsumedButtons),
            blocked & 1, context ? unsigned(Field<std::uint8_t>(context, layout::InputBlocked)) : 1,
            globalInputMasks ? Field<unsigned>(globalInputMasks, layout::GlobalInputMask) : ~0u,
            context ? Field<unsigned>(context, layout::InputMask) : ~0u,
            unsigned(pending));
    }
    if (!freshSingleShortcut || !context || !globalInputMasks || (blocked & 1) ||
        Field<std::uint8_t>(context, layout::InputBlocked) ||
        (Field<unsigned>(context, layout::InputMask) &
         Field<unsigned>(globalInputMasks, layout::GlobalInputMask)) ||
        (Field<unsigned>(inputRow, layout::ConsumedButtons) & pressed) ||
        (buttons & OtherActionButtons) || Field<std::uint16_t>(owner, layout::LogicalInput) != 0) {
        return std::nullopt;
    }
    const int slot = pocket_input::FindFocusedItemSlot(page, row);
    if (trace) {
        Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_TRACE focused_slot=%d\n", slot);
    }
    return PocketInput{owner, slot, row, pressed, trace, inputRow};
}
void TraceEligibility(void *owner, bool trace) {
    if (trace) {
        Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_TRACE eligibility=%s\n",
                    EligibilityReason());
        auto *menu = Field<void *>(owner, layout::PocketMenu);
        if (menu) {
            Logging.Debug(
                "[QoL Enhancements/Pockets] QUICK_STORE_TRACE menu state=%d queues=%d,%d "
                "builder_vt=%lx builder_fn=%lx owner_match=%u adjust=%lx\n",
                Field<int>(menu, layout::CurrentState), Field<int>(menu, layout::TransitionA),
                Field<int>(menu, layout::TransitionB),
                Field<std::uintptr_t>(menu, layout::MenuBuilderVtable) - game::MainAddress(0),
                Field<std::uintptr_t>(menu, layout::MenuBuilderFunction) - game::MainAddress(0),
                unsigned(Field<void *>(menu, layout::MenuBuilderOwner) == owner),
                Field<std::uintptr_t>(menu, layout::MenuBuilderAdjustment));
        }
    }
}
void ConsumeShortcut(const PocketInput &input) {
    Field<unsigned>(input.nativeRow, layout::ConsumedButtons) |= input.freshShortcut;
}
void ResetInputEdges() { hasPreviousInput = false; previousButtons = 0; }

const char *EligibilityReason() { return eligibilityReason; }
int SelectedSlot(void *pocket) { return Field<std::int8_t>(pocket, layout::SelectedSlot); }
unsigned char Resident(void *pocket) { return Field<unsigned char>(pocket, layout::PocketInventory); }
std::optional<Eligibility> Probe(void *pocket, int slot, Phase phase, Action requested, game::ChooseMenuEntry choose,
                                 bool trace, bool enteringMenu) {
    int row = -1;
    ItemIdentity item{};
    if (!CanUseShortcut(pocket, slot, (phase == Phase::Browsing ? state::Browsing : phase == Phase::SelectingCommand ? state::SelectingCommand : state::Command), requested, row, item, choose, trace, enteringMenu)) return std::nullopt;
    return Eligibility{item, static_cast<RowToken>(row), Resident(pocket)};
}
void QueueCommand(void *pocket) { QueueControllerState(pocket, state::Command); }
void QueueCommand(void *pocket, int slot) {
    Field<std::int8_t>(pocket, layout::SelectedSlot) = slot;
    QueueCommand(pocket);
}
void CancelToBrowsing(void *pocket) {
    if (Field<std::uint8_t>(pocket, layout::TransitionSlot) <= 1 && HasNoQueuedTransition(pocket))
        QueueControllerState(pocket, state::Browsing);
}
void BlockMenuInput(void *pocket) {
    auto *page = Field<void *>(pocket, layout::PocketPage);
    auto *context = page ? Field<void *>(page, layout::InputContext) : nullptr;
    if (context) Field<std::uint8_t>(context, layout::InputBlocked) = 1;
}
void PlayCursorTap(void *pocket, int slot, int inputRow) { PlayCursorTapNow(pocket, slot, inputRow); }
CommandProgress Dispatch(void *owner, RowToken selectedRow, int slot, int inputRow,
                         NativeCallback original, bool feedbackBefore, bool feedbackAfter) {
    auto *menu = Field<void *>(owner, layout::PocketMenu);
    auto &count = Field<int>(menu, layout::MenuRowCount);
    auto &action = Field<int>(menu, layout::MenuFirstAction);
    auto &selection = Field<std::uint8_t>(menu, layout::MenuSelection);
    const int oldCount = count, oldAction = action;
    count = 1;
    action = int(selectedRow);
    selection = 0;
    // UPDATE only: the native transition consumer clears entry-time queues.
    // Capture validity before removal and keep this widget borrow synchronous.
    const auto feedback = CaptureItemFeedback(owner, slot, inputRow);
    if (feedbackBefore) PlayItemFeedback(feedback);
    original(owner);
    count = oldCount;
    action = oldAction;
    selection = 0xff;
    if (HasNoQueuedTransition(owner)) return {};
    const bool success = Field<int>(owner, layout::PocketError) == 0;
    if (success && feedbackAfter) PlayItemFeedback(feedback);
    const int next = Field<int>(owner, layout::TransitionA + sizeof(int) *
                               Field<std::uint8_t>(owner, layout::TransitionSlot));
    Logging.Debug("[QoL Enhancements/Pockets] POCKET_SHORTCUT action=%d dispatched\n", int(selectedRow));
    return {true, success, next == state::Browsing || next == state::Refresh};
}
} // namespace qol_pockets::game::pocket_commands_native
