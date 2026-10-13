#include "lib.hpp"
// Player-facing policy starts at features/controls.hpp. Native hooks call those
// handlers directly; no dynamic binding or borrowed declaration storage.
#include "settings.hpp"
#include "hold_y.hpp"
#include "dialogue_hurry.hpp"
#include "auto_vault.hpp"
#include "auto_swim.hpp"
#include "camera_tilt.hpp"
namespace qol::controls {
namespace { Settings Preferences{}; }
unsigned Configure(std::string_view input) { return ApplySettings(input, Preferences); }
void ReportSettings() {
    Logging.Settings("[QoL Enhancements/Controls] settings: hold_to_hoover=%s hold_to_advance_dialogue=%s auto_vault=%s auto_ladder=%s auto_equip_traversal=%s auto_swim=%s swim_stroke_boost=%s\n",
                     Preferences.holdToHoover ? "true" : "false",
                     Preferences.holdToAdvanceDialogue ? "true" : "false",
                     Preferences.autoVault ? "true" : "false",
                     Preferences.autoLadder ? "true" : "false",
                     Preferences.autoEquipTraversal ? "true" : "false",
                     Preferences.autoSwim ? "true" : "false",
                     Preferences.swimStrokeBoost ? "true" : "false");
}
namespace {
bool InstallConfiguredTraversal() { return InstallAutoTraversal(Preferences.autoVault, Preferences.autoLadder, Preferences.autoEquipTraversal); }
bool InstallConfiguredSwim() { return InstallAutoSwim(Preferences.swimStrokeBoost); }
}
core::InstallResult Install() {
    const core::FeatureDescription features[]{
        {"controls.pickup", Preferences.holdToHoover, InstallHoldYPickup},
        {"controls.dialogue_hurry", Preferences.holdToAdvanceDialogue, InstallHoldToAdvanceDialogue},
        {"controls.traversal", Preferences.autoVault || Preferences.autoLadder, InstallConfiguredTraversal},
        {"controls.swimming", Preferences.autoSwim, InstallConfiguredSwim},
        {"controls.swimming_camera", true, InstallSwimmingCameraTilt},
    };
    return core::ActivateFeatures(features);
}
}
