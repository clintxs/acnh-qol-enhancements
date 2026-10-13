#pragma once
#include "program/core/feature_activation.hpp"
#include "program/core/settings_schema.hpp"
namespace qol::controls {
struct Settings {
    bool autoSwim = true;
    bool swimStrokeBoost = true;
    bool holdToHoover = true;
    bool holdToAdvanceDialogue = true;
    bool autoVault = true;
    bool autoEquipTraversal = true; // Borrow a pocket tool and restore the prior held item.
    bool autoLadder = true; // Handheld and placed ladders, including vines.
};
inline constexpr std::array SettingsSchema{
    qol::core::settings::Boolean<Settings, &Settings::autoSwim>("auto_swim"),
    qol::core::settings::Boolean<Settings, &Settings::swimStrokeBoost>("swim_stroke_boost"),
    qol::core::settings::Boolean<Settings, &Settings::holdToHoover>("hold_to_hoover"),
    qol::core::settings::Boolean<Settings, &Settings::holdToAdvanceDialogue>("hold_to_advance_dialogue"),
    qol::core::settings::Boolean<Settings, &Settings::autoVault>("auto_vault"),
    qol::core::settings::Boolean<Settings, &Settings::autoEquipTraversal>("auto_equip_traversal"),
    qol::core::settings::Boolean<Settings, &Settings::autoLadder>("auto_ladder")
};
static_assert(qol::core::settings::ValidSchema(SettingsSchema));
inline unsigned ApplySettings(std::string_view input, Settings &output) {
    return qol::core::settings::Apply(input, output, SettingsSchema);
}
unsigned Configure(std::string_view input);
void ReportSettings();
core::InstallResult Install();
}
