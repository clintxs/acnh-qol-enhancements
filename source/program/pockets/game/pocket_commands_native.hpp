#pragma once
#include "program/game/inventory.hpp"
#include "program/pockets/services/pocket_commands.hpp"
#include <optional>

namespace qol_pockets::game::pocket_commands_native {
using Action = commands::PocketAction;
enum class RowToken : int {}; // Opaque native eligibility evidence; policy only compares it.
using ItemIdentity = qol::game::inventory::ItemIdentity;
enum class Phase { Browsing, SelectingCommand, Command };
using NativeCallback = void (*)(void *);
struct Eligibility {
    ItemIdentity item;
    RowToken row;
    unsigned char resident;
};
struct PocketInput {
    void *owner;
    int slot, row;
    unsigned freshShortcut;
    bool trace;
    // Callback-scoped input-row borrow. Never retained by command policy.
    void *nativeRow;
};
std::optional<PocketInput> ReadPocketInput(void *page, void *snapshot, unsigned blocked,
                                          unsigned shortcutButtons, bool pending);
void ConsumeShortcut(const PocketInput &input);
void ResetInputEdges();
void TraceEligibility(void *owner, bool trace);
struct CommandProgress { bool transitioned, success, returnsToBrowsing; };
// Borrow only within a supported UI callback. Probe copies inventory values;
// neither it nor dispatch exposes a native inventory item pointer.
std::optional<Eligibility> Probe(void *pocket, int slot, Phase phase, Action action,
                                 bool trace = false, bool enteringMenu = false);
const char *EligibilityReason();
int SelectedSlot(void *pocket);
unsigned char Resident(void *pocket);
void QueueCommand(void *pocket, int slot);
void QueueCommand(void *pocket);
void CancelToBrowsing(void *pocket);
void BlockMenuInput(void *pocket);
CommandProgress Dispatch(void *pocket, RowToken row, int slot, int inputRow,
                         NativeCallback original, bool feedbackBefore, bool feedbackAfter);
void PlayCursorTap(void *pocket, int slot, int inputRow);
} // namespace qol_pockets::game::pocket_commands_native
