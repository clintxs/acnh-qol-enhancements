#include "pockets.hpp"
#include "program/pockets/settings.hpp"
#include "program/pockets/game/build_profile.hpp"
#include "program/core/native/runtime.hpp"
#include "stack_splitting.hpp"
#include "quick_store.hpp"
#include "hold_select.hpp"
#include "storage_withdrawal.hpp"
#include "recycling_claim.hpp"
#include "storage_hint.hpp"

namespace qol_pockets::features {
static bool ValidatePockets() {
    const auto result = qol_pockets::game::ValidateExpectedCode(qol::native::MainExecutable());
    if (!result) qol::native::ReportMismatch(result);
    return bool(result);
}
qol::core::InstallResult InstallPockets() {
    using namespace qol_pockets::features;
    const qol::core::FeatureDescription features[]{
        {"pockets.profile", true, ValidatePockets},
        {"pockets.stacks", true, InstallStackSplitting, {"pockets.profile"}},
        {"pockets.storage_withdrawal", true, InstallStorageWithdrawal, {"pockets.profile"}},
        {"pockets.quick_store", true, InstallQuickStore, {"pockets.profile"}},
        {"pockets.hold_select", qol_pockets::GetSettings().enableMultiselect, InstallHoldSelect, {"pockets.quick_store"}},
        {"pockets.recycling", true, InstallRecyclingClaim, {"pockets.profile"}},
        {"pockets.hints", true, InstallStorageHint, {"pockets.quick_store"}},
        {"pockets.withdrawal_hint", true, InstallStorageWithdrawalHint, {"pockets.storage_withdrawal", "pockets.hints"}},
    };
    return qol::core::ActivateFeatures(features);
}
}
