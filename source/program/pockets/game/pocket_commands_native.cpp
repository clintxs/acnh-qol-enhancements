#include "pocket_commands_native.hpp"
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
namespace state {
constexpr int Browsing = 3, Command = 7, Refresh = 14, NoQueuedTransition = -1;
constexpr int SelectingCommand = 5;
}
constexpr int PocketSlotLimit = 40;
namespace open_action {
constexpr int Present = int(MenuRowId::Present), Treasure = int(MenuRowId::Treasure), Bottle = int(MenuRowId::Bottle);
}
const char *eligibilityReason = "not-checked";
constexpr bool IsConsume(int row) {
    return row == int(MenuRowId::Eat) || row == int(MenuRowId::EatOne) || row == int(MenuRowId::Drink);
}
constexpr bool Matches(Action action, int row) {
    return (action == Action::PutInStorage && row == int(MenuRowId::PutInStorage)) ||
           (action == Action::DropOrRelease && (row == int(MenuRowId::DropItem) || row == int(MenuRowId::Release))) ||
           (action == Action::EquipOrConsume && (row == int(MenuRowId::Hold) || row == int(MenuRowId::Wear) ||
                                                row == int(MenuRowId::PutAway) || IsConsume(row)));
}
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
bool FindNativeShortcut(void *pocket, int slot, Action requested, int &action, bool trace) {
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
    bool found = false;
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
        const bool soleOpen = description.count == 1 && row.bank && row.key &&
            std::strcmp(row.bank, "CmnSubWin") == 0 &&
            ((row.action == open_action::Present && std::strcmp(row.key, "0113") == 0) ||
             (row.action == open_action::Treasure && std::strcmp(row.key, "0144") == 0) ||
             (row.action == open_action::Bottle && std::strcmp(row.key, "0148") == 0));
        if ((requested == Action::OpenContainer ? soleOpen : Matches(requested, row.action)) &&
            row.flags == 0 && row.extra == 0 &&
            description.callbacks[i].vtable == game::MainAddress(address::EmptyCallbackVtable) &&
            description.callbacks[i].fields[0] == 0 && description.callbacks[i].fields[1] == 0 &&
            description.callbacks[i].fields[2] == 0) {
            eligibilityReason = "eligible";
            // Equipment actions are less destructive: prefer them over eating
            // or drinking. Keep the first valid consume row only as fallback.
            if (!found || !IsConsume(row.action)) action = row.action;
            found = true;
            if (requested != Action::EquipOrConsume || !IsConsume(row.action)) return true;
        }
    }
    return found;
}
bool CanUseShortcut(void *pocket, int slot, int state, Action requested, int &action,
                    ItemIdentity &itemIdentity, bool trace = false, bool enteringMenu = false) {
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
    const auto inventory = game::quick_store_native::InventoryForPocket(pocket);
    eligibilityReason = "capacity";
    if (slot >= static_cast<int>(inventory.Capacity())) {
        return false;
    }
    eligibilityReason = "not-own-home";
    if (requested == Action::PutInStorage &&
        !game::quick_store_native::OwnHome(pocket)) {
        return false;
    }
    const auto item = inventory.ItemAt(slot);
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

    return FindNativeShortcut(pocket, slot, requested, action, trace);
}

} // namespace

const char *EligibilityReason() { return eligibilityReason; }
int SelectedSlot(void *pocket) { return Field<std::int8_t>(pocket, layout::SelectedSlot); }
unsigned char Resident(void *pocket) { return Field<unsigned char>(pocket, layout::PocketInventory); }
std::optional<Eligibility> Probe(void *pocket, int slot, Phase phase, Action requested,
                                 bool trace, bool enteringMenu) {
    int row = -1;
    ItemIdentity item{};
    if (!CanUseShortcut(pocket, slot, int(phase), requested, row, item, trace, enteringMenu)) return std::nullopt;
    return Eligibility{item, static_cast<MenuRowId>(row), Resident(pocket)};
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
CommandProgress Dispatch(void *owner, MenuRowId selectedRow, int slot, int inputRow,
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
