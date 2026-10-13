#include "lib.hpp"
// Actual feature decisions start in features/crafting_features.cpp and
// features/preview_features.cpp; this unit only owns settings and activation.
#include "settings.hpp"
#include "recipe_preview.hpp"

namespace qol::crafting {
namespace {
Settings Preferences{};
}
unsigned Configure(std::string_view input) { return ApplySettings(input, Preferences); }

void ReportSettings() {
    Logging.Settings("[QoL Enhancements/Crafting] settings: diy_app_preview=%s crafting_speed=%u; "
                "workbench_preview_tilt=%s, crafting_quantity_wrap=%s\n",
                Preferences.diyAppPreview ? "true" : "false", Preferences.craftingSpeed,
                Preferences.workbenchPreviewTilt ? "true" : "false",
                Preferences.craftingQuantityWrap ? "true" : "false");
}

namespace {
bool InstallConfiguredSpeed() { return InstallFasterCrafting(Preferences.craftingSpeed); }
}
core::InstallResult Install() {
    const core::FeatureDescription features[]{
        {"crafting.speed", Preferences.craftingSpeed != 0, InstallConfiguredSpeed},
        {"crafting.diy_preview", Preferences.diyAppPreview, InstallRecipePreview},
        {"crafting.workbench_tilt", Preferences.workbenchPreviewTilt, InstallWorkbenchPreviewTilt},
        {"crafting.quantity_wrap", Preferences.craftingQuantityWrap, InstallCraftingQuantityWrap},
        {"crafting.quantity_shortcut", true, InstallCraftingQuantityShortcut},
    };
    return core::ActivateFeatures(features);
}
} // namespace qol::crafting
