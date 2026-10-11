#pragma once
namespace qol_pockets::features {
bool InstallHoldSelect();
// Called by the existing pockets Play-entry hook; a reopened screen starts fresh.
void ResetHoldSelect(void *owner);
}
