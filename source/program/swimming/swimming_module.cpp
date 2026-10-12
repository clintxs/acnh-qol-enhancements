#include "lib.hpp"
#include "settings.hpp"
#include "auto_swim.hpp"
namespace qol::swimming {
namespace { Settings Preferences{}; }
unsigned Configure(std::string_view input) { return ApplySettings(input, Preferences); }
void ReportSettings() {
    Logging.Settings("[QoL Enhancements/Swimming] settings: auto_swim=%s\n",
                     Preferences.autoSwim ? "true" : "false");
}
bool Install() { return !Preferences.autoSwim || InstallAutoSwim(); }
}
