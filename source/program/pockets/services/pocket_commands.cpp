#include "pocket_commands.hpp"
#include "shortcut_declarations.hpp"
#include "lib.hpp"
#include "program/pockets/game/pocket_commands_native.hpp"
#include "pocket_lifecycle.hpp"
#include <optional>

namespace qol_pockets::commands {
namespace {
namespace native = game::pocket_commands_native;
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
    static inline ShortcutDeclarations declarations{};

    static unsigned Buttons() {
        unsigned mask = 0;
        for (const auto &rule : declarations.Rules()) mask |= static_cast<unsigned>(rule.button);
        return mask;
    }
    static std::optional<PocketAction> ActionFor(unsigned press) {
        for (const auto &rule : declarations.Rules())
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
public:
    static void HandleQuickStoreInput(void *page, void *snapshot, unsigned blocked) {
        const auto input = native::ReadPocketInput(page, snapshot, blocked, Buttons(), pending.has_value());
        if (!input || pending) return;
        const auto action = ActionFor(input->freshShortcut);
        if (!action) return;
        const auto request = FindEligible(input->owner, input->slot, native::Phase::Browsing,
                                           *action, input->row, input->trace);
        native::TraceEligibility(input->owner, input->trace);
        if (!request) return;
        Queue(*request);
        native::ConsumeShortcut(*input);
    }

    static void SetRules(std::span<const ShortcutRule> shortcuts) { declarations.Own(shortcuts); }
    static DeclarationMatch CompareRules(std::span<const ShortcutRule> shortcuts) { return declarations.Compare(shortcuts); }
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
        lifecycle::BeforePlayEntry(owner);
        const auto tap = pendingTap;
        const bool canTap = tap.owner == owner && tap.generation == generation &&
                            tap.resident == native::Resident(owner);
        pendingTap = {};
        pending.reset();
        ++generation;
        native::ResetInputEdges();
        original(owner);
        if (canTap) native::PlayCursorTap(owner, tap.slot, tap.inputRow);
    }
};
} // namespace
bool QuickStoreInput(void *page, void *snapshot, unsigned blocked) {
    PocketCommands::HandleQuickStoreInput(page, snapshot, blocked);
    return false;
}
DeclarationMatch CompareShortcutRules(std::span<const ShortcutRule> rules) { return PocketCommands::CompareRules(rules); }
void SetShortcutRules(std::span<const ShortcutRule> rules) { PocketCommands::SetRules(rules); }
void EnterMenu(void *owner, native::NativeCallback original) { PocketCommands::EnterMenu(owner, original); }
void UpdateMenu(void *owner, native::NativeCallback original, native::NativeCallback enterOriginal) {
    PocketCommands::UpdateMenu(owner, original, enterOriginal);
}
void UpdateCommand(void *owner, native::NativeCallback original) { PocketCommands::UpdateCommand(owner, original); }
void EnterPlay(void *owner, native::NativeCallback original) { PocketCommands::EnterPlay(owner, original); }
} // namespace qol_pockets::commands
