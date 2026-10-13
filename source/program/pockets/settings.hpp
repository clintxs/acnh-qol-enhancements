#pragma once
#include "program/core/settings_schema.hpp"

namespace qol_pockets {
struct Settings {
    bool bypassMenuOpenContainers = true;
    bool bypassMenuClaimRecycleBox = true;
    bool bypassMenuWithdrawHomeStorage = true;
    bool enableMultiselect = true;
};
// Omitted/invalid keys inherit prior values. Duplicates restore the value
// from before this file, matching the other modules' custom-file policy.
inline constexpr std::array SettingsSchema{
    qol::core::settings::Boolean<Settings, &Settings::bypassMenuOpenContainers>("bypass_menu_open_containers"),
    qol::core::settings::Boolean<Settings, &Settings::bypassMenuClaimRecycleBox>("bypass_menu_claim_recycle_box"),
    qol::core::settings::Boolean<Settings, &Settings::bypassMenuWithdrawHomeStorage>("bypass_menu_withdraw_home_storage"),
    qol::core::settings::Boolean<Settings, &Settings::enableMultiselect>("enable_multiselect")
};
static_assert(qol::core::settings::ValidSchema(SettingsSchema));
inline unsigned ApplySettings(std::string_view input, Settings &output) {
    return qol::core::settings::Apply(input, output, SettingsSchema);
}
unsigned Configure(std::string_view input);
void ReportSettings();
const Settings &GetSettings();
}
