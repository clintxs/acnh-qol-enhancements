#pragma once
namespace qol_pockets::lifecycle {
using PlayEntryReset = void (*)(void *controller);
// One existing consumer. Registration is startup-only and identical repeats
// are idempotent; another consumer fails before its dependent hooks publish.
bool RegisterBeforePlayEntry(PlayEntryReset reset);
// Timing: notify first, invalidate command/edge state, native Play, cursor tap.
void BeforePlayEntry(void *controller);
}
