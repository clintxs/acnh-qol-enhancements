#pragma once
namespace qol_pockets::features {
bool InstallStorageHint();
// Requires the shared caption resolver installed by InstallStorageHint.
bool InstallStorageWithdrawalHint();
}
