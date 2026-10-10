#pragma once

namespace qol::logging {
enum class Category { Version, Settings, Debug, Warning, Error };
struct Preferences {
    bool versions = true;
    bool settings = true;
    bool debug = false;
};
constexpr bool Enabled(const Preferences &preferences, Category category) {
    switch (category) {
        case Category::Version: return preferences.versions;
        case Category::Settings: return preferences.settings;
        case Category::Debug: return preferences.debug;
        case Category::Warning:
        case Category::Error: return true;
    }
    return true;
}
} // namespace qol::logging
