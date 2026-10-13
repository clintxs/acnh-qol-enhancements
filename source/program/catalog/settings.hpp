#pragma once
#include "program/core/settings_schema.hpp"
namespace catalog_aware_shops {
struct Settings { bool artDonationStars = true; };
inline constexpr std::array SettingsSchema{
    qol::core::settings::Boolean<Settings, &Settings::artDonationStars>("art_donation_stars")
};
static_assert(qol::core::settings::ValidSchema(SettingsSchema));
inline unsigned ApplySettings(std::string_view input, Settings &output) {
    return qol::core::settings::Apply(input, output, SettingsSchema);
}
unsigned Configure(std::string_view input);
void ReportSettings();
const Settings &GetSettings();
}
