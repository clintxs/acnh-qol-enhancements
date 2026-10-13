#pragma once
#include "program/core/feature_activation.hpp"
#include "program/core/settings_schema.hpp"

namespace qol::crafting {
inline constexpr unsigned MaximumCraftingSpeed = 30;

inline bool ParseCraftingSpeed(std::string_view value, unsigned &output) {
    return core::settings::ParseUnsigned(value, output,
        {0, MaximumCraftingSpeed, core::settings::Overflow::Clamp,
         core::settings::LeadingZeros::Reject});
}

struct Settings {
    bool diyAppPreview = true;
    bool workbenchPreviewTilt = true;
    bool craftingQuantityWrap = true;
    unsigned craftingSpeed = 6;
};

// Same layering rules as other modules: omitted/invalid values inherit, and
// duplicate keys restore the value from before this file. Encoding/size failures
// reject the whole overlay. Core validates enabled/version before calling this.
inline constexpr std::array SettingsSchema{
    qol::core::settings::Boolean<Settings, &Settings::diyAppPreview>("diy_app_preview"),
    qol::core::settings::Boolean<Settings, &Settings::workbenchPreviewTilt>("workbench_preview_tilt"),
    qol::core::settings::Boolean<Settings, &Settings::craftingQuantityWrap>("crafting_quantity_wrap"),
    qol::core::settings::Number<Settings, &Settings::craftingSpeed, ParseCraftingSpeed>("crafting_speed")
};
static_assert(qol::core::settings::ValidSchema(SettingsSchema));
inline unsigned ApplySettings(std::string_view input, Settings &output) {
    return qol::core::settings::Apply(input, output, SettingsSchema);
}

unsigned Configure(std::string_view input);
void ReportSettings();
core::InstallResult Install();
bool InstallFasterCrafting(unsigned maximumSpeed);
bool InstallCraftingQuantityWrap();
bool InstallCraftingQuantityShortcut();
} // namespace qol::crafting
