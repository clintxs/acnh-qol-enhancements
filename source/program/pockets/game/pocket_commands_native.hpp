#pragma once
#include "quick_store_native.hpp"
#include "program/pockets/services/pocket_commands.hpp"
#include <optional>

namespace qol_pockets::game::pocket_commands_native {
using Action = commands::PocketAction;
using MenuRowId = v303::quick_store::MenuRowId;
using ItemIdentity = qol::game::inventory::ItemIdentity;
enum class Phase : int { Browsing = 3, SelectingCommand = 5, Command = 7 };
using NativeCallback = void (*)(void *);
struct Eligibility {
    ItemIdentity item;
    MenuRowId row;
    unsigned char resident;
};
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
CommandProgress Dispatch(void *pocket, MenuRowId row, int slot, int inputRow,
                         NativeCallback original, bool feedbackBefore, bool feedbackAfter);
void PlayCursorTap(void *pocket, int slot, int inputRow);
} // namespace qol_pockets::game::pocket_commands_native
