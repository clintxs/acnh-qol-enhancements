#include "quick_store.hpp"
#include "program/pockets/game/stack_bindings.hpp"
#include "program/core/page_input.hpp"
#include <array>
#include <cstring>

namespace qol_pockets::features {
namespace {
// Native entrypoints and vtables are relative to main; feature hooks below
// validate their instruction fingerprints before replacing any code.
namespace address {
constexpr std::uintptr_t PocketControllerVtable = 0x04a06e28;
constexpr std::uintptr_t MenuBuilderVtable = 0x04a071c8;
constexpr std::uintptr_t MenuDescriptionVtable = 0x049c7208;
constexpr std::uintptr_t EmptyCallbackVtable = 0x049c7230;
constexpr std::uintptr_t InputBindingVtable = 0x04a070d8;
constexpr std::uintptr_t PocketInputCallback = 0x03088724;
constexpr std::uintptr_t PocketPageVtable = 0x04a06b00;
constexpr std::uintptr_t ApplicationGlobal = 0x059f83f0;
constexpr std::uintptr_t Play = 0x0308aa80, Command = 0x0308c5f8;
constexpr std::uintptr_t BuildMenu = 0x03088df0, OwnHome = 0x02815598;
constexpr std::uintptr_t Capacity = 0x02572e00, Item = 0x02577ea0;
constexpr std::uintptr_t ButtonFeedback = 0x03c66950;
constexpr std::uintptr_t ItemFeedbackValidity = 0x02fbfe5c;
constexpr std::uintptr_t UiApplicationGlobalGot = 0x04ab2230;
constexpr std::uintptr_t CursorVtable = 0x04a0ea80;
constexpr std::uintptr_t AnimationOperationVtableGot = 0x04ac4e68;
} // namespace address
// Field offsets in live 3.0.3 objects, not serialized save data. Names describe
// the observed role; we deliberately do not pretend the full class is known.
namespace layout {
constexpr std::size_t PocketPage = 0x210;
constexpr std::size_t FeedbackValidityParameter = 0x2a0;
constexpr std::size_t FeedbackParameterValue = 0x40;
constexpr std::size_t FeedbackRowParameter = 0x2a8;
constexpr std::size_t FeedbackContext = 0x298;
constexpr std::size_t SnapshotRowCount = 8;
constexpr std::size_t SnapshotRows = 0x10;
constexpr std::size_t TransitionA = 0x1f0;
constexpr std::size_t TransitionB = 0x1f4;
constexpr std::size_t TransitionSlot = 0x200;
constexpr std::size_t SelectedSlot = 0x1c514;
constexpr std::size_t PocketMode = 0x1c4fc;
constexpr std::size_t CurrentState = 0x1b0;
constexpr std::size_t PocketError = 0x1c50c;
constexpr std::size_t BlockedSlots = 0x1c4f0;
constexpr std::size_t PocketMenu = 0x218;
constexpr std::size_t MenuBuilderVtable = 0x218;
constexpr std::size_t MenuBuilderOwner = 0x220;
constexpr std::size_t MenuBuilderFunction = 0x228;
constexpr std::size_t MenuBuilderAdjustment = 0x230;
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
constexpr std::size_t HintButtonId = 0x14;
constexpr std::size_t HintInputRow = 0x10;
constexpr std::size_t EquippedSlot = 0x1c516;
constexpr std::size_t SelectedSlots = 0x1c4e8;
constexpr std::size_t TransientHoverSlot = 0x1c518;
constexpr std::size_t LogicalInput = 0x1c51a;
constexpr std::size_t BackButtonHint = 0x570;
constexpr std::size_t HintInputUnit = 0xa0;
constexpr std::size_t GlobalInputMasks = 0x418;
constexpr std::size_t ConsumedButtons = 0x3c;
constexpr std::size_t InputBlocked = 0x54;
constexpr std::size_t GlobalInputMask = 0x10;
constexpr std::size_t InputMask = 0x58;
constexpr std::size_t MenuRowCount = 0x26d8;
constexpr std::size_t MenuFirstAction = 0x3a0;
constexpr std::size_t MenuSelection = 0x26c9;
constexpr std::size_t ItemWidgets = 0x590;
constexpr std::size_t InputRowBytes = 0x48;
constexpr std::size_t BindingBytes = 32;
constexpr std::size_t PocketInventory = 0x1f8;
} // namespace layout
namespace cursor_layout {
constexpr std::size_t ApplicationCursor = 0xd0;
constexpr std::size_t Page = 0x210, FocusedPane = 0x218, OverrideLayout = 0x220;
constexpr std::size_t HideRequested = 0x26c, PageDefaultLayout = 0x570;
constexpr std::size_t ClickAnimation = 0x208;
constexpr std::size_t AnimationTarget = 8, AnimationImplementation = 0x18;
constexpr std::size_t PrepareOperationVirtual = 0x18, PlayVirtual = 0x190;
} // namespace cursor_layout
namespace state {
constexpr int Browsing = 3, Command = 7, NoQueuedTransition = -1;
}
constexpr int PocketSlotLimit = 40, InputRowLimit = 8, NativeBackButtonId = 5;
constexpr unsigned OtherActionButtons = 0x7f;
template <typename T> T &Field(void *controllerObject, std::size_t offset) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(controllerObject) + offset);
}
constexpr unsigned Y = 0x80;
constexpr int PutInStorage = 49;
struct MenuRow {
    const char *bank{};
    const char *key{};
    int action{};
    unsigned flags{}, extra{}, padding{};
};
struct MenuCallback {
    std::uintptr_t vtable{}, fields[3]{};
};
struct MenuDescription {
    std::uintptr_t vtable{}, bankVtable{};
    const char *bank{};
    std::byte reserved[0x28]{};
    int error{}, count{};
    MenuRow rows[7]{};
    MenuCallback callbacks[7]{};
};
static_assert(sizeof(MenuDescription) == 0x208 && offsetof(MenuDescription, rows) == 0x48);

// Flow: Y edge -> validate native storage row -> queue Command -> revalidate
// item identity -> run native dispatch -> restore menu fields.
// A request crosses one native state transition. It never owns game pointers:
// only a matching live controller may consume it, and Play entry invalidates it.
struct Request {
    void *owner{};
    int slot{};
    std::uint64_t item{};
    int inputRow{};
};
Request pending{};
// Successful transfer -> native removal refresh -> browsing entry -> cursor tap.
// Store no widget/layout pointers across the transition; resolve them afresh.
struct CursorTapRequest {
    void *owner{};
    int slot{}, inputRow{};
};
CursorTapRequest pendingTap{};
void *lastPage{};
int lastRow = -1;
bool wasYHeld = false, hasPreviousInput = false;
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
    return {widget, game::FunctionAt<int (*)(void *)>(address::ItemFeedbackValidity)(widget),
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
    struct SafeString {
        std::uintptr_t vtable;
        const char *text;
    };
    const SafeString event{*reinterpret_cast<const std::uintptr_t *>(
                               game::MainAddress(game::stack_offsets::SafeStringVtableGot)) +
                               16,
                           "Decide"};
    game::FunctionAt<void (*)(void *, const SafeString *, void *)>(address::ButtonFeedback)(
        widget, &event, Field<void *>(widget, layout::FeedbackContext));
}

void PlayCursorTap(void *pocket, int slot, int inputRow) {
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

// Independent of the action's edge detector: report a Y press even if resolving
// the native owner/button binding fails. Bounded, and never changes eligibility.
bool TracePress(void *page, void *inputSnapshot) {
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
    unsigned isYDown = 0;
    for (int i = 0; i < count; ++i) {
        if (Field<unsigned>(rows + i * layout::InputRowBytes, 0) & Y) {
            isYDown |= 1u << i;
        }
    }
    const bool fresh = (isYDown & ~previous) != 0;
    previous = isYDown;
    if (!fresh || reports >= 64) {
        return false;
    }
    ++reports;
    Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_TRACE press=%u Y_rows=%x\n", reports,
                isYDown);
    return true;
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
bool NativeMenuAllowsStorage(void *pocket, int slot, bool trace) {
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
    game::FunctionAt<void (*)(void *, MenuDescription *)>(address::BuildMenu)(pocket, &description);
    selected = previous;
    if (trace) {
        Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_TRACE built rows=%d error=%d\n",
                    description.count, description.error);
    }
    eligibilityReason = "menu-row-count";
    if (description.count < 1 || description.count > 7) {
        return false;
    }
    eligibilityReason = "no-unflagged-native-storage-row";
    for (int i = 0; i < description.count; ++i) {
        const auto &row = description.rows[i];
        if (trace) {
            Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_TRACE row=%d action=%d flags=%x "
                        "extra=%x callback_vt=%lx fields=%lx,%lx,%lx\n",
                        i, row.action, row.flags, row.extra,
                        description.callbacks[i].vtable - game::MainAddress(0),
                        description.callbacks[i].fields[0], description.callbacks[i].fields[1],
                        description.callbacks[i].fields[2]);
        }
        if (row.action == PutInStorage && row.flags == 0 && row.extra == 0 &&
            description.callbacks[i].vtable == game::MainAddress(address::EmptyCallbackVtable) &&
            description.callbacks[i].fields[0] == 0 && description.callbacks[i].fields[1] == 0 &&
            description.callbacks[i].fields[2] == 0) {
            eligibilityReason = "eligible";
            return true;
        }
    }
    return false;
}
bool CanQuickStoreItem(void *pocket, int slot, int state, std::uint64_t &itemIdentity,
                       bool trace = false) {
    // Only browsing (3) / our dispatch (7) are allowed; drag states 8/9 are not.
    // +1c516 is the equipped-item slot, not a drag flag. Leave it to the game.
    // +1c4e8 can retain multi-selection bits in normal mode. Native single-item
    // actions ignore them; require mode 0 below, not an empty selection mask.
    eligibilityReason = "pocket-state";
    if (Field<std::uintptr_t>(pocket, 0) != game::MainAddress(address::PocketControllerVtable) ||
        Field<int>(pocket, layout::PocketMode) != 0 ||
        Field<int>(pocket, layout::CurrentState) != state ||
        Field<std::uint8_t>(pocket, layout::TransitionSlot) > 1 || !HasNoQueuedTransition(pocket) ||
        Field<int>(pocket, layout::PocketError) != 0 || slot < 0 || slot >= PocketSlotLimit ||
        (Field<std::uint64_t>(pocket, layout::BlockedSlots) & (1ull << slot))) {
        return false;
    }
    auto *inventory = static_cast<std::byte *>(pocket) + layout::PocketInventory;
    eligibilityReason = "capacity";
    if (slot >= game::FunctionAt<int (*)(void *)>(address::Capacity)(inventory)) {
        return false;
    }
    eligibilityReason = "not-own-home";
    if (!(game::FunctionAt<unsigned (*)(void *)>(address::OwnHome)(inventory) & 1)) {
        return false;
    }
    auto *item = game::FunctionAt<void *(*)(void *, int)>(address::Item)(inventory, slot);
    eligibilityReason = "empty-item";
    if (!item || Field<std::uint16_t>(item, 0) == 0xfffe) {
        return false;
    }
    itemIdentity = Field<std::uint64_t>(item, 0);
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

    return NativeMenuAllowsStorage(pocket, slot, trace);
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

// Diagnostics are observational; keep them separate from input/eligibility.
void TracePocketBinding(void *page, void *owner, void *backButtonInput) {

    Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_TRACE binding owner=%lx cols=%d rows=%d "
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
        Logging.Log(
            "[QoL Enhancements/Pockets] QUICK_STORE_TRACE first_binding vt=%lx fn=%lx adjust=%lx\n",
            Field<std::uintptr_t>(first, 0) - game::MainAddress(0),
            Field<std::uintptr_t>(first, layout::BindingFunction) - game::MainAddress(0),
            Field<std::uintptr_t>(first, layout::BindingAdjustment));
    }
    if (owner) {
        Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_TRACE pocket state=%d mode=%d phase=%u "
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

bool IsFreshYPress(void *page, int row, bool isYDown) {
    if (lastPage != page || lastRow != row) {
        lastPage = page;
        lastRow = row;
        hasPreviousInput = false;
    }
    const bool isNewYPress = hasPreviousInput && isYDown && !wasYHeld;
    wasYHeld = isYDown;
    hasPreviousInput = true; // Track even when blocked: no delayed held press.
    return isNewYPress;
}

void HandleQuickStoreInput(void *page, void *inputSnapshot, unsigned blocked) {
    if (!inputSnapshot ||
        Field<std::uintptr_t>(page, 0) != game::MainAddress(address::PocketPageVtable)) {
        return;
    }
    const bool trace = TracePress(page, inputSnapshot);
    auto *owner = FindPocketController(page);
    auto *backButtonHint = Field<void *>(page, layout::BackButtonHint);
    auto *backButtonInput =
        backButtonHint ? Field<void *>(backButtonHint, layout::HintInputUnit) : nullptr;
    if (trace) {
        TracePocketBinding(page, owner, backButtonInput);
    }
    // Use the same controller row as the native B binding, not arbitrary row 0.
    if (!owner || !backButtonInput ||
        Field<int>(backButtonInput, layout::HintButtonId) != NativeBackButtonId) {
        return;
    }
    const int row = Field<int>(backButtonInput, layout::HintInputRow),
              count = Field<int>(inputSnapshot, layout::SnapshotRowCount);
    auto *rows = Field<std::byte *>(inputSnapshot, layout::SnapshotRows);
    if (!rows || count < 1 || count > InputRowLimit || row < 0 || row >= count) {
        return;
    }
    auto *inputRow = rows + row * layout::InputRowBytes;
    const unsigned buttons = Field<unsigned>(inputRow, 0);
    const bool isYDown = (buttons & Y) != 0;
    const bool isNewYPress = IsFreshYPress(page, row, isYDown);
    auto *context = Field<void *>(page, layout::InputContext);
    auto *application = *reinterpret_cast<void **>(game::MainAddress(address::ApplicationGlobal));
    auto *globalInputMasks =
        application ? Field<void *>(application, layout::GlobalInputMasks) : nullptr;
    if (trace) {
        Logging.Log(
            "[QoL Enhancements/Pockets] QUICK_STORE_TRACE input fresh=%u held=%x consumed=%x "
            "parent=%u local=%u global=%x mask=%x pending=%u\n",
            unsigned(isNewYPress), buttons, Field<unsigned>(inputRow, layout::ConsumedButtons),
            blocked & 1, context ? unsigned(Field<std::uint8_t>(context, layout::InputBlocked)) : 1,
            globalInputMasks ? Field<unsigned>(globalInputMasks, layout::GlobalInputMask) : ~0u,
            context ? Field<unsigned>(context, layout::InputMask) : ~0u,
            unsigned(pending.owner != nullptr));
    }
    if (!isNewYPress || pending.owner || !context || !globalInputMasks || (blocked & 1) ||
        Field<std::uint8_t>(context, layout::InputBlocked) ||
        (Field<unsigned>(context, layout::InputMask) &
         Field<unsigned>(globalInputMasks, layout::GlobalInputMask)) ||
        (Field<unsigned>(inputRow, layout::ConsumedButtons) & Y) ||
        (buttons & OtherActionButtons) || Field<std::uint16_t>(owner, layout::LogicalInput) != 0) {
        return;
    }
    const int slot = FindFocusedItemSlot(page, row);
    if (trace) {
        Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_TRACE focused_slot=%d\n", slot);
    }
    std::uint64_t item{};
    const bool eligible = CanQuickStoreItem(owner, slot, state::Browsing, item, trace);
    if (trace) {
        Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_TRACE eligibility=%s\n",
                    eligibilityReason);
        auto *menu = Field<void *>(owner, layout::PocketMenu);
        if (menu) {
            Logging.Log(
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
    if (!eligible) {
        return;
    }
    pending = {owner, slot, item, row};
    Field<std::int8_t>(owner, layout::SelectedSlot) = slot;
    Field<unsigned>(inputRow, layout::ConsumedButtons) |= Y;
    QueueControllerState(owner, state::Command);
    if (trace) {
        Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_TRACE queued state 7\n");
    }
}

bool QuickStoreInput(void *page, void *inputSnapshot, unsigned blocked) {
    // The native page's final pass (03c4bb20) blanket-consumes every button.
    // Check before it, respecting upstream consumption and simultaneous
    // A/B/X/navigation. Always run the original exactly once afterward.
    HandleQuickStoreInput(page, inputSnapshot, blocked);
    return false;
}

HOOK_DEFINE_TRAMPOLINE(QuickStoreCommand) {
    static void Callback(void *owner) {
        if (pending.owner != owner) {
            Orig(owner);
            return;
        }
        const auto request = pending;
        pending = {}; // Exactly once, before calling any native code.
        pendingTap = {};
        std::uint64_t item{};
        if (Field<std::int8_t>(owner, layout::SelectedSlot) != request.slot ||
            !CanQuickStoreItem(owner, request.slot, state::Command, item) || item != request.item) {
            Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_TRACE command cancelled selected=%d "
                        "expected=%d eligibility=%s identity_match=%u\n",
                        int(Field<std::int8_t>(owner, layout::SelectedSlot)), request.slot,
                        eligibilityReason, unsigned(item == request.item));
            if (Field<std::uint8_t>(owner, layout::TransitionSlot) <= 1 &&
                HasNoQueuedTransition(owner)) {
                QueueControllerState(owner, state::Browsing);
            }
            return;
        }
        auto *menu = Field<void *>(owner, layout::PocketMenu);
        auto &count = Field<int>(menu, layout::MenuRowCount);
        auto &action = Field<int>(menu, layout::MenuFirstAction);
        auto &selection = Field<std::uint8_t>(menu, layout::MenuSelection);
        const int oldCount = count, oldAction = action;
        count = 1;
        action = PutInStorage;
        selection = 0;
        // Run in state UPDATE, never entry: the transition consumer clears
        // pending states after entry. Native dispatch queues refresh/error here.
        const auto feedback = CaptureItemFeedback(owner, request.slot, request.inputRow);
        Orig(owner);
        count = oldCount;
        action = oldAction;
        selection = 0xff;
        if (Field<int>(owner, layout::PocketError) == 0) {
            PlayItemFeedback(feedback);
            pendingTap = {owner, request.slot, request.inputRow};
        }
        Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE native Put in Storage dispatched\n");
    }
};

HOOK_DEFINE_TRAMPOLINE(QuickStorePlay) {
    static void Callback(void *owner) {
        const auto tap = pendingTap;
        pendingTap = {};
        pending = {};
        hasPreviousInput = false;
        wasYHeld = false;
        Orig(owner);
        if (tap.owner == owner) {
            PlayCursorTap(owner, tap.slot, tap.inputRow);
        }
    }
};
} // namespace

// Instruction fingerprints are exact 3.0.3 ARM words, intentionally left as
// hex. They guard hook locations and native assumptions; do not edit them to
// silence a mismatch without re-checking the game's disassembly.
bool InstallQuickStore() {
    struct Check {
        std::uintptr_t offset;
        std::array<std::uint32_t, 5> words;
    };
    constexpr std::array checks{
        Check{address::Play, {0xa9ba7bfd, 0xa9016ffc, 0x910003fd, 0xa90267fa, 0xa9035ff8}},
        Check{address::Command, {0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd, 0x91407008}},
        Check{address::BuildMenu, {0xd10383ff, 0xa9087bfd, 0xa9096ffc, 0xa90a67fa, 0xa90b5ff8}},
        Check{address::OwnHome, {0xf0011988, 0x798fa908, 0xb100051f, 0x540007a0, 0xd100c3ff}},
        Check{0x03076d78, {0xd10103ff, 0xa9017bfd, 0xa90257f6, 0xa9034ff4, 0x910043fd}},
        Check{address::ButtonFeedback,
              {0xd102c3ff, 0xa9077bfd, 0xf90043f7, 0xa90957f6, 0xa90a4ff4}},
        Check{address::ItemFeedbackValidity,
              {0x394b0c09, 0x7200013f, 0x394c5408, 0x7a401900, 0x1a9f07e0}},
        Check{0x031007dc, {0xf9411268, 0xb5000088, 0xf9410a68, 0xf942b908, 0xb4000268}},
        Check{0x0310080c, {0xf90013e8, 0x52800028, 0xb9002be8, 0xf9400288, 0xf9400d08}},
        Check{0x03100824, {0xf9400e88, 0x1e2e1000, 0xf9400680, 0xf940c908, 0xd63f0100}}};
    for (const auto &check : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(check.offset)),
                        check.words.data(), sizeof(check.words)) != 0) {
            Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_DISABLED code mismatch at 0x%lx\n",
                        check.offset);
            return false;
        }
    }
    if (!qol::core::RegisterPageInput(QuickStoreInput)) {
        Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_DISABLED shared input unavailable\n");
        return false;
    }
    QuickStoreCommand::InstallAtOffset(address::Command);
    QuickStorePlay::InstallAtOffset(address::Play);
    Logging.Log("[QoL Enhancements/Pockets] QUICK_STORE_READY Y uses native Put in Storage\n");
    return true;
}
} // namespace qol_pockets::features
