#pragma once
#include "settings_schema.hpp"
#include "logging_policy.hpp"

namespace qol::core {
// Native BC44B0..BC44BC decrements an unsigned halfword, without saturation.
// Zero means leave the native initialization and countdown entirely untouched.
inline constexpr unsigned MaxAmiiboTimeoutFrames = 65535;
struct Settings {
    unsigned amiiboTimeoutFrames = 9000;
    logging::Preferences logging{};
};

inline bool ParseAmiiboTimeout(std::string_view value, unsigned &output) {
    return settings::ParseUnsigned(value, output,
        {0, MaxAmiiboTimeoutFrames, settings::Overflow::Reject,
         settings::LeadingZeros::Allow});
}

// Core already exists in the executable: both files are optional settings
// overlays, not installation markers, and require no enabled/version metadata.
// Match feature preference behavior: bad values and duplicates retain the
// value from before this file. Whole-file encoding/size errors are atomic.
inline constexpr std::array SettingsSchema{
    qol::core::settings::Number<Settings, &Settings::amiiboTimeoutFrames, ParseAmiiboTimeout>("amiibo_timeout_frames"),
    qol::core::settings::Boolean<Settings, &Settings::logging, &logging::Preferences::versions>("log_versions"),
    qol::core::settings::Boolean<Settings, &Settings::logging, &logging::Preferences::settings>("log_settings"),
    qol::core::settings::Boolean<Settings, &Settings::logging, &logging::Preferences::debug>("log_debug")
};
static_assert(qol::core::settings::ValidSchema(SettingsSchema));
inline unsigned ApplyCoreSettings(std::string_view input, Settings &output) {
    return qol::core::settings::Apply(input, output, SettingsSchema, qol::core::settings::Metadata::None);
}

bool InstallAmiiboTimeout(unsigned frames);
} // namespace qol::core
