#include "lib.hpp"
#include "settings.hpp"
#include "hold_y.hpp"
#include "dialogue_hurry.hpp"
#include "auto_vault.hpp"
#include "auto_swim.hpp"
namespace qol::controls {
namespace { Settings Preferences{}; }
unsigned Configure(std::string_view input) { return ApplySettings(input, Preferences); }
void ReportSettings() {
    Logging.Settings("[QoL Enhancements/Controls] settings: hold_to_hoover=%s hold_to_advance_dialogue=%s auto_vault=%s auto_ladder=%s auto_equip_traversal=%s auto_swim=%s\n",
                     Preferences.holdToHoover ? "true" : "false",
                     Preferences.holdToAdvanceDialogue ? "true" : "false",
                     Preferences.autoVault ? "true" : "false",
                     Preferences.autoLadder ? "true" : "false",
                     Preferences.autoEquipTraversal ? "true" : "false",
                     Preferences.autoSwim ? "true" : "false");
}
bool Install() {
    // A conflict in one feature must not suppress the other independent features.
    const bool pickup = !Preferences.holdToHoover || InstallHoldYPickup();
    const bool dialogue = !Preferences.holdToAdvanceDialogue || InstallHoldToAdvanceDialogue();
    const bool vault = !(Preferences.autoVault || Preferences.autoLadder) ||
        InstallAutoTraversal(Preferences.autoVault, Preferences.autoLadder, Preferences.autoEquipTraversal);
    const bool swim = !Preferences.autoSwim || InstallAutoSwim();
    return pickup && dialogue && vault && swim;
}
}
