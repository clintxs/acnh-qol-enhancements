#pragma once

namespace qol::core::game {
// Verified code/profile operations used by Core's startup assembly. These are
// synchronous and retain the existing HookPlan publication/failure semantics.
bool ValidateCatalogProfile();
// Installs the scan-initialization override only; countdown/expiry remain native.
bool InstallAmiiboTimeout(unsigned frames);
}
