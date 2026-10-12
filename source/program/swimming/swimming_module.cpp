#include "lib.hpp"
#include "settings.hpp"
#include "auto_swim.hpp"
#include "camera_tilt.hpp"
namespace qol::swimming {
namespace { Settings Preferences{}; }
unsigned Configure(std::string_view input) { return ApplySettings(input, Preferences); }
void ReportSettings() {
    Logging.Settings("[QoL Enhancements/Swimming] settings: auto_swim=%s camera_tilt_while_swimming=%s\n",
                     Preferences.autoSwim ? "true" : "false",
                     Preferences.cameraTiltWhileSwimming ? "true" : "false");
}
bool Install() {
    const bool movement = !Preferences.autoSwim || InstallAutoSwim();
    const bool camera = !Preferences.cameraTiltWhileSwimming || InstallSwimmingCameraTilt();
    return movement && camera;
}
}
