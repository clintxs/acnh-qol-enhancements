#pragma once
namespace qol_pockets::features {
bool InstallHoldSelect();
// Registered with the shared lifecycle service; runs before native Play entry.
void ResetHoldSelect(void *owner);
}
