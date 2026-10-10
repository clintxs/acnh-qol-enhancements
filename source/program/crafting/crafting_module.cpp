#include "lib.hpp"
#include "settings.hpp"
#include "recipe_preview.hpp"

namespace qol::crafting {
namespace {
Settings Preferences{};
}
unsigned Configure(std::string_view input) { return ApplySettings(input, Preferences); }

bool Install() {
    Logging.Log("[QoL Enhancements/Crafting] settings: diy_app_preview=%s crafting_speed=%u; "
                "workbench_preview_tilt=%s, crafting_quantity_wrap=%s\n",
                Preferences.diyAppPreview ? "true" : "false", Preferences.craftingSpeed,
                Preferences.workbenchPreviewTilt ? "true" : "false",
                Preferences.craftingQuantityWrap ? "true" : "false");
    // Install independently so a preview conflict cannot suppress speed support,
    // or a speed-hook conflict suppress the already-working preview feature.
    const bool speed = InstallFasterCrafting(Preferences.craftingSpeed);
    const bool preview = !Preferences.diyAppPreview || InstallRecipePreview();
    const bool tilt = !Preferences.workbenchPreviewTilt || InstallWorkbenchPreviewTilt();
    const bool quantity = !Preferences.craftingQuantityWrap || InstallCraftingQuantityWrap();
    return speed && preview && tilt && quantity;
}
} // namespace qol::crafting
