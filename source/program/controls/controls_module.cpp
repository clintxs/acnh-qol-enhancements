#include "lib.hpp"
#include "settings.hpp"
#include "hold_y.hpp"
#include "dialogue_hurry.hpp"
#include "auto_vault.hpp"
namespace qol::controls {
namespace { Settings Preferences{}; }
unsigned Configure(std::string_view input) { return ApplySettings(input, Preferences); }
void ReportSettings() {
    Logging.Settings("[QoL Enhancements/Controls] settings: hold_to_hoover=%s hold_to_advance_dialogue=%s auto_vault=%s auto_ladder=%s\n",
                     Preferences.holdToHoover ? "true" : "false",
                     Preferences.holdToAdvanceDialogue ? "true" : "false",
                     Preferences.autoVault ? "true" : "false",
                     Preferences.autoLadder ? "true" : "false");
}
bool Install() {
    // A conflict in either feature must not suppress the independent feature.
    const bool pickup = !Preferences.holdToHoover || InstallHoldYPickup();
    const bool dialogue = !Preferences.holdToAdvanceDialogue || InstallHoldToAdvanceDialogue();
    const bool vault = !(Preferences.autoVault || Preferences.autoLadder) ||
        InstallAutoTraversal(Preferences.autoVault, Preferences.autoLadder);
    return pickup && dialogue && vault;
}
}
