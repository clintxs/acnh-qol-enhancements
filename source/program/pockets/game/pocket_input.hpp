#pragma once

namespace qol_pockets::game::pocket_input {
// Resolve native bindings/focus; the transient click slot is not cursor focus.
void *FindPocketController(void *page);
int FindFocusedItemSlot(void *page, int row);
}
