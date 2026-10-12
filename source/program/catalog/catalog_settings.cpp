#include "settings.hpp"
#include "lib.hpp"
namespace catalog_aware_shops {
namespace { Settings Preferences{}; }
unsigned Configure(std::string_view input) { return ApplySettings(input, Preferences); }
const Settings &GetSettings() { return Preferences; }
void ReportSettings() {
    Logging.Settings("[QoL Enhancements/Catalog] settings: art_donation_stars=%s\n",
                     Preferences.artDonationStars ? "true" : "false");
}
}
