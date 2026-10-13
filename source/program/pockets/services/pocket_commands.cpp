#include "pocket_commands.hpp"
#include "program/pockets/game/pocket_commands_native.hpp"
#include "program/pockets/features/pocket_input.hpp"
#include "program/pockets/features/hold_select.hpp"
#include "program/core/hook_plan.hpp"
#include "program/core/native/runtime.hpp"
#include "program/core/page_input.hpp"
#include <optional>

namespace qol_pockets::commands {
namespace {
namespace native = game::pocket_commands_native;
namespace profile = game::v303::quick_store;
namespace address = profile::address;
namespace layout = profile::layout;
using game::quick_store_native::Field;
constexpr int InputRowLimit = 8, NativeBackButtonId = 5;
constexpr unsigned OtherActionButtons = 0x37f; // UI bits: D-pad, A/B/X and L/R.

class PocketCommands {
    // Opaque eligibility evidence: only FindEligible can create a request.
    // The owner address is a correlation check, not a reliable lifetime handle.
    // Verified Play/menu entry boundaries invalidate outstanding requests.
    struct Request {
        void *owner;
        int slot, inputRow;
        PocketAction action;
        native::Phase phase;
        native::Eligibility evidence;
        std::uint64_t generation;
        bool started = false;
    };
    struct CursorTapRequest {
        void *owner;
        int slot, inputRow;
        std::uint64_t generation;
        unsigned char resident;
    };
    static inline std::optional<Request> pending{};
    static inline CursorTapRequest pendingTap{};
    static inline std::uint64_t generation{};
    static inline std::span<const ShortcutRule> rules{};
    static inline void *lastPage{};
    static inline int lastRow = -1;
    static inline unsigned previousButtons{};
    static inline bool hasPreviousInput = false;

    static unsigned Buttons() {
        unsigned mask = 0;
        for (const auto &rule : rules) mask |= static_cast<unsigned>(rule.button);
        return mask;
    }
    static std::optional<PocketAction> ActionFor(unsigned press) {
        for (const auto &rule : rules)
            if (static_cast<unsigned>(rule.button) == press) return rule.action;
        return std::nullopt;
    }
    static std::optional<Request> FindEligible(void *owner, int slot, native::Phase phase,
                                              PocketAction action, int inputRow, bool trace = false,
                                              bool enteringMenu = false) {
        const auto evidence = native::Probe(owner, slot, phase, action, trace, enteringMenu);
        if (!evidence) return std::nullopt;
        return Request{owner, slot, inputRow, action, phase, *evidence, generation};
    }
    static void Queue(const Request &request) {
        // No item moves here. Only the verified Command UPDATE can dispatch.
        pending = request;
        if (request.phase == native::Phase::Browsing) native::QueueCommand(request.owner, request.slot);
        else native::BlockMenuInput(request.owner);
    }
    static bool StillEligible(const Request &request, native::Phase phase) {
        if (request.generation != generation || native::Resident(request.owner) != request.evidence.resident ||
            native::SelectedSlot(request.owner) != request.slot) return false;
        const auto current = FindEligible(request.owner, request.slot, phase, request.action, request.inputRow);
        return current && current->evidence.item == request.evidence.item &&
               current->evidence.row == request.evidence.row;
    }
    // Independent of the action's edge detector: report a shortcut press even if resolving
    // the native owner/button binding fails. Bounded, and never changes eligibility.
    static bool TracePress(void *page, void *inputSnapshot) {
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
            if (Field<unsigned>(rows + i * layout::InputRowBytes, 0) & Buttons()) {
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

    static unsigned FreshShortcutButtons(void *page, int row, unsigned buttons) {
        if (lastPage != page || lastRow != row) {
            lastPage = page;
            lastRow = row;
            hasPreviousInput = false;
        }
        const unsigned fresh = hasPreviousInput ? buttons & ~previousButtons & Buttons() : 0;
        previousButtons = buttons;
        hasPreviousInput = true; // Track even when blocked: no delayed held press.
        return fresh;
    }

public:
    static void HandleQuickStoreInput(void *page, void *inputSnapshot, unsigned blocked) {
        if (!inputSnapshot ||
            Field<std::uintptr_t>(page, 0) != game::MainAddress(address::PocketPageVtable)) {
            return;
        }
        const bool trace = TracePress(page, inputSnapshot);
        auto *owner = features::pocket_input::FindPocketController(page);
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
        const unsigned pressed = FreshShortcutButtons(page, row, buttons);
        const unsigned heldShortcuts = buttons & Buttons();
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
                unsigned(pending.has_value()));
        }
        if (!freshSingleShortcut || pending || !context || !globalInputMasks || (blocked & 1) ||
            Field<std::uint8_t>(context, layout::InputBlocked) ||
            (Field<unsigned>(context, layout::InputMask) &
             Field<unsigned>(globalInputMasks, layout::GlobalInputMask)) ||
            (Field<unsigned>(inputRow, layout::ConsumedButtons) & pressed) ||
            (buttons & OtherActionButtons) || Field<std::uint16_t>(owner, layout::LogicalInput) != 0) {
            return;
        }
        const int slot = features::pocket_input::FindFocusedItemSlot(page, row);
        if (trace) {
            Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_TRACE focused_slot=%d\n", slot);
        }
        const auto action = ActionFor(pressed);
        const auto eligible = action ? FindEligible(owner, slot, native::Phase::Browsing, *action, row, trace) : std::nullopt;
        if (trace) {
            Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_TRACE eligibility=%s\n",
                        native::EligibilityReason());
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
        if (!eligible) {
            return;
        }
        Queue(*eligible);
        Field<unsigned>(inputRow, layout::ConsumedButtons) |= pressed;
        if (trace) {
            Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_TRACE queued state 7\n");
        }
    }

    static void SetRules(std::span<const ShortcutRule> shortcuts) { rules = shortcuts; }
    static void EnterMenu(void *owner, native::NativeCallback original) {
        pending.reset();
        pendingTap = {};
        ++generation;
        const auto request = FindEligible(owner, native::SelectedSlot(owner), native::Phase::SelectingCommand,
                                           PocketAction::OpenContainer, 0, false, true);
        if (!request) { original(owner); return; }
        // Preserve the entry lock; queue the transition only during UPDATE.
        Queue(*request);
    }
    static void UpdateMenu(void *owner, native::NativeCallback original, native::NativeCallback enterOriginal) {
        if (!pending || pending->owner != owner || pending->action != PocketAction::OpenContainer) {
            original(owner);
            return;
        }
        if (!StillEligible(*pending, native::Phase::SelectingCommand)) {
            pending.reset();
            enterOriginal(owner);
            return;
        }
        native::QueueCommand(owner);
    }
    static void UpdateCommand(void *owner, native::NativeCallback original) {
        if (!pending || pending->owner != owner) { original(owner); return; }
        const auto request = *pending;
        pendingTap = {};
        if (request.generation != generation || native::Resident(owner) != request.evidence.resident ||
            (!request.started && !StillEligible(request, native::Phase::Command))) {
            pending.reset();
            Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_TRACE command cancelled selected=%d expected=%d eligibility=%s\n",
                          native::SelectedSlot(owner), request.slot, native::EligibilityReason());
            native::CancelToBrowsing(owner);
            return;
        }
        // Native placement may take many updates and modify the selected item.
        // Revalidate once before starting, then continue without restarting it.
        pending->started = true;
        const auto progress = native::Dispatch(owner, request.evidence.row, request.slot, request.inputRow, original,
            !request.started && request.action != PocketAction::PutInStorage && request.action != PocketAction::OpenContainer,
            request.action == PocketAction::PutInStorage);
        if (!progress.transitioned) return;
        pending.reset();
        if (progress.success && progress.returnsToBrowsing && request.action != PocketAction::OpenContainer)
            pendingTap = {owner, request.slot, request.inputRow, generation, request.evidence.resident};
    }
    static void EnterPlay(void *owner, native::NativeCallback original) {
        features::ResetHoldSelect(owner);
        const auto tap = pendingTap;
        const bool canTap = tap.owner == owner && tap.generation == generation &&
                            tap.resident == native::Resident(owner);
        pendingTap = {};
        pending.reset();
        ++generation;
        hasPreviousInput = false;
        previousButtons = 0;
        original(owner);
        if (canTap) native::PlayCursorTap(owner, tap.slot, tap.inputRow);
    }
};
bool QuickStoreInput(void *page, void *inputSnapshot, unsigned blocked) {
    // Observe before native blanket consumption; run the original once afterward.
    PocketCommands::HandleQuickStoreInput(page, inputSnapshot, blocked);
    return false;
}
HOOK_DEFINE_TRAMPOLINE(EnterPocketMenu) {
    static void Original(void *owner) { Orig(owner); }
    static void Callback(void *owner) { PocketCommands::EnterMenu(owner, Original); }
};
HOOK_DEFINE_TRAMPOLINE(UpdatePocketMenu) {
    static void Original(void *owner) { Orig(owner); }
    static void Callback(void *owner) {
        PocketCommands::UpdateMenu(owner, Original, EnterPocketMenu::Original);
    }
};
HOOK_DEFINE_TRAMPOLINE(QuickStoreCommand) {
    static void Original(void *owner) { Orig(owner); }
    static void Callback(void *owner) { PocketCommands::UpdateCommand(owner, Original); }
};
HOOK_DEFINE_TRAMPOLINE(QuickStorePlay) {
    static void Original(void *owner) { Orig(owner); }
    static void Callback(void *owner) { PocketCommands::EnterPlay(owner, Original); }
};
} // namespace
// Instruction fingerprints are exact 3.0.3 ARM words, intentionally left as
// hex. They guard hook locations and native assumptions; do not edit them to
// silence a mismatch without re-checking the game's disassembly.
bool InstallShortcuts(std::span<const ShortcutRule> rules) {
    if (!ValidShortcuts(rules)) return false;
    const auto executable = qol::native::MainExecutable();
    for (const auto &check : profile::Checks) {
        const auto result = executable.Check(check);
        if (!result) {
            Logging.Error("[QoL Enhancements/Pockets] QUICK_STORE_DISABLED code mismatch at 0x%lx\n", check.offset);
            qol::native::ReportMismatch(result);
            return false;
        }
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<QuickStoreCommand>("QuickStoreCommand", address::Command, profile::HookWords(address::Command), true),
        qol::core::Site<QuickStorePlay>("QuickStorePlay", address::Play, profile::HookWords(address::Play), true),
        qol::core::Site<EnterPocketMenu>("EnterPocketMenu", address::EnterMenu, profile::HookWords(address::EnterMenu), true),
        qol::core::Site<UpdatePocketMenu>("UpdatePocketMenu", address::UpdateMenu, profile::HookWords(address::UpdateMenu), true),
    };
    if (!hookPlan) return false;
    if (!qol::core::RegisterPageInput(QuickStoreInput)) {
        Logging.Error("[QoL Enhancements/Pockets] QUICK_STORE_DISABLED shared input unavailable\n");
        return false;
    }
    PocketCommands::SetRules(rules);
    if (!hookPlan.Publish<QuickStoreCommand>(address::Command)) return false;
    if (!hookPlan.Publish<QuickStorePlay>(address::Play)) return false;
    if (!hookPlan.Publish<EnterPocketMenu>(address::EnterMenu)) return false;
    if (!hookPlan.Publish<UpdatePocketMenu>(address::UpdateMenu)) return false;
    Logging.Debug("[QoL Enhancements/Pockets] OPEN_READY single-action A menu bypass\n");
    Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_READY Y storage; ZL drop/release; ZR hold/wear/put-away\n");
    return true;
}
} // namespace qol_pockets::commands
